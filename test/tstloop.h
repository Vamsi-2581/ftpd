/*
** tstloop.h -- loopback helpers shared by the MVS tests that run a session
** against a real client socket (TSTQUE, TSTPAS).
**
** Included, not linked: every function is static, so a test uses all of them
** or the build stops on -Wunused-function.  MVS-only (SVC 75 sockets, ECB).
*/
#ifndef TSTLOOP_H
#define TSTLOOP_H

#include <string.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <mvs/socket.h>
#include <mvs/ecb.h>

#define LOOPBACK        0x7F000001UL

static void
nap(unsigned tenths)
{
    ECB ecb = 0;

    ecb_timed_wait(&ecb, tenths * 10, 0);
}

/* A listening socket on the loopback port, or -1. */
static int
loop_listen(unsigned short port)
{
    struct sockaddr_in sa;
    int                lsn = socket(AF_INET, SOCK_STREAM, 0);

    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons(port);
    sa.sin_addr.s_addr = htonl(LOOPBACK);
    if (lsn < 0 || bind(lsn, (struct sockaddr *)&sa, sizeof(sa)) < 0 ||
        listen(lsn, 5) < 0)
        return -1;
    return lsn;
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

#endif /* TSTLOOP_H */
