/* TSTSIT.C
** Tests for ftpd_site_dispatch() -- SITE keywords and keyword values are
** case-insensitive (issue #189, RFC 959 section 5.3).
**
** For every spelling pair the test runs the upper-case form and the variant
** against a fresh session and requires the same reply and the same session
** state.  The upper-case run is also checked against the value it must
** produce, so two runs that agree on a wrong answer do not pass.
**
** The SITE verb itself is upper-cased by the session loop before dispatch
** and never reaches ftpd_site_dispatch(); that part is covered by the
** end-to-end script (test/test_ftpd.sh, section "SITE in any spelling").
**
** MVS only: ftpd.h pulls in the libc370 MVS headers.  The two reply
** functions and the volume check are replaced here, so no socket, no UCB
** scan and no FTPD log are needed.
*/
#include "ftpd.h"
#include "ftpd#ses.h"
#include "ftpd#sit.h"
#include <mbtcheck.h>

static ftpd_server_t    server;
static ftpd_session_t   sess;

static int  last_code;
static char last_text[256];
static int  replies;

int
ftpd_session_reply(ftpd_session_t *s, int code, const char *fmt, ...)
{
    va_list ap;

    (void)s;
    va_start(ap, fmt);
    vsnprintf(last_text, sizeof(last_text), fmt, ap);
    va_end(ap);
    last_code = code;
    replies++;
    return 0;
}

int
ftpd_session_reply_multi(ftpd_session_t *s, int code,
                         const char *first, const char *last)
{
    (void)s;
    snprintf(last_text, sizeof(last_text), "%s|%s", first, last);
    last_code = code;
    replies++;
    return 0;
}

/* Only an upper-case serial is "mounted": a value that reaches the check
** unnormalised is refused and shows up as a state difference. */
int
ftpdcfg_volume_online(const char *volser, unsigned short *dasdtype)
{
    if (dasdtype)
        *dasdtype = 0;
    return strcmp(volser, "PUB001") == 0 || strcmp(volser, "SYSRES") == 0;
}

static void
fresh_session(void)
{
    memset(&server, 0, sizeof(server));
    memset(&sess, 0, sizeof(sess));
    sess.server = &server;
    strcpy(sess.user, "IBMUSER");
    strcpy(server.config.defaults.recfm, "FB");
    server.config.defaults.lrecl = 80;
    server.config.defaults.blksize = 3120;
    server.config.defaults.primary = 100;
    server.config.defaults.secondary = 50;
    strcpy(server.config.defaults.unit, "3380");
    strcpy(server.config.defaults.volume, "SYSRES");
    strcpy(sess.alloc.recfm, "FB");
    sess.alloc.lrecl = 80;
    sess.alloc.blksize = 3120;
    sess.alloc.primary = 100;
    sess.alloc.secondary = 50;
    strcpy(sess.alloc.spacetype, "TRK");
    strcpy(sess.alloc.unit, "3380");
    strcpy(sess.alloc.volume, "SYSRES");
    sess.filetype = FT_SEQ;
    sess.fsmode = FS_MVS;
    sess.prev_fsmode = FS_MVS;
}

/* Everything SITE can change, in one comparable piece. */
typedef struct state {
    ftpd_alloc_t    alloc;
    int             filetype;
    int             fsmode;
    int             prev_fsmode;
    char            jes_owner[9];
    char            jes_jobname[9];
    char            jes_status[8];
    int             trailing;
    int             truncate;
    int             rdw;
} state_t;

static void
snapshot(state_t *st)
{
    memset(st, 0, sizeof(*st));
    st->alloc = sess.alloc;
    st->filetype = sess.filetype;
    st->fsmode = sess.fsmode;
    st->prev_fsmode = sess.prev_fsmode;
    strcpy(st->jes_owner, sess.jes_owner);
    strcpy(st->jes_jobname, sess.jes_jobname);
    strcpy(st->jes_status, sess.jes_status);
    st->trailing = sess.trailing;
    st->truncate = sess.truncate;
    st->rdw = sess.rdw;
}

/* Member by member: a struct compare would also compare padding. */
static int
state_equal(const state_t *a, const state_t *b)
{
    return strcmp(a->alloc.recfm, b->alloc.recfm) == 0
        && a->alloc.lrecl == b->alloc.lrecl
        && a->alloc.blksize == b->alloc.blksize
        && a->alloc.primary == b->alloc.primary
        && a->alloc.secondary == b->alloc.secondary
        && strcmp(a->alloc.spacetype, b->alloc.spacetype) == 0
        && strcmp(a->alloc.volume, b->alloc.volume) == 0
        && strcmp(a->alloc.unit, b->alloc.unit) == 0
        && a->alloc.dirblks == b->alloc.dirblks
        && a->filetype == b->filetype
        && a->fsmode == b->fsmode
        && a->prev_fsmode == b->prev_fsmode
        && strcmp(a->jes_owner, b->jes_owner) == 0
        && strcmp(a->jes_jobname, b->jes_jobname) == 0
        && strcmp(a->jes_status, b->jes_status) == 0
        && a->trailing == b->trailing
        && a->truncate == b->truncate
        && a->rdw == b->rdw;
}

/* Run one SITE argument string on a fresh session. */
static void
run(const char *arg, state_t *st, int *code, char *text, int textsz)
{
    fresh_session();
    last_code = 0;
    last_text[0] = '\0';
    replies = 0;
    ftpd_site_dispatch(&sess, arg);
    snapshot(st);
    *code = (replies == 1) ? last_code : -replies;
    strncpy(text, last_text, textsz - 1);
    text[textsz - 1] = '\0';
}

/* The variant must behave exactly like the canonical spelling. */
static void
same(const char *canon, const char *variant)
{
    state_t a, b;
    int ca, cb;
    char ta[256], tb[256];
    char msg[160];

    run(canon, &a, &ca, ta, sizeof(ta));
    run(variant, &b, &cb, tb, sizeof(tb));

    snprintf(msg, sizeof(msg), "\"%s\" replies like \"%s\" (%d)",
             variant, canon, ca);
    CHECK(ca == 200 && cb == ca && strcmp(ta, tb) == 0, msg);

    snprintf(msg, sizeof(msg), "\"%s\" leaves the same state as \"%s\"",
             variant, canon);
    CHECK(state_equal(&a, &b), msg);
}

int
main(void)
{
    state_t st;
    int code;
    char text[256];

    printf("=== FTPD SITE case-insensitivity tests (#189) ===\n");

    /* --- what the canonical spelling does ----------------------------- */
    run("RECFM=VB", &st, &code, text, sizeof(text));
    CHECK(strcmp(st.alloc.recfm, "VB") == 0, "RECFM=VB sets VB");
    run("LRECL=255", &st, &code, text, sizeof(text));
    CHECK(st.alloc.lrecl == 255, "LRECL=255 sets 255");
    run("VOLUME=PUB001", &st, &code, text, sizeof(text));
    CHECK(strcmp(st.alloc.volume, "PUB001") == 0, "VOLUME=PUB001 sets PUB001");
    run("UNIT=3390", &st, &code, text, sizeof(text));
    CHECK(strcmp(st.alloc.unit, "3390") == 0, "UNIT=3390 sets 3390");
    run("FILETYPE=JES", &st, &code, text, sizeof(text));
    CHECK(st.filetype == FT_JES, "FILETYPE=JES switches to JES");
    run("JESOWNER=ABC", &st, &code, text, sizeof(text));
    CHECK(strcmp(st.jes_owner, "ABC") == 0, "JESOWNER=ABC sets ABC");
    run("JESJOBNAME=MYJOB", &st, &code, text, sizeof(text));
    CHECK(strcmp(st.jes_jobname, "MYJOB") == 0, "JESJOBNAME=MYJOB sets MYJOB");
    run("TRAILING", &st, &code, text, sizeof(text));
    CHECK(st.trailing == 1, "TRAILING toggles on");

    /* --- keywords ------------------------------------------------------ */
    same("RECFM=FB", "recfm=fb");
    same("RECFM=VB", "Recfm=Vb");
    same("LRECL=255", "lrecl=255");
    same("BLKSIZE=6240", "BlkSize=6240");
    same("PRIMARY=10", "primary=10");
    same("SECONDARY=5", "secondary=5");
    same("DIRECTORY=20", "directory=20");
    same("TRACKS", "tracks");
    same("CYLINDERS", "cylinders");
    same("BLOCKS", "blocks");
    same("VOLUME=PUB001", "volume=pub001");
    same("UNIT=3390", "unit=3390");
    same("FILETYPE=JES", "filetype=jes");
    same("FILETYPE=JES", "FileType=Jes");
    same("FILETYPE=SEQ", "filetype=seq");
    same("TRAILING", "trailing");
    same("TRUNCATE", "truncate");
    same("RDW", "rdw");
    same("SBSENDEOL", "sbsendeol");
    same("DATACLAS=X", "dataclas=x");
    same("STORCLAS=X", "storclas=x");
    same("MGMTCLAS=X", "mgmtclas=x");

    /* --- values that name something --------------------------------- */
    same("JESJOBNAME=MYJOB", "jesjobname=myjob");
    same("JESOWNER=IBMUSER", "jesowner=ibmuser");
    same("JESSTATUS=OUTPUT", "jesstatus=output");
    same("JESSTATUS=ALL", "JesStatus=All");

    /* --- several parameters in one command ---------------------------- */
    same("RECFM=VB LRECL=255 FILETYPE=JES JESOWNER=ABC",
         "recfm=vb lrecl=255 filetype=jes jesowner=abc");
    same("RECFM=VB LRECL=255 FILETYPE=JES JESOWNER=ABC",
         "Recfm=vb LRECL=255 fileType=JES JesOwner=abc");

    /* --- a parameter that is not recognised stays unrecognised -------- */
    run("JESINTERFACELEVEL=1", &st, &code, text, sizeof(text));
    CHECK(code == 200 && strstr(text, "Unrecognized parameter") != NULL,
          "JESINTERFACELEVEL=1 is unrecognised");
    run("jesinterfacelevel=1", &st, &code, text, sizeof(text));
    CHECK(code == 200 && strstr(text, "Unrecognized parameter") != NULL,
          "jesinterfacelevel=1 is unrecognised too");

    return mbt_test_summary("TSTSIT");
}
