/*
** TSTDCB -- ftpd#180: SITE DCBDSN reads RECFM, LRECL and BLKSIZE of a model
** data set from its Format-1 DSCB.
**
** tstsit.c proves the SITE side with a stand-in for the lookup.  This is the
** lookup itself, against real catalog entries and DSCBs: four scratch data
** sets are allocated here (FB, VBA, U, and a PDS) and read back through
** ftpd_mvs_dcbmodel(), then a name that is not cataloged.
**
** The session has no ACEE, so the READ check against RAKF is skipped here:
** that part needs a RAKF profile the test cannot create.
**
** MVS-only: catalog and VTOC.  PARM='<prefix>' is the high-level part of the
** scratch names (default IBMUSER.FTPD.TDCB); each gets one more qualifier.
**
** RC 0 = all checks passed.
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mvs/dynalloc.h>
#include <mbtcheck.h>

#include "ftpd.h"
#include "ftpd#ses.h"
#include "ftpd#mvs.h"

#define DFLT_PREFIX "IBMUSER.FTPD.TDCB"

static ftpd_server_t    server;
static ftpd_session_t   sess;

static int
make(const char *prefix, const char *suffix, const char *attrs)
{
    char dd[9];
    int rc;

    rc = __dsalcf(dd, "DSN=%s.%s;DISP=(NEW,CATLG,DELETE);%s;SPACE=TRK(1,1)",
                  prefix, suffix, attrs);
    if (rc != 0) {
        printf("  allocating %s.%s failed rc=%d\n", prefix, suffix, rc);
        return -1;
    }
    __dsfree(dd);
    return 0;
}

static void
drop(const char *prefix, const char *suffix)
{
    char dd[9];

    if (__dsalcf(dd, "DSN=%s.%s;DISP=(OLD,DELETE)", prefix, suffix) == 0)
        __dsfree(dd);
}

static void
model(const char *prefix, const char *suffix, const char *recfm,
      int lrecl, int blksize)
{
    ftpd_dcbmodel_t m;
    char dsn[48];
    char err[128];
    char msg[160];
    int rc;

    snprintf(dsn, sizeof(dsn), "%s.%s", prefix, suffix);
    err[0] = '\0';
    rc = ftpd_mvs_dcbmodel(&sess, dsn, &m, err, sizeof(err));
    snprintf(msg, sizeof(msg), "%s reads back as %s %d %d (got rc=%d %s %d %d)",
             suffix, recfm, lrecl, blksize, rc, m.recfm, m.lrecl, m.blksize);
    CHECK(rc == 0 && strcmp(m.recfm, recfm) == 0 && m.lrecl == lrecl
          && m.blksize == blksize && m.note[0] == '\0', msg);
}

int
main(int argc, char **argv)
{
    const char *prefix = (argc > 1 && argv[1][0]) ? argv[1] : DFLT_PREFIX;
    ftpd_dcbmodel_t m;
    char err[128];
    char dsn[48];
    int rc;

    printf("=== FTPD SITE DCBDSN model lookup (#180) ===\n");

    memset(&server, 0, sizeof(server));
    memset(&sess, 0, sizeof(sess));
    sess.server = &server;
    strcpy(sess.user, "IBMUSER");

    drop(prefix, "FB");
    drop(prefix, "VBA");
    drop(prefix, "U");
    drop(prefix, "PDS");

    if (make(prefix, "FB", "DSORG=PS;RECFM=FB;LRECL=80;BLKSIZE=3120") != 0
        || make(prefix, "VBA", "DSORG=PS;RECFM=VBA;LRECL=137;BLKSIZE=6233") != 0
        || make(prefix, "U", "DSORG=PS;RECFM=U;LRECL=0;BLKSIZE=6144") != 0
        || make(prefix, "PDS", "DSORG=PO;RECFM=FB;LRECL=100;BLKSIZE=3200;"
                               "DIR=5") != 0) {
        CHECK(0, "the scratch data sets could be allocated");
        return mbt_test_summary("TSTDCB");
    }

    model(prefix, "FB", "FB", 80, 3120);
    model(prefix, "VBA", "VBA", 137, 6233);
    model(prefix, "U", "U", 0, 6144);
    model(prefix, "PDS", "FB", 100, 3200);

    snprintf(dsn, sizeof(dsn), "%s.NOSUCH", prefix);
    err[0] = '\0';
    rc = ftpd_mvs_dcbmodel(&sess, dsn, &m, err, sizeof(err));
    CHECK(rc != 0 && strstr(err, "is not cataloged") != NULL
          && strstr(err, dsn) != NULL,
          "a name that is not cataloged is refused, naming the data set");

    drop(prefix, "FB");
    drop(prefix, "VBA");
    drop(prefix, "U");
    drop(prefix, "PDS");

    return mbt_test_summary("TSTDCB");
}
