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

#define DFLT_PORT       21772
#define LOOPBACK        0x7F000001UL
#define REPLY_WAIT      5               /* seconds a reply may take     */

static void
nap(unsigned tenths)
{
    ECB ecb = 0;

    ecb_timed_wait(&ecb, tenths * 10, 0);
}

/* A connected pair: *cli is the client end, *srv the accepted end. */
static int
connected_pair(int lsn, unsigned short port, int *cli, int *srv)
{
    struct sockaddr_in sa;
    struct sockaddr_in pa;
    int                len = sizeof(pa);

    *cli = socket(AF_INET, SOCK_STREAM, 0);
    if (*cli < 0)
        return -1;

    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons(port);
    sa.sin_addr.s_addr = htonl(LOOPBACK);
    if (connect(*cli, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
        closesocket(*cli);
        return -1;
    }

    *srv = accept(lsn, (struct sockaddr *)&pa, &len);
    if (*srv < 0) {
        closesocket(*cli);
        return -1;
    }
    return 0;
}

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
    struct sockaddr_in sa;
    int                lsn, cli, srv;
    int                n, i;
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

    lsn = socket(AF_INET, SOCK_STREAM, 0);
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons(port);
    sa.sin_addr.s_addr = htonl(LOOPBACK);
    if (lsn < 0 || bind(lsn, (struct sockaddr *)&sa, sizeof(sa)) < 0 ||
        listen(lsn, 5) < 0) {
        printf("FAIL: cannot listen on the loopback port, errno=%d\n", errno);
        return 8;
    }

    CHECK_EQ(__isauth(), 0,
             "precondition: the test task is not APF-authorized");

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
    nap(5);
    cthread_manager_term(&mgr);
    closesocket(lsn);
    free(server);

    return mbt_test_summary("TSTPAS");
}
