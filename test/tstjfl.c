/* TSTJFL.C
** Tests for ftpd_jfl_name() and ftpd_jfl_match() -- the JESSTATUS filter of
** the JES mode of LIST, RETR and DELE (#207).  SITE JESSTATUS= was stored and
** never looked at: every job showed up whatever it said.
**
** ftpd#jfl.c is free of project and MVS headers, so this is a DUAL test.
*/
#include <stdio.h>
#include <string.h>

#include "ftpd#jfl.h"
#include <mbtcheck.h>

static int
is(const char *got, const char *want)
{
    return got != NULL && strcmp(got, want) == 0;
}

int
main(void)
{
    CHECK(is(ftpd_jfl_name("ALL"), "ALL"), "ALL");
    CHECK(is(ftpd_jfl_name("INPUT"), "INPUT"), "INPUT");
    CHECK(is(ftpd_jfl_name("ACTIVE"), "ACTIVE"), "ACTIVE");
    CHECK(is(ftpd_jfl_name("OUTPUT"), "OUTPUT"), "OUTPUT");
    CHECK(is(ftpd_jfl_name("output"), "OUTPUT"), "lower case is accepted");
    CHECK(is(ftpd_jfl_name("Active"), "ACTIVE"), "mixed case is accepted");
    CHECK(is(ftpd_jfl_name(""), "ALL"), "an empty value resets to ALL");
    CHECK(is(ftpd_jfl_name(NULL), "ALL"), "NULL is ALL");

    CHECK(ftpd_jfl_name("HELD") == NULL, "HELD is not a JESSTATUS");
    CHECK(ftpd_jfl_name("OUT") == NULL, "an abbreviated value is refused");
    CHECK(ftpd_jfl_name("OUTPUTS") == NULL, "a longer value is refused");
    CHECK(ftpd_jfl_name("ALLL") == NULL, "ALLL is refused");
    CHECK(ftpd_jfl_name("*") == NULL, "a wildcard is refused");

    CHECK(ftpd_jfl_match("ALL", "OUTPUT") == 1, "ALL passes OUTPUT");
    CHECK(ftpd_jfl_match("ALL", "ACTIVE") == 1, "ALL passes ACTIVE");
    CHECK(ftpd_jfl_match("ALL", "XMIT") == 1, "ALL passes XMIT");
    CHECK(ftpd_jfl_match("ALL", "UNKNOWN") == 1, "ALL passes UNKNOWN");
    CHECK(ftpd_jfl_match(NULL, "OUTPUT") == 1, "NULL filter passes");
    CHECK(ftpd_jfl_match("", "INPUT") == 1, "empty filter passes");

    CHECK(ftpd_jfl_match("OUTPUT", "OUTPUT") == 1, "OUTPUT passes OUTPUT");
    CHECK(ftpd_jfl_match("OUTPUT", "ACTIVE") == 0, "OUTPUT stops ACTIVE");
    CHECK(ftpd_jfl_match("OUTPUT", "INPUT") == 0, "OUTPUT stops INPUT");
    CHECK(ftpd_jfl_match("ACTIVE", "ACTIVE") == 1, "ACTIVE passes ACTIVE");
    CHECK(ftpd_jfl_match("ACTIVE", "OUTPUT") == 0, "ACTIVE stops OUTPUT");
    CHECK(ftpd_jfl_match("INPUT", "INPUT") == 1, "INPUT passes INPUT");
    CHECK(ftpd_jfl_match("INPUT", "XMIT") == 0, "INPUT stops XMIT");
    CHECK(ftpd_jfl_match("INPUT", "UNKNOWN") == 0, "INPUT stops UNKNOWN");
    CHECK(ftpd_jfl_match("OUTPUT", NULL) == 0, "no state passes no filter but ALL");

    return mbt_test_summary("TSTJFL");
}
