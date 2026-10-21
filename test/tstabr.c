/* TSTABR.C
** The case behind FTPD076W (issue #232): a STOR into a PDS whose directory is
** full ends in an SB14 in the CLOSE, recovery abandons the FILE, and the
** warning must say what became of the DD.
**
** The sequence is the one ftpd_session_recover() runs: the CLOSE ABENDs under
** try(), then __fabandon() is called on the FILE that is left.  The first time
** a process does this, the CLOSE inside __fabandon() has abended SA0A and
** returned the ABEND code although its unallocation freed the DD all the same
** (libc370#705).  Whatever __fabandon() returns, the TIOT has to agree with
** the text FTPD logs: a DD that is gone is never reported as held.
**
** The directory is filled by writing members until one CLOSE ABENDs.  The
** data set is created here and deleted at the end.
**
** MVS-only: a real PDS, SVC 99, the TIOT and try().
**
** PARM='<dsn>' names the scratch PDS (default IBMUSER.FTPD.TABR).
** RC 0 = all checks passed.
*/
#include <stdio.h>
#include <string.h>

#include <mvs/dd.h>
#include <mvs/dynalloc.h>
#include <mvs/file.h>
#include <mvs/recovery.h>
#include <mbtcheck.h>

#include "ftpd#abd.h"

#define DFLT_DSN    "IBMUSER.FTPD.TABR"
#define MAX_MEMBERS 200             /* a one block directory ends far sooner */

static const char *dsn = DFLT_DSN;
static FILE       *cur;             /* what recovery would find in cur_file */
static char        curdd[9];

/* One STOR: open the member, write a record, close.  Leaves cur set while
** the CLOSE runs, as ftpd_mvs_stor() leaves sess->cur_file. */
static int
store(int i)
{
    char name[80];

    snprintf(name, sizeof(name), "'%s(M%05d)'", dsn, i);
    cur = fopen(name, "w");
    if (!cur)
        return -1;
    strcpy(curdd, cur->ddname);
    fputs("A RECORD\n", cur);
    if (fclose(cur) != 0)
        return -2;
    cur = NULL;
    return 0;
}

/* Provoke one SB14: members until a CLOSE ABENDs.  Returns the ABEND code,
** or 0 when the directory never filled. */
static int
fill(int *next)
{
    int i;
    int rc;

    for (i = *next; i < MAX_MEMBERS; i++) {
        rc = try(store, i);
        if (rc != 0 && cur != NULL) {
            *next = i + 1;
            return rc;
        }
        if (rc != 0 || cur != NULL)
            return -1;
    }
    return 0;
}

/* What recovery does with the FILE: abandon it, then ask the TIOT. */
static void
recover(const char *what, int *arc_out, int *state_out)
{
    FILE *fp = cur;
    char  dd[9];

    cur = NULL;
    strcpy(dd, curdd);
    *arc_out = __fabandon(fp);
    *state_out = ftpd_abd_dd_state(__tiot(), dd);

    printf("  %s: ABEND rc recovered, __fabandon() returned %d (X'%06X'), "
           "DD %s %s\n", what, *arc_out, (unsigned)*arc_out, dd,
           *state_out == FTPD_ABD_HELD ? "HELD" : "gone");
    if (*arc_out > 0)
        printf("  FTPD076W would say: %s\n",
               ftpd_abd_close_text(*state_out));
}

int
main(int argc, char **argv)
{
    char dd[9];
    int  next = 0;
    int  abcode;
    int  arc;
    int  state;
    int  rc;

    if (argc > 1 && argv[1][0] != '\0')
        dsn = argv[1];

    rc = __dsalcf(dd, "DDNAME=TSTABRP;DSN=%s;DISP=(NEW,CATLG,DELETE);"
                      "DSORG=PO;RECFM=FB;LRECL=80;BLKSIZE=3120;"
                      "SPACE=TRK(2,1,1)", dsn);
    CHECK(rc == 0, "the scratch PDS is allocated");
    if (rc != 0)
        return mbt_test_summary("TSTABR");
    __dsfree(dd);

    abcode = fill(&next);
    CHECK(abcode > 0, "a full directory ends a CLOSE in an ABEND");
    if (abcode > 0) {
        printf("  first ABEND: X'%06X' after %d members\n",
               (unsigned)abcode, next - 1);
        recover("first", &arc, &state);
        CHECK(state == FTPD_ABD_FREE,
              "the first SB14: the DD is gone after the abandon");
        CHECK(strstr(ftpd_abd_close_text(state), "STILL HELD") == NULL,
              "the first SB14: FTPD076W does not say the DD is held");
    }

    abcode = fill(&next);
    if (abcode > 0) {
        recover("second", &arc, &state);
        CHECK(state == FTPD_ABD_FREE,
              "the second SB14: the DD is gone after the abandon");
        CHECK(strstr(ftpd_abd_close_text(state), "STILL HELD") == NULL,
              "the second SB14: FTPD076W does not say the DD is held");
    }

    if (__dsalcf(dd, "DSN=%s;DISP=(OLD,DELETE)", dsn) == 0)
        __dsfree(dd);

    return mbt_test_summary("TSTABR");
}
