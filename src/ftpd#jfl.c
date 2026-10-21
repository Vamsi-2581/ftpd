/*
** FTPD JESSTATUS filter
**
** See include/ftpd#jfl.h.
*/
#include <ctype.h>
#include <string.h>

#include "ftpd#jfl.h"

static const char *const jfl_names[] = { "ALL", "INPUT", "ACTIVE", "OUTPUT" };

const char *
ftpd_jfl_name(const char *val)
{
    size_t i;

    if (!val || !val[0])
        return jfl_names[0];

    for (i = 0; i < sizeof(jfl_names) / sizeof(jfl_names[0]); i++) {
        const char *n = jfl_names[i];
        const char *v = val;

        while (*n && *v && *n == (char)toupper((unsigned char)*v)) {
            n++;
            v++;
        }
        if (*n == '\0' && *v == '\0')
            return jfl_names[i];
    }
    return NULL;
}

int
ftpd_jfl_match(const char *filter, const char *job_status)
{
    if (!filter || !filter[0] || strcmp(filter, "ALL") == 0)
        return 1;
    return job_status != NULL && strcmp(filter, job_status) == 0;
}
