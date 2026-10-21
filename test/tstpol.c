/* TSTPOL.C
** Tests for ftpd_pool_size() -- the worker pool is never smaller than
** FTPD_POOL_MIN, because libc370's thread manager starts no worker for a pool
** of 1 to 3 (libc370#700, ftpd#168).  MAXSESSIONS=2 left FTPD accepting and
** queueing connections that no worker ever picked up.
**
** ftpd#pol.c is free of project and MVS headers, so this is a DUAL test.
*/
#include <stdio.h>

#include "ftpd#pol.h"
#include <mbtcheck.h>

int
main(void)
{
    CHECK(ftpd_pool_size(1) == 4, "MAXSESSIONS=1 gets a pool of 4");
    CHECK(ftpd_pool_size(2) == 4, "MAXSESSIONS=2 gets a pool of 4");
    CHECK(ftpd_pool_size(3) == 4, "MAXSESSIONS=3 gets a pool of 4");
    CHECK(ftpd_pool_size(4) == 4, "MAXSESSIONS=4 gets a pool of 4");
    CHECK(ftpd_pool_size(5) == 5, "MAXSESSIONS=5 is not raised");
    CHECK(ftpd_pool_size(10) == 10, "the default 10 is not raised");
    CHECK(ftpd_pool_size(64) == 64, "a large limit is not raised");
    CHECK(ftpd_pool_size(0) == 4, "a nonsense 0 still gets a pool of 4");
    CHECK(ftpd_pool_size(-1) == 4, "a negative value still gets a pool of 4");

    return mbt_test_summary("TSTPOL");
}
