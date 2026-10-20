/* TSTLSE.C
** Tests for ftpd_list_nomem() -- telling a storage shortage from an empty
** result after a libc370 list builder answered (issue #118).
**
** __listds() returns NULL for "nothing there" and, since libc370 #61, also
** for "ran out of storage"; only errno differs.  LIST read every NULL as an
** empty level and sent 226.  The cases pin down which pairs mean a shortage.
**
** The builders themselves cannot be driven into ENOMEM from a unit test, so
** the call sites in ftpd#mvs.c are covered by reading: errno = 0 before the
** call, errno copied into a local right after it, ENOMEM answered 451.
** ftpd#lse.c is free of project and MVS headers, so this is a DUAL test.
*/
#include <errno.h>
#include <stdio.h>

#include "ftpd#lse.h"
#include <mbtcheck.h>

int
main(void)
{
    int dummy = 0;
    void *list = &dummy;

    printf("=== FTPD list builder result tests (#118) ===\n");

    CHECK(ftpd_list_nomem(NULL, ENOMEM) == 1,
          "NULL with ENOMEM is a storage shortage (was an empty listing)");
    CHECK(ftpd_list_nomem(NULL, 0) == 0,
          "NULL with errno 0 is an empty result");
    CHECK(ftpd_list_nomem(NULL, ENOENT) == 0,
          "NULL with another errno is not a storage shortage");
    CHECK(ftpd_list_nomem(list, ENOMEM) == 0,
          "a list that came back is complete, a stale ENOMEM notwithstanding");
    CHECK(ftpd_list_nomem(list, 0) == 0, "a list with errno 0 is fine");

    return mbt_test_summary("TSTLSE");
}
