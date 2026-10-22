/*
** FTPD job ID shape check
**
** See include/ftpd#jid.h.
*/
#include <string.h>

#include "ftpd#jid.h"

#define JID_PREFIX 3
#define JID_DIGITS 5

static const char *const jid_prefix[] = { "JOB", "STC", "TSU" };

int
ftpd_jid_valid(const char *id)
{
    size_t i;
    int k;

    if (!id)
        return 0;

    for (i = 0; i < sizeof(jid_prefix) / sizeof(jid_prefix[0]); i++) {
        if (strncmp(id, jid_prefix[i], JID_PREFIX) == 0)
            break;
    }
    if (i == sizeof(jid_prefix) / sizeof(jid_prefix[0]))
        return 0;

    for (k = JID_PREFIX; k < JID_PREFIX + JID_DIGITS; k++) {
        if (id[k] < '0' || id[k] > '9')
            return 0;
    }
    return id[k] == '\0';
}
