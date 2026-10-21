/*
** FTPD Abandon Check
**
** See include/ftpd#abd.h.
*/
#include <string.h>

#include "ftpd#abd.h"

#define TIOT_FIRST_ENTRY  24        /* after job name and step names     */
#define TIOT_MAX_BYTES    65536     /* a corrupt table must end somewhere */
#define TIOE_STATUS_A     1         /* TIOESTTA                          */
#define TIOE_NOT_IN_USE   0x80      /* ... entry not in use              */
#define TIOE_DDNAME       4         /* TIOEDDNM, 8 bytes                 */
#define DDNAME_LEN        8

/* --------------------------------------------------------------------
** Is a DD of this name still in the TIOT?  See ftpd#abd.h.
** ----------------------------------------------------------------- */
int
ftpd_abd_dd_state(const void *tiot, const char *ddname)
{
    const unsigned char *t = (const unsigned char *)tiot;
    char want[DDNAME_LEN];
    size_t len;
    size_t at;

    if (!t || !ddname || ddname[0] == '\0')
        return FTPD_ABD_UNKNOWN;

    len = strlen(ddname);
    if (len > DDNAME_LEN)
        return FTPD_ABD_UNKNOWN;

    memset(want, ' ', sizeof(want));
    memcpy(want, ddname, len);

    for (at = TIOT_FIRST_ENTRY;
         at < TIOT_MAX_BYTES && t[at] != 0;
         at += t[at]) {
        if (t[at + TIOE_STATUS_A] & TIOE_NOT_IN_USE)
            continue;
        if (memcmp(t + at + TIOE_DDNAME, want, DDNAME_LEN) == 0)
            return FTPD_ABD_HELD;
    }

    return FTPD_ABD_FREE;
}

/* --------------------------------------------------------------------
** The tail of the FTPD076W text.  See ftpd#abd.h.
** ----------------------------------------------------------------- */
const char *
ftpd_abd_close_text(int state)
{
    switch (state) {
    case FTPD_ABD_HELD:
        return "CLOSE ABENDED, DD STILL HELD";
    case FTPD_ABD_FREE:
        return "CLOSE ABENDED, DD RELEASED";
    default:
        return "CLOSE ABENDED, DD STATE UNKNOWN";
    }
}
