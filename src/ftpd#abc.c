/*
** FTPD ABEND Code Text
**
** See include/ftpd#abc.h.
*/
#include <stdio.h>

#include "ftpd#abc.h"

#define USER_CODE_MASK  0xFFF

/* --------------------------------------------------------------------
** The code try() returned, as text.  See ftpd#abc.h.
** ----------------------------------------------------------------- */
int
ftpd_abc_text(int rc, char *buf)
{
    buf[0] = '\0';

    if (rc <= 0)
        return 0;

    if (rc > USER_CODE_MASK)
        snprintf(buf, FTPD_ABC_LEN, "S%03X", (unsigned)(rc >> 12) & 0xFFF);
    else
        snprintf(buf, FTPD_ABC_LEN, "U%04u", (unsigned)rc);

    return 1;
}
