/*
** TSTPAS -- ftpd#258: PASS on a server that is not program-authorized must be
** answered, and the session must stay usable.
**
** FTPD started without APF authorization (FTPD003W) still accepts
** connections.  racf_login() needs supervisor state; unauthorized it does
** not return an error, it ABENDs S047.  The ABEND is caught and
** ftpd_session_recover() starts, and its first step, racf_set_acee(), is the
** same MODESET -- a second S047, swallowed by the try() around the recovery,
** so the client never got a reply to PASS.
**
** Two rounds:
**
**   1  the server knows it is not authorized (server->authorized == 0, as
**      main() sets it): PASS is refused with 530 before racf_login(), the
**      failed-login counter does not move, the session goes on.
**   2  the server believes it is authorized, which the test task is not:
**      racf_login() ABENDs S047 and so does the identity reset in
**      ftpd_session_recover().  The recovery must still tell the client
**      (451) and go on with the next command.  This is the "recovery dies
**      silently" case on its own, without the check of round 1.
**
** The test runs the real session loop (ftpd_session_run) in a real worker of
** a real thread manager and talks to it over a loopback connection, the way
** a client does: the banner, USER, PASS, NOOP, QUIT.  The test task itself
** is not APF-authorized (it is loaded from a plain test library), which is
** what makes racf_login() ABEND.
**
** MVS-only: thread manager, SVC 75 sockets, MODESET.
** PARM='<port>' is the loopback listen port (default 21772).
**
** RC 0 = all checks passed.
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <mvs/apf.h>
#include <mvs/socket.h>
#include <mvs/thread.h>
#include <mvs/ecb.h>
#include <mbtcheck.h>

#include "ftpd.h"
#include "ftpd#log.h"
#include "ftpd#ses.h"
#include "ftpd#xlt.h"
#include "tstloop.h"

#define DFLT_PORT       21772
#define REPLY_WAIT      5               /* seconds a reply may take     */

/* Read one reply line (up to LF) from the control connection and leave it
** in EBCDIC in buf.  Returns the length, 0 when the server said nothing
** within REPLY_WAIT seconds, -1 when it closed the connection first. */
static int
reply(int fd, char *buf, int max)
{
    int n = 0;

    for (;;) {
        fd_set         rfds;
        struct timeval tv;
        int            rc;

        FD_ZERO(&rfds);
        FD_SET(fd, &rfds);
        tv.tv_sec = REPLY_WAIT;
        tv.tv_usec = 0;
        rc = select(fd + 1, &rfds, NULL, NULL, &tv);
        if (rc == 0)
            return 0;
        if (rc < 0)
            return -1;

        rc = recv(fd, buf + n, 1, 0);
        if (rc <= 0)
            return -1;
        n += rc;
        buf[n] = '\0';
        if ((unsigned char)buf[n - 1] == ASCII_LF || n >= max - 1) {
            ftpd_xlat_a2e((unsigned char *)buf, n);
            return n;
        }
    }
}

/* Send one command line, given in EBCDIC. */
static int
command(int fd, const char *line)
{
    char buf[80];
    int  n = (int)strlen(line);

    memcpy(buf, line, n);
    ftpd_xlat_e2a((unsigned char *)buf, n);
    memcpy(buf + n, ASCII_CRLF, 2);
    return send(fd, buf, n + 2, 0) == n + 2 ? 0 : -1;
}

/* True when the reply line starts with the three digits in code. */
static int
is_code(const char *buf, int n, const char *code)
{
    return n >= 3 && strncmp(buf, code, 3) == 0;
}

int
main(int argc, char **argv)
{
    unsigned short     port = DFLT_PORT;
    ftpd_server_t     *server;
    CTHDMGR           *mgr;
    int                lsn;
    int                cli;
    int                srv;
    int                n;
    int                i;
    char               buf[200];

    if (argc > 1 && argv[1] && argv[1][0])
        port = (unsigned short)atoi(argv[1]);

    printf("TSTPAS -- ftpd#258: PASS without APF authorization is answered "
           "(port %d)\n\n", port);

    server = calloc(1, sizeof(*server));
    if (!server) {
        printf("FAIL: no storage for the server block\n");
        return 8;
    }
    strcpy(server->eye, FTPD_EYE);
    server->flags |= FTPD_ACTIVE;
    server->config.idle_timeout = 60;

    lsn = loop_listen(port);
    if (lsn < 0) {
        printf("FAIL: cannot listen on the loopback port, errno=%d\n", errno);
        return 8;
    }

    CHECK_EQ(__isauth(), 0,
             "precondition: the test task is not APF-authorized");
    server->authorized = 0;

    mgr = cthread_manager_init(1, ftpd_session_run, server, 64 * 1024);
    if (!mgr) {
        printf("FAIL: cthread_manager_init\n");
        return 8;
    }
    server->mgr = mgr;
    mgr->mintask = 1;
    cthread_post(&mgr->wait, CTHDMGR_POST_DATA);

    if (connected_pair(lsn, port, &cli, &srv) != 0) {
        printf("FAIL: loopback connect/accept\n");
        return 8;
    }
    if (ftpd_session_queue(server, ftpd_session_new(server, srv)) != 0) {
        printf("FAIL: the session was not queued\n");
        return 8;
    }

    n = reply(cli, buf, sizeof(buf));
    CHECK(is_code(buf, n, "220"), "the banner is sent");

    CHECK_EQ(command(cli, "USER IBMUSER"), 0, "USER sent");
    n = reply(cli, buf, sizeof(buf));
    CHECK(is_code(buf, n, "331"), "USER is answered 331");

    CHECK_EQ(command(cli, "PASS SECRET"), 0, "PASS sent");
    n = reply(cli, buf, sizeof(buf));
    CHECK(is_code(buf, n, "530"),
          "PASS is answered 530 within the time a reply may take");
    CHECK(n > 3 && strstr(buf, "APF") != NULL,
          "... and the text names the cause");

    /* Three more in a row: nothing counts them as failed logins or as
    ** consecutive ABENDs, so the session is still there afterwards. */
    for (i = 0; i < 3; i++) {
        command(cli, "USER IBMUSER");
        n = reply(cli, buf, sizeof(buf));
        command(cli, "PASS SECRET");
        n = reply(cli, buf, sizeof(buf));
        CHECK(is_code(buf, n, "530"), "repeated PASS is answered 530");
    }

    CHECK_EQ(command(cli, "NOOP"), 0, "NOOP sent");
    n = reply(cli, buf, sizeof(buf));
    CHECK(is_code(buf, n, "200"), "the session is still usable: NOOP is 200");

    CHECK_EQ(command(cli, "QUIT"), 0, "QUIT sent");
    n = reply(cli, buf, sizeof(buf));
    CHECK(is_code(buf, n, "221"), "QUIT is answered 221");

    closesocket(cli);

    /* ---- round 2: the recovery ABENDs again ------------------------- */

    server->authorized = 1;
    if (connected_pair(lsn, port, &cli, &srv) != 0 ||
        ftpd_session_queue(server, ftpd_session_new(server, srv)) != 0) {
        printf("FAIL: round 2: no second session\n");
        return 8;
    }

    n = reply(cli, buf, sizeof(buf));
    CHECK(is_code(buf, n, "220"), "round 2: the banner is sent");

    command(cli, "USER IBMUSER");
    n = reply(cli, buf, sizeof(buf));
    CHECK(is_code(buf, n, "331"), "round 2: USER is answered 331");

    for (i = 0; i < 2; i++) {
        command(cli, "PASS SECRET");
        n = reply(cli, buf, sizeof(buf));
        CHECK(is_code(buf, n, "451"),
              "round 2: PASS ABENDs twice and is still answered (451)");
        command(cli, "USER IBMUSER");
        n = reply(cli, buf, sizeof(buf));
    }

    command(cli, "NOOP");
    n = reply(cli, buf, sizeof(buf));
    CHECK(is_code(buf, n, "200"), "round 2: the session is still usable");

    command(cli, "QUIT");
    n = reply(cli, buf, sizeof(buf));
    CHECK(is_code(buf, n, "221"), "round 2: QUIT is answered 221");

    closesocket(cli);
    nap(5);
    cthread_manager_term(&mgr);
    closesocket(lsn);
    free(server);

    return mbt_test_summary("TSTPAS");
}
