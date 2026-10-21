/* TSTABD.C
** Tests for the DD check behind FTPD076W (issue #232).
**
** The first recovered SB14 of a process abends SA0A in the CLOSE that
** __fabandon() issues, and the warning said "DD STILL HELD" although the DD
** was free: a batch job with DISP=(OLD,DELETE) ran without waiting and MKD of
** the name succeeded.  __fabandon() returns the ABEND code either way, so
** FTPD looks into the TIOT.  These cases build a TIOT by hand: a freed DD is
** one that is not there (or is flagged unused), a held one is still listed.
**
** ftpd#abd.c is free of project and MVS headers, so this is a DUAL test.
*/
#include <stdio.h>
#include <string.h>

#include "ftpd#abd.h"
#include <mbtcheck.h>

#define TIOT_HEADER  24         /* job name + step names                 */
#define ENTRY_LEN    20         /* 16 bytes DD entry + 4 byte device     */
#define ENTRY_NOTUSE 0x80       /* TIOESTTA: entry not in use            */

/* Append a DD entry; returns the next free offset. */
static size_t
add(unsigned char *tiot, size_t at, const char *ddname, int status)
{
    memset(tiot + at, '\0', ENTRY_LEN);
    tiot[at] = ENTRY_LEN;
    tiot[at + 1] = (unsigned char)status;
    memset(tiot + at + 4, ' ', 8);
    memcpy(tiot + at + 4, ddname, strlen(ddname));
    return at + ENTRY_LEN;
}

int
main(void)
{
    unsigned char tiot[256];
    size_t at;

    /* A TIOT with SYSIN, a data set DD, SYSPRINT -- the zero length byte
    ** after the last entry ends it. */
    memset(tiot, '\0', sizeof(tiot));
    at = TIOT_HEADER;
    at = add(tiot, at, "SYSIN", 0);
    at = add(tiot, at, "SYS00042", 0);
    at = add(tiot, at, "SYSPRINT", 0);

    CHECK(ftpd_abd_dd_state(tiot, "SYS00042") == FTPD_ABD_HELD,
          "a DD in the TIOT is held");
    CHECK(ftpd_abd_dd_state(tiot, "SYSIN") == FTPD_ABD_HELD,
          "the first entry is found");
    CHECK(ftpd_abd_dd_state(tiot, "SYSPRINT") == FTPD_ABD_HELD,
          "the last entry is found");
    CHECK(ftpd_abd_dd_state(tiot, "SYS00043") == FTPD_ABD_FREE,
          "a DD that is not in the TIOT is free");
    CHECK(ftpd_abd_dd_state(tiot, "SYS0004") == FTPD_ABD_FREE,
          "a prefix of a DD name is not that DD");
    CHECK(ftpd_abd_dd_state(tiot, "SYS000420") == FTPD_ABD_UNKNOWN,
          "more than eight characters is no DD name");

    /* Unallocation leaves the slot behind, flagged unused, and the entries
    ** after it stay reachable. */
    memset(tiot, '\0', sizeof(tiot));
    at = TIOT_HEADER;
    at = add(tiot, at, "SYSIN", 0);
    at = add(tiot, at, "SYS00042", ENTRY_NOTUSE);
    at = add(tiot, at, "SYSPRINT", 0);
    CHECK(ftpd_abd_dd_state(tiot, "SYS00042") == FTPD_ABD_FREE,
          "an entry flagged not in use is free");
    CHECK(ftpd_abd_dd_state(tiot, "SYSPRINT") == FTPD_ABD_HELD,
          "the entry after an unused slot is still found");

    /* A slot with a blank name must not match a blank search. */
    CHECK(ftpd_abd_dd_state(tiot, "") == FTPD_ABD_UNKNOWN,
          "an empty name is unknown, not free");
    CHECK(ftpd_abd_dd_state(NULL, "SYSIN") == FTPD_ABD_UNKNOWN,
          "no TIOT is unknown");
    CHECK(ftpd_abd_dd_state(tiot, NULL) == FTPD_ABD_UNKNOWN,
          "no name is unknown");

    /* An empty TIOT: no entries at all. */
    memset(tiot, '\0', sizeof(tiot));
    CHECK(ftpd_abd_dd_state(tiot, "SYS00042") == FTPD_ABD_FREE,
          "a TIOT without entries holds nothing");

    /* The wording is the point of the issue: "held" only when held. */
    CHECK(strstr(ftpd_abd_close_text(FTPD_ABD_FREE), "STILL HELD") == NULL,
          "a released DD is not reported as held");
    CHECK(strstr(ftpd_abd_close_text(FTPD_ABD_FREE), "RELEASED") != NULL,
          "a released DD is reported as released");
    CHECK(strstr(ftpd_abd_close_text(FTPD_ABD_HELD), "STILL HELD") != NULL,
          "a held DD is reported as held");
    CHECK(strstr(ftpd_abd_close_text(FTPD_ABD_UNKNOWN), "STILL HELD") == NULL,
          "an unknown state is not reported as held");
    CHECK(strstr(ftpd_abd_close_text(FTPD_ABD_UNKNOWN), "UNKNOWN") != NULL,
          "an unknown state is reported as unknown");

    return mbt_test_summary("TSTABD");
}
