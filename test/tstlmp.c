/* TSTLMP.C
** Tests for ftpd_lmp_split() -- the LIST/NLST argument DSN(pattern) that the
** Zowe FTP plugin sends as LIST 'IBMUSER.CNTL(*)' (issue #302).
**
** Free of project and MVS headers, so a DUAL test: native via `mbt test`,
** on MVS via `mbt test --mvs`.
*/
#include <stdio.h>
#include <string.h>

#include "ftpd#lmp.h"
#include <mbtcheck.h>

static char dsn[64];
static char pat[16];

/* Split with the given cwd; the buffers are poisoned first so a case that
** must leave them alone can be told from one that wrote an empty string. */
static int
split(const char *arg, const char *cwd)
{
    memset(dsn, '#', sizeof(dsn) - 1);
    dsn[sizeof(dsn) - 1] = '\0';
    memset(pat, '#', sizeof(pat) - 1);
    pat[sizeof(pat) - 1] = '\0';
    return ftpd_lmp_split(arg, cwd, dsn, sizeof(dsn), pat, sizeof(pat));
}

static int
untouched(void)
{
    return dsn[0] == '#' && pat[0] == '#';
}

int
main(void)
{
    printf("=== FTPD LIST member pattern tests (#302) ===\n");

    /* --- the reported command ----------------------------------------- */
    CHECK(split("'IBMUSER.CNTL(*)'", "HERC01.") == 1,
          "the Zowe plugin's LIST 'DSN(*)' is a member pattern");
    CHECK(strcmp(dsn, "IBMUSER.CNTL") == 0, "  the data set is IBMUSER.CNTL");
    CHECK(strcmp(pat, "*") == 0, "  the pattern is *");

    /* --- quoting and the CWD ------------------------------------------ */
    CHECK(split("'SYS1.MACLIB(AB*)'", "HERC01.") == 1 &&
          strcmp(dsn, "SYS1.MACLIB") == 0 && strcmp(pat, "AB*") == 0,
          "an absolute name ignores the CWD");
    CHECK(split("'SYS1.MACLIB(AB*)", "HERC01.") == 1 &&
          strcmp(dsn, "SYS1.MACLIB") == 0 && strcmp(pat, "AB*") == 0,
          "the closing quote is optional");
    CHECK(split("CNTL(*)", "HERC01.") == 1 &&
          strcmp(dsn, "HERC01.CNTL") == 0 && strcmp(pat, "*") == 0,
          "an unquoted name is relative to the CWD");
    CHECK(split("SYS1.MACLIB(*)", "HERC01.") == 1 &&
          strcmp(dsn, "HERC01.SYS1.MACLIB") == 0,
          "an unquoted name gets the CWD even when it looks absolute");

    /* --- case, pattern shape ------------------------------------------ */
    CHECK(split("'ibmuser.cntl(abc%)'", "HERC01.") == 1 &&
          strcmp(dsn, "IBMUSER.CNTL") == 0 && strcmp(pat, "ABC%") == 0,
          "name and pattern are uppercased, % is kept");
    CHECK(split("'A.B()'", "HERC01.") == 1 && pat[0] == '\0',
          "DSN() is every member");
    CHECK(split("'A.B(ABCDEFGHIJ)'", "HERC01.") == 1 &&
          strcmp(pat, "ABCDEFGH") == 0,
          "a pattern is cut to the 8 characters of a member name");
    CHECK(split("'A.B(*", "HERC01.") == 0 && untouched(),
          "no closing parenthesis: not a member form");

    /* --- not a member form: left to the data set listing -------------- */
    CHECK(split("'IBMUSER.CNTL'", "HERC01.") == 0 && untouched(),
          "a plain quoted name is not a member form");
    CHECK(split("IBMUSER.*", "HERC01.") == 0 && untouched(),
          "a plain pattern is not a member form");
    CHECK(split("*", "HERC01.") == 0 && untouched(), "* is not a member form");
    CHECK(split("", "HERC01.") == 0, "an empty argument is not a member form");
    CHECK(split("'A.B(X)Y'", "HERC01.") == 0,
          "text after the closing parenthesis: not a member form");
    CHECK(split("')A.B('", "HERC01.") == 0,
          "a ')' before the '(' : not a member form");

    /* --- names that cannot be a PDS ----------------------------------- */
    CHECK(split("'IBMUSER.*(*)'", "HERC01.") == -2,
          "a wildcard in the data set part answers -2 as resolve_dsn does");
    CHECK(split("'IBMUSER.CN/TL(*)'", "HERC01.") == -3,
          "a character no name may hold answers -3");
    CHECK(split("'IBMUSER..CNTL(*)'", "HERC01.") == -3,
          "an empty qualifier answers -3");
    CHECK(split("'(*)'", "HERC01.") == -3, "no data set name at all answers -3");
    CHECK(ftpd_lmp_split("'AAAAAAAA.BBBBBBBB.CCCCCCCC.DDDDDDDD.EEEEEEEEF(*)'",
                         "HERC01.", dsn, sizeof(dsn), pat, sizeof(pat)) == -1,
          "a name over 44 characters answers -1");
    CHECK(ftpd_lmp_split("'IBMUSER.CNTL(*)'", "HERC01.", dsn, 8,
                         pat, sizeof(pat)) == -1,
          "a dsn buffer too small answers -1");
    CHECK(ftpd_lmp_split("'A.B(ABC*)'", "HERC01.", dsn, sizeof(dsn),
                         pat, 3) == 1 && strcmp(pat, "AB") == 0,
          "a small pattern buffer cuts, it does not overrun");

    return mbt_test_summary("TSTLMP");
}
