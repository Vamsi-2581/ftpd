#ifndef FTPD_JWN_H
#define FTPD_JWN_H
/*
** FTPD JES spool authorization
**
** Who may see, read and delete which job on the JES spool (#90).  The JES
** mode of LIST, LIST JOBnnnnn, RETR and DELE asks ftpd_jwn_check() once per
** job; SITE JESOWNER asks ftpd_jwn_narrows() and ftpd_jwn_probe().
**
** The model, in the order ftpd_jwn_check() applies it:
**   1. no ACEE or no userid            -> hidden, even the user's own jobs
**   2. the job's owner is the user     -> allowed, no RACF call, no entity
**   3. entity <owner>.<jobname>.<jobid> -> a component outside [A-Z0-9$#@]
**                                          or longer than 8 is an error
**   4. RACF class JESSPOOL, READ (and, for DELE only, UPDATE)
**        no profile, no READ, error  -> hidden
**        READ                        -> list and read, DELE refused
**        UPDATE or better            -> everything
**
** This file and ftpd#jwn.c are free of project and MVS headers so a host unit
** test can link them (test/tstjwn.c).  Nothing here keeps state: counters and
** buffers belong to the caller (FTPD is AC(1), module storage is read-only).
*/

/* Characters a job name, job id or owner may consist of.  Written as a
** string, never as a range: EBCDIC letters are not contiguous. */
#define FTPD_JWN_CHARS "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789$#@"

#define FTPD_JWN_COMPMAX    8       /* longest component                */
#define FTPD_JWN_ENTMAX     26      /* 8 + '.' + 8 + '.' + 8           */
#define FTPD_JWN_ENTSZ      (FTPD_JWN_ENTMAX + 1)
#define FTPD_JWN_KEYLEN     8       /* RAKF compares 8 characters       */
#define FTPD_JWN_ERRLINES   8       /* ERR lines written per command    */
#define FTPD_JWN_WARNMAX    16      /* denied lines written per session */
#define FTPD_JWN_ERRSESSMAX 16      /* ERR lines of RETR, DELE and
                                       LIST JOBnnnnn per session        */
#define FTPD_JWN_PRTSZ      (FTPD_JWN_COMPMAX + 1)
#define FTPD_JWN_CACHEMAX   16      /* cache keys per command           */

/* What the caller is doing.  Only a delete asks for UPDATE. */
#define FTPD_JWN_OP_LIST    0       /* listing of the spool             */
#define FTPD_JWN_OP_LISTJOB 1       /* LIST JOBnnnnn                    */
#define FTPD_JWN_OP_RETR    2
#define FTPD_JWN_OP_DELE    3

/* Access level of a foreign job, lowest first. */
#define FTPD_JWN_LV_ERR       0     /* unusable answer or entity        */
#define FTPD_JWN_LV_NOPROFILE 1     /* the class has no profile for it  */
#define FTPD_JWN_LV_NONE      2     /* a profile matches, no READ       */
#define FTPD_JWN_LV_READ      3
#define FTPD_JWN_LV_UPDATE    4     /* UPDATE, CONTROL or ALTER         */

/* Decision.  HIDE is 0 so that a cleared result is a closed one. */
#define FTPD_JWN_HIDE       0
#define FTPD_JWN_ALLOW      1
#define FTPD_JWN_NOCANCEL   2       /* visible, but not deletable       */

/* Why. */
#define FTPD_JWN_R_ERR_RC       0   /* bad or missing RACF answer       */
#define FTPD_JWN_R_OWN          1
#define FTPD_JWN_R_NOACEE       2
#define FTPD_JWN_R_ERR_COMPONENT 3
#define FTPD_JWN_R_NOPROFILE    4
#define FTPD_JWN_R_NONE         5
#define FTPD_JWN_R_READ         6
#define FTPD_JWN_R_UPDATE       7

/* Answer of SITE JESOWNER= classification. */
#define FTPD_JWN_NR_BAD     0       /* 501, nothing stored              */
#define FTPD_JWN_NR_RESET   1       /* empty: back to the own userid    */
#define FTPD_JWN_NR_OWN     2       /* the own userid                   */
#define FTPD_JWN_NR_ALL     3       /* "*": needs READ on the probe ..  */
#define FTPD_JWN_NR_OTHER   4       /* one foreign userid: probe X..    */

/* What the caller writes after ftpd_jwn_tally(). */
#define FTPD_JWN_LINE_NONE      0
#define FTPD_JWN_LINE_ERROR     1   /* LOG_ERROR ... reason=ERR         */
#define FTPD_JWN_LINE_WARN      2   /* LOG_WARN ... denied              */
#define FTPD_JWN_LINE_NOACEE    3   /* LOG_WARN ... reason=NOACEE       */

/* The one question to RACF: may the user have READ (want_update 0) or
** UPDATE (want_update 1) on JESSPOOL entity?  Answer is racf_auth()'s return
** code.  arg is the caller's (the session's ACEE). */
typedef int (*ftpd_jwn_auth_fn)(void *arg, const char *entity,
                                int want_update);

/* Levels found during one command, by the first FTPD_JWN_KEYLEN characters
** of the entity (what RAKF compares).  A stack variable of the caller; it
** lives one command.  The 17th key is not stored and not evicted for: it is
** asked every time, which is always right and only costs more. */
typedef struct ftpd_jwn_cache {
    int             n;
    struct {
        char        key[FTPD_JWN_KEYLEN + 1];
        char        update;     /* asked for UPDATE as well             */
        char        level;      /* one of the FTPD_JWN_LV values        */
        char        rc;         /* RACF return code the level came from */
    } e[FTPD_JWN_CACHEMAX];
} ftpd_jwn_cache_t;

typedef struct ftpd_jwn_ctx {
    int                 acee_present;   /* the session has an ACEE      */
    const char         *user;           /* sess->user, upper case       */
    ftpd_jwn_auth_fn    auth_fn;        /* NULL: foreign jobs are hidden */
    void               *auth_arg;
    ftpd_jwn_cache_t   *cache;          /* NULL: ask RACF every time    */
} ftpd_jwn_ctx_t;

typedef struct ftpd_jwn_res {
    int decision;       /* HIDE, ALLOW or NOCANCEL, see above           */
    int reason;         /* one of the FTPD_JWN_R reasons                */
    int level;          /* an FTPD_JWN_LV value, foreign jobs only      */
    int rc;             /* last RACF return code, -1 = none called      */
    int elen;           /* length of the entity, 0 = none built         */
    int racf_calls;     /* 0, 1 or 2                                    */
} ftpd_jwn_res_t;

/* Counters of one command (a stack variable of the caller). */
typedef struct ftpd_jwn_tally {
    int own;            /* own jobs allowed                             */
    int hidden;         /* hidden, not ERR (NOPROFILE, NONE, NOACEE)    */
    int err;            /* every ERR, written or not                    */
    int err_more;       /* ERR hits after the first FTPD_JWN_ERRLINES   */
    int racf_calls;
    int noacee_seen;    /* the NOACEE line of this command was asked    */
} ftpd_jwn_tally_t;

/* Counters of a session (a field of struct ftpd_session). */
typedef struct ftpd_jwn_sess {
    int denied;         /* RETR, DELE, LIST JOBnnnnn refusals           */
    int denied_more;    /* refusals after the first FTPD_JWN_WARNMAX    */
    int err;            /* ERR hits of RETR, DELE, LIST JOBnnnnn        */
    int err_more;       /* ERR hits after the first FTPD_JWN_ERRSESSMAX */
} ftpd_jwn_sess_t;

void ftpd_jwn_cache_init(ftpd_jwn_cache_t *c)               asm("FTPJWNCI");

/* 1 if s is 1..8 characters of FTPD_JWN_CHARS, else 0. */
int ftpd_jwn_component_ok(const char *s)                    asm("FTPJWNCO");

/* Builds "<owner>.<jobname>.<jobid>" into out.  Every component is checked
** first; the owner alone may be empty.  Returns the length, -1 for a bad
** component or a buffer smaller than the entity needs. */
int ftpd_jwn_entity(char *out, int sz, const char *owner,
                    const char *jobname, const char *jobid)
                                                            asm("FTPJWNEN");

/* The SITE JESOWNER probe: "X.." for a userid, ".." for "" (any owner).
** Returns the length, -1 for a bad userid or a small buffer. */
int ftpd_jwn_probe(char *out, int sz, const char *userid)
                                                            asm("FTPJWNPR");

/* RACF return codes to level (0/4/8 only, anything else is LV_ERR).  Asks
** READ, and UPDATE when want_update and READ gave 0.  Sets rc and
** racf_calls of res. */
int ftpd_jwn_level(const ftpd_jwn_ctx_t *ctx, const char *entity,
                   int want_update, ftpd_jwn_res_t *res)
                                                            asm("FTPJWNLV");

/* 1 if owner and user are the same non-empty userid, whole length, no
** blank or control character in either.  MIKE1 is not MIKE10. */
int ftpd_jwn_mine(const char *owner, const char *user)      asm("FTPJWNMN");

/* The decision for a foreign job at a level.  Sets res->decision. */
int ftpd_jwn_decide(int op, int level)                      asm("FTPJWNDC");

/* The whole decision; see the top of this file.  res is set closed first.
** Returns res->decision. */
int ftpd_jwn_check(const ftpd_jwn_ctx_t *ctx, int op, const char *owner,
                   const char *jobname, const char *jobid,
                   ftpd_jwn_res_t *res)                     asm("FTPJWNCK");

/* Does the display filter (SITE JESOWNER) pattern show this owner?  An exact
** owner or "*"; no prefix. */
int ftpd_jwn_match(const char *pattern, const char *owner)  asm("FTPJWNMT");

/* Classifies a SITE JESOWNER= value, whole value, any case.  canon (at least
** 9 bytes) receives the upper-cased value for RESET, OWN, ALL and OTHER, ""
** otherwise.  Returns FTPD_JWN_NR_*. */
int ftpd_jwn_narrows(const char *user, const char *value,
                     char *canon, int sz)                   asm("FTPJWNNR");

/* Copies value for output: only FTPD_JWN_CHARS stay, anything else becomes
** '?', at most 8 characters, always NUL terminated (nothing written for
** sz < 1). */
void ftpd_jwn_printable(char *out, int sz, const char *value)
                                                            asm("FTPJWNPT");

void ftpd_jwn_tally_init(ftpd_jwn_tally_t *t)               asm("FTPJWNTI");

/* Counts the outcome of one job and says which line to write. */
int ftpd_jwn_tally(ftpd_jwn_tally_t *t, ftpd_jwn_sess_t *st, int op,
                   const ftpd_jwn_res_t *res)               asm("FTPJWNTL");

/* ERR hits not written one by one in this command (0 = no summary line). */
int ftpd_jwn_tally_more(const ftpd_jwn_tally_t *t)          asm("FTPJWNTM");

/* Refusals not written in this session (0 = no summary line). */
int ftpd_jwn_tally_session_more(const ftpd_jwn_sess_t *st)  asm("FTPJWNTS");

/* ERR lines not written in this session (0 = no summary line). */
int ftpd_jwn_tally_session_err_more(const ftpd_jwn_sess_t *st)
                                                            asm("FTPJWNTE");

#endif /* FTPD_JWN_H */
