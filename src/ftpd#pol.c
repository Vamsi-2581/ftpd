/*
** FTPD Worker Pool Size
**
** See include/ftpd#pol.h.
*/
#include "ftpd#pol.h"

int
ftpd_pool_size(int max_sessions)
{
    return max_sessions < FTPD_POOL_MIN ? FTPD_POOL_MIN : max_sessions;
}
