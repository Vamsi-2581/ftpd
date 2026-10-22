/*
** FTPD JES spool authorization
**
** See include/ftpd#jwn.h.
*/
#include <ctype.h>
#include <string.h>

#include "ftpd#jwn.h"

/* --------------------------------------------------------------------
** Components and entities
** ----------------------------------------------------------------- */
int
ftpd_jwn_component_ok(const char *s)
{
    size_t i;

    if (!s || !s[0])
        return 0;
    for (i = 0; s[i]; i++) {
        if (i >= FTPD_JWN_COMPMAX)
            return 0;
        if (strchr(FTPD_JWN_CHARS, s[i]) == NULL)
            return 0;
    }
    return 1;
}

int
ftpd_jwn_entity(char *out, int sz, const char *owner, const char *jobname,
                const char *jobid)
{
    size_t lo, lj, li;
    int need;

    if (!out || sz < 1 || !owner || !jobname || !jobid)
        return -1;
    out[0] = '\0';

    if (owner[0] && !ftpd_jwn_component_ok(owner))
        return -1;
    if (!ftpd_jwn_component_ok(jobname) || !ftpd_jwn_component_ok(jobid))
        return -1;

    lo = strlen(owner);
    lj = strlen(jobname);
    li = strlen(jobid);
    need = (int)(lo + 1 + lj + 1 + li);
    if (need + 1 > sz)
        return -1;

    memcpy(out, owner, lo);
    out[lo] = '.';
    memcpy(out + lo + 1, jobname, lj);
    out[lo + 1 + lj] = '.';
    memcpy(out + lo + 1 + lj + 1, jobid, li);
    out[need] = '\0';
    return need;
}

int
ftpd_jwn_probe(char *out, int sz, const char *userid)
{
    size_t l = 0;

    if (!out || sz < 1)
        return -1;
    out[0] = '\0';

    if (userid && userid[0]) {
        if (!ftpd_jwn_component_ok(userid))
            return -1;
        l = strlen(userid);
    }
    if ((int)(l + 2 + 1) > sz)
        return -1;

    if (l)
        memcpy(out, userid, l);
    out[l] = '.';
    out[l + 1] = '.';
    out[l + 2] = '\0';
    return (int)(l + 2);
}

/* --------------------------------------------------------------------
** Ownership
** ----------------------------------------------------------------- */
static int
has_filler(const char *s)
{
    for (; *s; s++) {
        if ((unsigned char)*s <= (unsigned char)' ')
            return 1;
    }
    return 0;
}

int
ftpd_jwn_mine(const char *owner, const char *user)
{
    if (!owner || !user || !owner[0] || !user[0])
        return 0;
    if (has_filler(owner) || has_filler(user))
        return 0;
    return strcmp(owner, user) == 0;
}

/* --------------------------------------------------------------------
** Level and decision
** ----------------------------------------------------------------- */
int
ftpd_jwn_level(const ftpd_jwn_ctx_t *ctx, const char *entity,
               int want_update, ftpd_jwn_res_t *res)
{
    int rc;

    res->rc = -1;
    res->racf_calls = 0;

    if (!ctx || !ctx->auth_fn || !entity)
        return FTPD_JWN_LV_ERR;

    rc = ctx->auth_fn(ctx->auth_arg, entity, 0);
    res->rc = rc;
    res->racf_calls = 1;
    if (rc == 4)
        return FTPD_JWN_LV_NOPROFILE;
    if (rc == 8)
        return FTPD_JWN_LV_NONE;
    if (rc != 0)
        return FTPD_JWN_LV_ERR;
    if (!want_update)
        return FTPD_JWN_LV_READ;

    rc = ctx->auth_fn(ctx->auth_arg, entity, 1);
    res->rc = rc;
    res->racf_calls = 2;
    if (rc == 0)
        return FTPD_JWN_LV_UPDATE;
    if (rc == 8)
        return FTPD_JWN_LV_READ;
    return FTPD_JWN_LV_ERR;
}

void
ftpd_jwn_cache_init(ftpd_jwn_cache_t *c)
{
    if (c)
        memset(c, 0, sizeof(*c));
}

/* The level of a foreign job: from the cache if its key was asked in this
** command, otherwise from RACF (and then remembered unless the answer is
** unusable or the cache is full).  RC values are 0, 4 and 8 whenever a
** level is stored, so they fit a char. */
static int
level_cached(const ftpd_jwn_ctx_t *ctx, const char *entity, int want_update,
             ftpd_jwn_res_t *res)
{
    ftpd_jwn_cache_t *c = ctx->cache;
    char key[FTPD_JWN_KEYLEN + 1];
    int i, level;

    if (!c)
        return ftpd_jwn_level(ctx, entity, want_update, res);

    strncpy(key, entity, FTPD_JWN_KEYLEN);
    key[FTPD_JWN_KEYLEN] = '\0';

    for (i = 0; i < c->n; i++) {
        if (c->e[i].update == (char)(want_update != 0) &&
            strcmp(c->e[i].key, key) == 0) {
            res->rc = c->e[i].rc;
            res->racf_calls = 0;
            return c->e[i].level;
        }
    }

    level = ftpd_jwn_level(ctx, entity, want_update, res);
    if (level != FTPD_JWN_LV_ERR && c->n < FTPD_JWN_CACHEMAX) {
        strcpy(c->e[c->n].key, key);
        c->e[c->n].update = (char)(want_update != 0);
        c->e[c->n].level = (char)level;
        c->e[c->n].rc = (char)res->rc;
        c->n++;
    }
    return level;
}

int
ftpd_jwn_decide(int op, int level)
{
    switch (level) {
    case FTPD_JWN_LV_READ:
        return op == FTPD_JWN_OP_DELE ? FTPD_JWN_NOCANCEL : FTPD_JWN_ALLOW;
    case FTPD_JWN_LV_UPDATE:
        return FTPD_JWN_ALLOW;
    default:
        return FTPD_JWN_HIDE;
    }
}

static int
reason_of(int level)
{
    switch (level) {
    case FTPD_JWN_LV_NOPROFILE: return FTPD_JWN_R_NOPROFILE;
    case FTPD_JWN_LV_NONE:      return FTPD_JWN_R_NONE;
    case FTPD_JWN_LV_READ:      return FTPD_JWN_R_READ;
    case FTPD_JWN_LV_UPDATE:    return FTPD_JWN_R_UPDATE;
    default:                    return FTPD_JWN_R_ERR_RC;
    }
}

int
ftpd_jwn_check(const ftpd_jwn_ctx_t *ctx, int op, const char *owner,
               const char *jobname, const char *jobid, ftpd_jwn_res_t *res)
{
    char entity[FTPD_JWN_ENTSZ];
    int n;

    if (!res)
        return FTPD_JWN_HIDE;

    res->decision = FTPD_JWN_HIDE;
    res->reason = FTPD_JWN_R_ERR_RC;
    res->level = FTPD_JWN_LV_ERR;
    res->rc = -1;
    res->elen = 0;
    res->racf_calls = 0;

    if (!ctx)
        return res->decision;

    if (!ctx->acee_present || !ctx->user || !ctx->user[0]) {
        res->reason = FTPD_JWN_R_NOACEE;
        return res->decision;
    }

    if (ftpd_jwn_mine(owner, ctx->user)) {
        res->decision = FTPD_JWN_ALLOW;
        res->reason = FTPD_JWN_R_OWN;
        return res->decision;
    }

    n = ftpd_jwn_entity(entity, (int)sizeof(entity), owner ? owner : "",
                        jobname, jobid);
    if (n < 0) {
        res->reason = FTPD_JWN_R_ERR_COMPONENT;
        return res->decision;
    }
    res->elen = n;

    res->level = level_cached(ctx, entity, op == FTPD_JWN_OP_DELE, res);
    res->reason = reason_of(res->level);
    res->decision = ftpd_jwn_decide(op, res->level);
    return res->decision;
}

/* --------------------------------------------------------------------
** The owner display filter and SITE JESOWNER
** ----------------------------------------------------------------- */
int
ftpd_jwn_match(const char *pattern, const char *owner)
{
    if (!pattern || !pattern[0])
        return 0;
    if (pattern[0] == '*' && pattern[1] == '\0')
        return 1;
    if (!owner || !owner[0] || !ftpd_jwn_component_ok(pattern))
        return 0;
    return strcmp(owner, pattern) == 0;
}

int
ftpd_jwn_narrows(const char *user, const char *value, char *canon, int sz)
{
    char up[FTPD_JWN_PRTSZ];
    size_t len, i;
    int kind;

    if (canon && sz > 0)
        canon[0] = '\0';
    if (!value || !canon || sz < FTPD_JWN_PRTSZ)
        return FTPD_JWN_NR_BAD;

    len = strlen(value);
    if (len > FTPD_JWN_COMPMAX)
        return FTPD_JWN_NR_BAD;
    for (i = 0; i < len; i++)
        up[i] = (char)toupper((unsigned char)value[i]);
    up[len] = '\0';

    if (len == 0)
        kind = FTPD_JWN_NR_RESET;
    else if (ftpd_jwn_mine(up, user))
        kind = FTPD_JWN_NR_OWN;
    else if (up[0] == '*' && up[1] == '\0')
        kind = FTPD_JWN_NR_ALL;
    else if (ftpd_jwn_component_ok(up))
        kind = FTPD_JWN_NR_OTHER;
    else
        return FTPD_JWN_NR_BAD;

    memcpy(canon, up, len + 1);
    return kind;
}

/* --------------------------------------------------------------------
** Output
** ----------------------------------------------------------------- */
void
ftpd_jwn_printable(char *out, int sz, const char *value)
{
    int i = 0;

    if (!out || sz < 1)
        return;
    if (value) {
        while (value[i] && i < FTPD_JWN_COMPMAX && i < sz - 1) {
            out[i] = strchr(FTPD_JWN_CHARS, value[i]) ? value[i] : '?';
            i++;
        }
    }
    out[i] = '\0';
}

/* --------------------------------------------------------------------
** Counting and the lines that follow from it
** ----------------------------------------------------------------- */
void
ftpd_jwn_tally_init(ftpd_jwn_tally_t *t)
{
    if (t)
        memset(t, 0, sizeof(*t));
}

int
ftpd_jwn_tally(ftpd_jwn_tally_t *t, ftpd_jwn_sess_t *st, int op,
               const ftpd_jwn_res_t *res)
{
    if (!t || !res)
        return FTPD_JWN_LINE_NONE;

    t->racf_calls += res->racf_calls;

    if (res->decision == FTPD_JWN_ALLOW) {
        if (res->reason == FTPD_JWN_R_OWN)
            t->own++;
        return FTPD_JWN_LINE_NONE;
    }

    if (res->reason == FTPD_JWN_R_NOACEE) {
        t->hidden++;
        if (t->noacee_seen)
            return FTPD_JWN_LINE_NONE;
        t->noacee_seen = 1;
        return FTPD_JWN_LINE_NOACEE;
    }

    if (res->reason == FTPD_JWN_R_ERR_RC ||
        res->reason == FTPD_JWN_R_ERR_COMPONENT) {
        t->err++;
        if (t->err > FTPD_JWN_ERRLINES) {
            t->err_more++;
            return FTPD_JWN_LINE_NONE;
        }
        if (op == FTPD_JWN_OP_LIST || !st)
            return FTPD_JWN_LINE_ERROR;
        st->err++;
        if (st->err <= FTPD_JWN_ERRSESSMAX)
            return FTPD_JWN_LINE_ERROR;
        st->err_more++;
        return FTPD_JWN_LINE_NONE;
    }

    if (res->decision == FTPD_JWN_HIDE)
        t->hidden++;

    if (op == FTPD_JWN_OP_LIST || !st)
        return FTPD_JWN_LINE_NONE;

    st->denied++;
    if (st->denied <= FTPD_JWN_WARNMAX)
        return FTPD_JWN_LINE_WARN;
    st->denied_more++;
    return FTPD_JWN_LINE_NONE;
}

int
ftpd_jwn_tally_more(const ftpd_jwn_tally_t *t)
{
    return t ? t->err_more : 0;
}

int
ftpd_jwn_tally_session_more(const ftpd_jwn_sess_t *st)
{
    return st ? st->denied_more : 0;
}

int
ftpd_jwn_tally_session_err_more(const ftpd_jwn_sess_t *st)
{
    return st ? st->err_more : 0;
}
