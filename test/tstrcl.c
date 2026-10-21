/* TSTRCL.C
** Test for ftpd_mvs_retr() -- #233: RETR must keep sess->cur_file set until
** fclose() has returned.
**
** ABEND recovery reaches the FILE, and with it fopen's DD, only through
** sess->cur_file.  STOR kept it set across the CLOSE since #223; RETR cleared
** it first, so a CLOSE that ABENDed left the DD allocated to the address
** space for good.  A real CLOSE of an input DCB cannot be made to ABEND on
** demand (a damaged DEB is refused with IEC223I, not abended), so this test
** compiles ftpd#mvs.c into this program with fclose() replaced by one that
** records what recovery would see and, when armed, ABENDs instead of closing.
**
** Three rounds, each against the real RETR code and a real data set:
**   1  normal transfer: cur_file is set when fclose() runs, cleared after
**   2  data connection refused (425 path): same
**   3  fclose() ABENDs: cur_file still names the FILE, and abandoning it as
**      recovery does releases the DD
**
** MVS only: fopen of a data set, SVC 99, and try().
*/
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "ftpd.h"
#include "ftpd#ses.h"
#include "ftpd#mvs.h"
#include "ftpd#dat.h"
#include "ftpd#aut.h"
#include <mvs/dynalloc.h>
#include <mvs/file.h>
#include <mvs/recovery.h>
#include <mbtcheck.h>

#define DSN_ARG     "'SYS1.MACLIB(ABEND)'"
#define DD_UNKNOWN  4           /* __dsfree(): no such DD in this TIOT */

static ftpd_server_t    server;
static ftpd_session_t   sess;

static int      data_open_rc;
static int      arm_abend;      /* fclose() ABENDs instead of closing      */
static int      close_calls;
static FILE    *close_fp;
static FILE    *close_cur;      /* sess.cur_file as fclose() saw it        */
static char     close_dd[9];
static int      last_code;

int
ftpd_session_reply(ftpd_session_t *s, int code, const char *fmt, ...)
{
    (void)s;
    (void)fmt;
    last_code = code;
    return 0;
}

int ftpd_data_open(ftpd_session_t *s)  { (void)s; return data_open_rc; }
int ftpd_data_send(ftpd_session_t *s, const void *b, int n)
{
    (void)s; (void)b; (void)n;
    return 0;
}
void ftpd_data_close(ftpd_session_t *s) { (void)s; }
int ftpd_data_recv(ftpd_session_t *s, void *b, int n)
{
    (void)s; (void)b; (void)n;
    return 0;
}
int ftpd_data_printf(ftpd_session_t *s, const char *fmt, ...)
{
    (void)s; (void)fmt;
    return 0;
}
int ftpd_racf_allowed(int rc) { (void)rc; return 1; }
void ftpd_acee_enter(ftpd_session_t *s) { (void)s; }
void ftpd_acee_leave(ftpd_session_t *s) { (void)s; }

/* The test's fclose(): record, then ABEND or close.  Closing goes through
** __fabandon(); for a read-only FILE it is the same CLOSE and the same
** unallocation as fclose()'s. */
static int
tstrcl_fclose(FILE *fp)
{
    volatile int *trap = (volatile int *)0;

    close_calls++;
    close_fp = fp;
    close_cur = sess.cur_file;
    strncpy(close_dd, fp->ddname, sizeof(close_dd) - 1);
    close_dd[sizeof(close_dd) - 1] = '\0';

    if (arm_abend)
        *trap = 0;                      /* S0C4 inside the CLOSE */

    return __fabandon(fp) == 0 ? 0 : EOF;
}

/* ftpd#mvs.c is part of this translation unit so that only its fclose()
** calls are redirected: libc's own, at exit, must stay the real one. */
#define fclose tstrcl_fclose
#include "../src/ftpd#mvs.c"
#undef fclose

static void
fresh(int open_rc, int abend)
{
    memset(&server, 0, sizeof(server));
    memset(&sess, 0, sizeof(sess));
    sess.server = &server;
    sess.fsmode = FS_MVS;
    sess.type = XFER_TYPE_I;
    strcpy(sess.user, "IBMUSER");
    data_open_rc = open_rc;
    arm_abend = abend;
    close_calls = 0;
    close_fp = NULL;
    close_cur = NULL;
    close_dd[0] = '\0';
    last_code = 0;
}

int
main(void)
{
    int rc;

    printf("=== FTPD RETR close order tests (#233) ===\n");

    fresh(0, 0);
    ftpd_mvs_retr(&sess, DSN_ARG);
    CHECK(close_calls == 1, "round 1: fclose() ran once");
    CHECK(close_fp != NULL && close_cur == close_fp,
          "round 1: cur_file still names the FILE while fclose() runs");
    CHECK(sess.cur_file == NULL, "round 1: cur_file is clear afterwards");
    CHECK(last_code == FTP_250, "round 1: transfer completed (250)");

    fresh(-1, 0);
    ftpd_mvs_retr(&sess, DSN_ARG);
    CHECK(close_calls == 1, "round 2: fclose() ran once");
    CHECK(close_fp != NULL && close_cur == close_fp,
          "round 2: cur_file still names the FILE while fclose() runs");
    CHECK(sess.cur_file == NULL, "round 2: cur_file is clear afterwards");
    CHECK(last_code == FTP_425, "round 2: 425 Cannot open data connection");

    fresh(0, 1);
    rc = try(ftpd_mvs_retr, &sess, DSN_ARG);
    CHECK(rc != 0, "round 3: the ABEND in fclose() reached try()");
    CHECK(close_calls == 1, "round 3: fclose() was entered");
    CHECK(sess.cur_file != NULL && sess.cur_file == close_fp,
          "round 3: recovery can still reach the FILE through cur_file");

    /* What ftpd_session_recover() does with it. */
    arm_abend = 0;
    CHECK(sess.cur_file != NULL, "round 3: recovery has a FILE to abandon");
    if (sess.cur_file) {
        CHECK(__fabandon(sess.cur_file) == 0,
              "round 3: abandoning the FILE closes it");
        sess.cur_file = NULL;
        CHECK(__dsfree(close_dd) == DD_UNKNOWN,
              "round 3: the DD is released after recovery");
    } else if (close_fp) {
        /* Recovery could not reach it: release it by hand so the test
        ** does not leave the DD behind in the batch job. */
        __fabandon(close_fp);
    }

    return mbt_test_summary("TSTRCL");
}
