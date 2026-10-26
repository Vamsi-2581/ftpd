/*
** FTPD LIST/NLST Member Pattern Argument
**
** See include/ftpd#lmp.h.
*/
#include <ctype.h>
#include <string.h>

#include "ftpd#dsn.h"
#include "ftpd#lmp.h"

/* FTPD_MAX_DSN_LEN (ftpd.h) -- not included, that header needs MVS. */
#define LMP_MAX_DSN     44
#define LMP_MAX_MBR     8

int
ftpd_lmp_split(const char *arg, const char *cwd,
               char *dsn, int dsnsz,
               char *pattern, int patsz)
{
    const char *body;
    const char *lp;
    int quoted;
    int len;
    int baselen;
    int cwdlen;
    int plen;
    int i;

    if (!arg || !arg[0])
        return 0;

    quoted = (arg[0] == '\'');
    body = arg + quoted;
    len = (int)strlen(body);
    if (quoted && len > 0 && body[len - 1] == '\'')
        len--;

    /* Nothing but the closing parenthesis may end the argument. */
    if (len < 2 || body[len - 1] != ')')
        return 0;
    for (lp = body; lp < body + len && *lp != '('; lp++)
        ;
    if (lp == body + len)
        return 0;

    /* One level of parentheses: DSN(a)(b) is not a member form. */
    plen = (int)(body + len - 1 - (lp + 1));
    for (i = 0; i < plen; i++)
        if (lp[1 + i] == '(' || lp[1 + i] == ')')
            return 0;

    baselen = (int)(lp - body);
    cwdlen = quoted ? 0 : (int)strlen(cwd);
    if (cwdlen + baselen > LMP_MAX_DSN || cwdlen + baselen >= dsnsz)
        return -1;

    memcpy(dsn, cwd, cwdlen);
    memcpy(dsn + cwdlen, body, baselen);
    len = cwdlen + baselen;
    dsn[len] = '\0';
    for (i = 0; i < len; i++)
        dsn[i] = (char)toupper((unsigned char)dsn[i]);
    if (len > 0 && dsn[len - 1] == '.')
        dsn[--len] = '\0';

    if (strpbrk(dsn, FTPD_DSN_WILDCARDS) != NULL)
        return -2;
    if (!ftpd_dsn_valid(dsn, 0))
        return -3;

    if (plen > LMP_MAX_MBR)
        plen = LMP_MAX_MBR;
    if (plen > patsz - 1)
        plen = patsz - 1;
    for (i = 0; i < plen; i++)
        pattern[i] = (char)toupper((unsigned char)lp[1 + i]);
    pattern[plen < 0 ? 0 : plen] = '\0';

    return 1;
}
