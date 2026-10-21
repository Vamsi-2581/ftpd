/* TSTABT.C
** The DD check behind FTPD076W (issue #232) against the real TIOT.
**
** TSTABD feeds the check a TIOT built by hand.  This is the other half: the
** layout it assumes (entries from offset 24, a length byte each, the DD name
** at offset 4, X'80' for an unused slot) is read off the IBM mapping, and has
** to hold on the table the system actually keeps.  A DD that is allocated
** must be found, one that was freed must not, and the DD name an
** open FILE keeps must be the one the TIOT lists.
**
** The data set is a temporary one, so there is nothing to clean up.
**
** MVS-only: SVC 99 and the TIOT have no host equivalent.
**
** RC 0 = all checks passed.
*/
#include <stdio.h>
#include <string.h>

#include <mvs/dd.h>
#include <mvs/dynalloc.h>
#include <mbtcheck.h>

#include "ftpd#abd.h"

int
main(void)
{
    char  dd[9];
    FILE *fp;
    int   rc;

    rc = __dsalcf(dd, "DDNAME=TSTABTW;DISP=(NEW,DELETE);DSORG=PS;"
                      "RECFM=FB;LRECL=80;BLKSIZE=3120;SPACE=TRK(1,1);"
                      "UNIT=SYSDA");
    CHECK(rc == 0, "the scratch DD is allocated");
    if (rc != 0)
        return mbt_test_summary("TSTABT");

    CHECK(ftpd_abd_dd_state(__tiot(), dd) == FTPD_ABD_HELD,
          "an allocated DD is found in the real TIOT");
    CHECK(ftpd_abd_dd_state(__tiot(), "TSTABTX") == FTPD_ABD_FREE,
          "a DD nobody allocated is not");

    fp = fopen("dd:TSTABTW", "w");
    CHECK(fp != NULL, "the DD opens for output");
    if (fp) {
        CHECK(strcmp(fp->ddname, "TSTABTW") == 0,
              "the FILE keeps the DD name it was opened through");
        CHECK(ftpd_abd_dd_state(__tiot(), fp->ddname) == FTPD_ABD_HELD,
              "...and the TIOT lists it while it is open");
        fclose(fp);
    }

    CHECK(ftpd_abd_dd_state(__tiot(), "TSTABTW") == FTPD_ABD_HELD,
          "closing the FILE does not free the DD allocated by hand");

    CHECK(__dsfree("TSTABTW") == 0, "the DD is freed");
    CHECK(ftpd_abd_dd_state(__tiot(), "TSTABTW") == FTPD_ABD_FREE,
          "a freed DD is no longer held");

    return mbt_test_summary("TSTABT");
}
