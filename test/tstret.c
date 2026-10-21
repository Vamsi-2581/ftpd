/* TSTRET.C
** Tests for ftpd_ret_unexpired() -- the expiration date check that keeps
** STOR and APPE from walking into IEC507D (issue #198).
**
** ftpd#ret.c is free of project and MVS headers, so this is a DUAL test: it
** runs natively via `mbt test` and on MVS via `mbt test --mvs`.
**
** The boundary cases come from a measurement on MVS 3.8j: with the local date
** 2026.293, a data set with expdt 26293 opened for output without a reply,
** one with 26294 raised IEC507D, and so did 99365.  The DSCB holds the
** two-digit year, which the first version of this check read as 1926.
*/
#include <stdio.h>

#include "ftpd#ret.h"
#include <mbtcheck.h>

/* DS1EXPDT for year (full) and day of the year, as MVS 3.8j stores it: the
** two-digit year, so 2026.323 is X'1A0143' (read off a DSCB written by
** SITE RETPD=30).  Years before 2000 keep their offset from 1900. */
static void
expdt(unsigned char b[3], int year, int yday)
{
    b[0] = (unsigned char)(year >= 2000 ? year - 2000 : year - 1900);
    b[1] = (unsigned char)(yday >> 8);
    b[2] = (unsigned char)(yday & 0xFF);
}

static int
active(int year, int yday, int cur_year, int cur_yday)
{
    unsigned char b[3];

    expdt(b, year, yday);
    return ftpd_ret_unexpired(b, cur_year, cur_yday, NULL);
}

int
main(void)
{
    unsigned char b[3];
    ftpd_ret_date_t d;

    printf("=== FTPD retention check tests (#198) ===\n");

    b[0] = b[1] = b[2] = 0;
    CHECK(ftpd_ret_unexpired(b, 2026, 293, NULL) == 0,
          "an all-zero expdt is no retention");

    CHECK(active(2026, 294, 2026, 293) == 1, "tomorrow is retained");
    CHECK(active(2026, 293, 2026, 293) == 0, "today is expired");
    CHECK(active(2026, 292, 2026, 293) == 0, "yesterday is expired");
    CHECK(active(2027, 1, 2026, 366) == 1, "next year is retained");
    CHECK(active(2025, 366, 2026, 1) == 0, "last year is expired");
    CHECK(active(2026, 366, 2026, 365) == 1, "leap day count in a leap year");

    b[0] = 99; b[1] = 0x01; b[2] = 0x6D;
    CHECK(ftpd_ret_unexpired(b, 2026, 293, &d) == 1 && d.permanent == 1,
          "99365 never expires");
    b[0] = 99; b[1] = 0x01; b[2] = 0x6E;
    CHECK(ftpd_ret_unexpired(b, 2026, 293, &d) == 1 && d.permanent == 1,
          "99366 never expires");
    b[0] = 99; b[1] = 0x00; b[2] = 0x64;
    CHECK(ftpd_ret_unexpired(b, 2026, 293, &d) == 1 && !d.permanent
          && d.year == 2099, "99100 is April 2099, not permanent");

    b[0] = 0x1A; b[1] = 0x01; b[2] = 0x43;
    CHECK(ftpd_ret_unexpired(b, 2026, 294, &d) == 1 && d.year == 2026
          && d.yday == 323, "X'1A0143' (SITE RETPD=30 on 2026.293) is 2026.323");

    b[0] = 126; b[1] = 0x01; b[2] = 0x26;
    CHECK(ftpd_ret_unexpired(b, 2026, 293, &d) == 1 && d.year == 2026
          && d.yday == 294, "a year byte of 126 is the year since 1900");

    expdt(b, 2026, 0);
    CHECK(ftpd_ret_unexpired(b, 2026, 293, NULL) == 0, "day 0 is not a date");
    expdt(b, 2030, 400);
    CHECK(ftpd_ret_unexpired(b, 2026, 293, NULL) == 0, "day 400 is not a date");

    expdt(b, 2026, 294);
    CHECK(ftpd_ret_unexpired(b, 2026, 293, &d) == 1 && d.year == 2026
          && d.month == 10 && d.day == 21 && d.yday == 294 && !d.permanent,
          "2026.294 is 2026-10-21");
    expdt(b, 2026, 1);
    ftpd_ret_unexpired(b, 2025, 365, &d);
    CHECK(d.month == 1 && d.day == 1, "day 1 is January 1");
    expdt(b, 2026, 365);
    ftpd_ret_unexpired(b, 2025, 365, &d);
    CHECK(d.month == 12 && d.day == 31, "2026 day 365 is December 31");
    expdt(b, 2028, 60);
    ftpd_ret_unexpired(b, 2025, 365, &d);
    CHECK(d.month == 2 && d.day == 29, "2028 day 60 is February 29");
    expdt(b, 2027, 60);
    ftpd_ret_unexpired(b, 2025, 365, &d);
    CHECK(d.month == 3 && d.day == 1, "2027 day 60 is March 1");
    expdt(b, 2028, 366);
    ftpd_ret_unexpired(b, 2025, 365, &d);
    CHECK(d.month == 12 && d.day == 31, "2028 day 366 is December 31");

    return mbt_test_summary("TSTRET");
}
