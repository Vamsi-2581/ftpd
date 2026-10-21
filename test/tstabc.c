/* TSTABC.C
** Tests for ftpd_abc_text() -- the ABEND code in FTPD070E and FTPD077E
** (issue #260).
**
** FTPD077E decoded the value try() returned as a system code only, so a user
** ABEND and a failed ESTAE both read S000.  try() returns 0x00sssuuu: system
** code in bits 12-23, user code in the low 12 bits, a negative value when the
** ESTAE was not set up.
**
** ftpd#abc.c is free of project and MVS headers, so this is a DUAL test.
*/
#include <stdio.h>
#include <string.h>

#include "ftpd#abc.h"
#include <mbtcheck.h>

static int
is(int rc, int found, const char *text)
{
    char buf[FTPD_ABC_LEN];
    int got = ftpd_abc_text(rc, buf);

    return got == found && strcmp(buf, text) == 0;
}

int
main(void)
{
    CHECK(is(0x000C4000, 1, "S0C4"), "S0C4");
    CHECK(is(0x00B14000, 1, "SB14"), "SB14 keeps its letters");
    CHECK(is(0x00047000, 1, "S047"), "S047, as an unauthorized MODESET ends");
    CHECK(is(0x00001000, 1, "S001"), "the lowest system code is S001");
    CHECK(is(0x00FFF000, 1, "SFFF"), "the highest system code is SFFF");
    CHECK(is(0x00B14ABC, 1, "SB14"),
          "a system ABEND with a reason in the user part is still system");

    CHECK(is(123, 1, "U0123"), "a user ABEND is U and decimal");
    CHECK(is(1, 1, "U0001"), "the lowest user code is U0001");
    CHECK(is(0xFFF, 1, "U4095"),
          "0xFFF is the highest user code, not a system code");

    CHECK(is(0, 0, ""), "zero is no ABEND");
    CHECK(is(-1, 0, ""), "a failed ESTAE has no code");
    CHECK(is(-2147483647 - 1, 0, ""), "the most negative value has no code");

    {
        char buf[FTPD_ABC_LEN] = "stale";

        ftpd_abc_text(0, buf);
        CHECK(buf[0] == '\0', "a stale buffer is cleared");
    }

    return mbt_test_summary("TSTABC");
}
