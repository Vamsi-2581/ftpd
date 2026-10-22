/* TSTJWN.C
** Tests for the JES spool authorization of FTPD (#90): components and
** entities, ownership, the RACF return code to level table, the decision
** table in the order ftpd_jwn_check() applies it, the owner display filter,
** SITE JESOWNER classification, output sanitizing and the log counters.
**
** Before #90 the owner filter was a prefix compare (IBMUSER matched
** IBMUSER2) and every job of the spool was handed to any user.
**
** ftpd#jwn.c is free of project and MVS headers, so this is a DUAL test.
** RACF is a stub that answers what the test sets and counts its calls.
*/
#include <stdio.h>
#include <string.h>

#include "ftpd#jwn.h"
#include <mbtcheck.h>

typedef struct stub {
    int rc_read;
    int rc_update;
    int calls;
    int update_calls;
    char entity[40];
} stub_t;

static int
stub_auth(void *arg, const char *entity, int want_update)
{
    stub_t *s = (stub_t *)arg;

    s->calls++;
    if (strlen(entity) < sizeof(s->entity))
        strcpy(s->entity, entity);
    if (want_update) {
        s->update_calls++;
        return s->rc_update;
    }
    return s->rc_read;
}

static void
setup(ftpd_jwn_ctx_t *ctx, stub_t *s, const char *user, int rc_read,
      int rc_update)
{
    memset(s, 0, sizeof(*s));
    s->rc_read = rc_read;
    s->rc_update = rc_update;
    ctx->acee_present = 1;
    ctx->user = user;
    ctx->auth_fn = stub_auth;
    ctx->auth_arg = s;
    ctx->cache = NULL;
}

static int
is(const char *got, const char *want)
{
    return got != NULL && strcmp(got, want) == 0;
}

/* One decision: returns 1 if everything the table says holds. */
static int
decided(const ftpd_jwn_ctx_t *ctx, int op, const char *owner, int decision,
        int reason, int calls, const stub_t *s)
{
    ftpd_jwn_res_t res;

    ftpd_jwn_check(ctx, op, owner, "JOBA", "JOB00001", &res);
    return res.decision == decision && res.reason == reason &&
           res.racf_calls == calls && s->calls == calls;
}

static void
test_components(void)
{
    char out[FTPD_JWN_ENTSZ];
    char small[8];
    int i;

    CHECK(ftpd_jwn_component_ok("IBMUSER") == 1, "7 characters");
    CHECK(ftpd_jwn_component_ok("A") == 1, "1 character");
    CHECK(ftpd_jwn_component_ok("ABCDEFGH") == 1, "8 characters");
    CHECK(ftpd_jwn_component_ok("$#@A0Z9") == 1, "national characters");
    CHECK(ftpd_jwn_component_ok("") == 0, "empty");
    CHECK(ftpd_jwn_component_ok(NULL) == 0, "NULL");
    CHECK(ftpd_jwn_component_ok("ABCDEFGHI") == 0, "9 characters");
    CHECK(ftpd_jwn_component_ok("A.B") == 0, "dot");
    CHECK(ftpd_jwn_component_ok("*") == 0, "star");
    CHECK(ftpd_jwn_component_ok("AB%") == 0, "percent");
    CHECK(ftpd_jwn_component_ok("AB C") == 0, "blank");
    CHECK(ftpd_jwn_component_ok("ABC ") == 0, "trailing blank");
    CHECK(ftpd_jwn_component_ok("abc") == 0, "lower case");
    CHECK(ftpd_jwn_component_ok("A\rB") == 0, "carriage return");
    CHECK(ftpd_jwn_component_ok("A\nB") == 0, "line feed");
    CHECK(ftpd_jwn_component_ok("A\033B") == 0, "escape");

    /* a run through the whole alphabet, written out, not as a range */
    CHECK(ftpd_jwn_component_ok("ABCDEFGH") && ftpd_jwn_component_ok("IJKLMNOP")
          && ftpd_jwn_component_ok("QRSTUVWX") && ftpd_jwn_component_ok("YZ01234")
          && ftpd_jwn_component_ok("56789"), "all letters and digits");
    for (i = 1; i < 256; i++) {
        char c[2];

        c[0] = (char)i;
        c[1] = '\0';
        if (strchr(FTPD_JWN_CHARS, c[0]) == NULL &&
            ftpd_jwn_component_ok(c)) {
            CHECK(0, "a byte outside the set passed");
            break;
        }
    }

    CHECK(ftpd_jwn_entity(out, sizeof(out), "ABC", "JOBA", "JOB00001") == 17
          && is(out, "ABC.JOBA.JOB00001"), "owner of 3");
    CHECK(ftpd_jwn_entity(out, sizeof(out), "ABCDEFG", "JOBA", "JOB00001")
          == 21 && is(out, "ABCDEFG.JOBA.JOB00001"), "owner of 7");
    CHECK(ftpd_jwn_entity(out, sizeof(out), "ABCDEFGH", "JOBNAME8", "JOB00001")
          == 26 && is(out, "ABCDEFGH.JOBNAME8.JOB00001"),
          "the longest entity fits");
    CHECK(ftpd_jwn_entity(out, sizeof(out), "A$#", "J@B", "STC00001") == 16
          && is(out, "A$#.J@B.STC00001"), "national characters");
    CHECK(ftpd_jwn_entity(out, sizeof(out), "", "JOBN", "JOB00001") == 14
          && is(out, ".JOBN.JOB00001"), "the owner alone may be empty");
    CHECK(ftpd_jwn_entity(out, sizeof(out), "ABC", "", "JOB00001") < 0,
          "an empty job name is an error");
    CHECK(ftpd_jwn_entity(out, sizeof(out), "ABC", "JOBN", "") < 0,
          "an empty job id is an error");
    CHECK(ftpd_jwn_entity(out, sizeof(out), "A.B", "JOBN", "JOB00001") < 0,
          "dot in the owner");
    CHECK(ftpd_jwn_entity(out, sizeof(out), "ABC", "J*", "JOB00001") < 0,
          "star in the job name");
    CHECK(ftpd_jwn_entity(out, sizeof(out), "ABC", "JOBN", "JOB%0001") < 0,
          "percent in the job id");
    CHECK(ftpd_jwn_entity(out, sizeof(out), "abc", "JOBN", "JOB00001") < 0,
          "lower case owner");
    CHECK(ftpd_jwn_entity(out, sizeof(out), "ABCDEFGHI", "JOBN", "JOB00001")
          < 0, "owner of 9");
    CHECK(ftpd_jwn_entity(out, sizeof(out), "ABC", "JOBNAMEXX", "JOB00001")
          < 0, "job name of 9");
    CHECK(ftpd_jwn_entity(out, sizeof(out), "ABC", "JOBN", "JOB000012") < 0,
          "job id of 9");
    CHECK(ftpd_jwn_entity(out, sizeof(out), "ABC     ", "JOBN", "JOB00001")
          < 0, "a blank padded owner is an error, not trimmed");
    CHECK(ftpd_jwn_entity(out, sizeof(out), " ", "JOBN", "JOB00001") < 0,
          "an owner of one blank is an error");
    CHECK(ftpd_jwn_entity(small, sizeof(small), "ABC", "JOBN", "JOB00001")
          < 0, "a buffer too small for the entity");
    CHECK(ftpd_jwn_entity(out, 17, "ABC", "JOBA", "JOB00001") < 0,
          "no room for the terminator");
    CHECK(ftpd_jwn_entity(out, 18, "ABC", "JOBA", "JOB00001") == 17,
          "room for the terminator");
    CHECK(ftpd_jwn_entity(out, sizeof(out), NULL, "JOBN", "JOB00001") < 0,
          "NULL owner");

    CHECK(ftpd_jwn_probe(out, sizeof(out), "ABC") == 5 && is(out, "ABC.."),
          "probe of a userid");
    CHECK(ftpd_jwn_probe(out, sizeof(out), "") == 2 && is(out, ".."),
          "probe of everybody");
    CHECK(ftpd_jwn_probe(out, sizeof(out), NULL) == 2 && is(out, ".."),
          "probe of NULL");
    CHECK(ftpd_jwn_probe(out, sizeof(out), "A.B") < 0, "probe of a bad userid");
    CHECK(ftpd_jwn_probe(small, 4, "ABC") < 0, "probe buffer too small");
    CHECK(ftpd_jwn_probe(small, 6, "ABC") == 5, "probe buffer just fits");
}

static void
test_mine(void)
{
    CHECK(ftpd_jwn_mine("MIKE1", "MIKE1") == 1, "same userid");
    CHECK(ftpd_jwn_mine("MIKE1", "MIKE10") == 0, "MIKE1 is not MIKE10");
    CHECK(ftpd_jwn_mine("MIKE10", "MIKE1") == 0, "MIKE10 is not MIKE1");
    CHECK(ftpd_jwn_mine("IBMUSER2", "IBMUSER") == 0,
          "IBMUSER2 is not IBMUSER");
    CHECK(ftpd_jwn_mine("IBMUSER", "IBMUSER2") == 0,
          "IBMUSER is not IBMUSER2");
    CHECK(ftpd_jwn_mine("", "MIKE1") == 0, "no owner is nobody's");
    CHECK(ftpd_jwn_mine("MIKE1", "") == 0, "no user owns nothing");
    CHECK(ftpd_jwn_mine("", "") == 0, "nothing equals nothing");
    CHECK(ftpd_jwn_mine(NULL, "MIKE1") == 0, "NULL owner");
    CHECK(ftpd_jwn_mine("MIKE1", NULL) == 0, "NULL user");
    CHECK(ftpd_jwn_mine("mike1", "MIKE1") == 0, "lower case owner");
    CHECK(ftpd_jwn_mine("MIKE1   ", "MIKE1   ") == 0,
          "blank padded owner and user");
    CHECK(ftpd_jwn_mine("MIKE1   ", "MIKE1") == 0, "blank padded owner");
    CHECK(ftpd_jwn_mine("MIKE1", "MIKE1   ") == 0, "blank padded user");
    CHECK(ftpd_jwn_mine("MIKE1\001", "MIKE1\001") == 0,
          "control character in both");
}

static void
test_level(void)
{
    ftpd_jwn_ctx_t ctx;
    ftpd_jwn_res_t res;
    stub_t s;

    setup(&ctx, &s, "ME", 0, 0);
    CHECK(ftpd_jwn_level(&ctx, "A.B.C", 0, &res) == FTPD_JWN_LV_READ &&
          res.rc == 0 && res.racf_calls == 1 && s.calls == 1,
          "READ 0 is READ and no more is asked");
    CHECK(is(s.entity, "A.B.C"), "the stub got the entity");

    setup(&ctx, &s, "ME", 0, 0);
    CHECK(ftpd_jwn_level(&ctx, "A.B.C", 1, &res) == FTPD_JWN_LV_UPDATE &&
          res.rc == 0 && res.racf_calls == 2 && s.update_calls == 1,
          "READ 0, UPDATE 0 is UPDATE");

    setup(&ctx, &s, "ME", 0, 8);
    CHECK(ftpd_jwn_level(&ctx, "A.B.C", 1, &res) == FTPD_JWN_LV_READ &&
          res.rc == 8 && res.racf_calls == 2, "READ 0, UPDATE 8 is READ");

    setup(&ctx, &s, "ME", 0, 4);
    CHECK(ftpd_jwn_level(&ctx, "A.B.C", 1, &res) == FTPD_JWN_LV_ERR,
          "UPDATE 4 after READ 0 contradicts: ERR");

    setup(&ctx, &s, "ME", 0, 12);
    CHECK(ftpd_jwn_level(&ctx, "A.B.C", 1, &res) == FTPD_JWN_LV_ERR,
          "UPDATE 12: ERR");

    setup(&ctx, &s, "ME", 4, 0);
    CHECK(ftpd_jwn_level(&ctx, "A.B.C", 1, &res) == FTPD_JWN_LV_NOPROFILE &&
          res.racf_calls == 1 && s.update_calls == 0,
          "READ 4 is NOPROFILE and UPDATE is not asked");

    setup(&ctx, &s, "ME", 8, 0);
    CHECK(ftpd_jwn_level(&ctx, "A.B.C", 1, &res) == FTPD_JWN_LV_NONE &&
          res.racf_calls == 1 && s.update_calls == 0,
          "READ 8 is NONE and UPDATE is not asked");

    {
        static const int bad[] = { 1, 2, 3, 5, 6, 7, 9, 10, 11, 12, 16, 20,
                                   -1, -4, 255, 1000 };
        size_t i;
        int ok = 1;

        for (i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
            setup(&ctx, &s, "ME", bad[i], 0);
            if (ftpd_jwn_level(&ctx, "A.B.C", 0, &res) != FTPD_JWN_LV_ERR ||
                res.rc != bad[i])
                ok = 0;
        }
        CHECK(ok, "every READ code but 0, 4 and 8 is ERR");
    }

    ctx.auth_fn = NULL;
    CHECK(ftpd_jwn_level(&ctx, "A.B.C", 0, &res) == FTPD_JWN_LV_ERR &&
          res.racf_calls == 0 && res.rc == -1, "no checker: ERR, no call");

    CHECK(ftpd_jwn_decide(FTPD_JWN_OP_RETR, FTPD_JWN_LV_ERR) == FTPD_JWN_HIDE,
          "ERR hides");
    CHECK(ftpd_jwn_decide(FTPD_JWN_OP_RETR, FTPD_JWN_LV_NOPROFILE)
          == FTPD_JWN_HIDE, "NOPROFILE hides");
    CHECK(ftpd_jwn_decide(FTPD_JWN_OP_RETR, FTPD_JWN_LV_NONE)
          == FTPD_JWN_HIDE, "NONE hides");
    CHECK(ftpd_jwn_decide(FTPD_JWN_OP_LIST, FTPD_JWN_LV_READ)
          == FTPD_JWN_ALLOW, "READ lists");
    CHECK(ftpd_jwn_decide(FTPD_JWN_OP_RETR, FTPD_JWN_LV_READ)
          == FTPD_JWN_ALLOW, "READ reads");
    CHECK(ftpd_jwn_decide(FTPD_JWN_OP_DELE, FTPD_JWN_LV_READ)
          == FTPD_JWN_NOCANCEL, "READ does not delete");
    CHECK(ftpd_jwn_decide(FTPD_JWN_OP_DELE, FTPD_JWN_LV_UPDATE)
          == FTPD_JWN_ALLOW, "UPDATE deletes");
    CHECK(ftpd_jwn_decide(FTPD_JWN_OP_LIST, 99) == FTPD_JWN_HIDE,
          "an unknown level hides");
}

static void
test_check(void)
{
    ftpd_jwn_ctx_t ctx;
    ftpd_jwn_res_t res;
    stub_t s;
    int op;
    static const int ops[] = { FTPD_JWN_OP_LIST, FTPD_JWN_OP_LISTJOB,
                               FTPD_JWN_OP_RETR, FTPD_JWN_OP_DELE };
    int ok;
    size_t i;

    /* own job: ALLOW/OWN for every answer RACF could give, no call */
    {
        static const int rcs[] = { 0, 4, 8, 12, 5, -1 };
        size_t r;

        ok = 1;
        for (r = 0; r < sizeof(rcs) / sizeof(rcs[0]); r++) {
            for (i = 0; i < sizeof(ops) / sizeof(ops[0]); i++) {
                setup(&ctx, &s, "MIKE1", rcs[r], rcs[r]);
                if (!decided(&ctx, ops[i], "MIKE1", FTPD_JWN_ALLOW,
                             FTPD_JWN_R_OWN, 0, &s))
                    ok = 0;
            }
        }
        CHECK(ok, "own job: ALLOW/OWN whatever RACF says, no call");
    }

    /* M2 comes before ownership */
    setup(&ctx, &s, "MIKE1", 0, 0);
    ctx.acee_present = 0;
    CHECK(decided(&ctx, FTPD_JWN_OP_LIST, "MIKE1", FTPD_JWN_HIDE,
                  FTPD_JWN_R_NOACEE, 0, &s), "no ACEE hides an own job");
    CHECK(decided(&ctx, FTPD_JWN_OP_LIST, "OTHER", FTPD_JWN_HIDE,
                  FTPD_JWN_R_NOACEE, 0, &s), "no ACEE hides a foreign job");
    setup(&ctx, &s, "", 0, 0);
    CHECK(decided(&ctx, FTPD_JWN_OP_RETR, "", FTPD_JWN_HIDE,
                  FTPD_JWN_R_NOACEE, 0, &s),
          "an empty user hides, also an ownerless job");
    setup(&ctx, &s, NULL, 0, 0);
    CHECK(decided(&ctx, FTPD_JWN_OP_RETR, "X", FTPD_JWN_HIDE,
                  FTPD_JWN_R_NOACEE, 0, &s), "no user hides");
    setup(&ctx, &s, "MIKE1", 0, 0);
    ctx.acee_present = 0;
    ctx.auth_fn = NULL;
    CHECK(ftpd_jwn_check(&ctx, FTPD_JWN_OP_LIST, "MIKE1", "JOBA",
                         "JOB00001", &res) == FTPD_JWN_HIDE &&
          res.reason == FTPD_JWN_R_NOACEE,
          "NOACEE needs no checker");

    /* own job needs no checker, no entity: names that are no components */
    setup(&ctx, &s, "MIKE1", 12, 12);
    ctx.auth_fn = NULL;
    CHECK(ftpd_jwn_check(&ctx, FTPD_JWN_OP_DELE, "MIKE1", "a.b", "JOB*",
                         &res) == FTPD_JWN_ALLOW &&
          res.reason == FTPD_JWN_R_OWN && res.elen == 0 &&
          res.racf_calls == 0 && res.rc == -1,
          "own job with job name a.b and job id JOB*: ALLOW/OWN, no entity");
    setup(&ctx, &s, "MIKE1", 12, 12);
    CHECK(ftpd_jwn_check(&ctx, FTPD_JWN_OP_LIST, "MIKE1", "JOBA", "",
                         &res) == FTPD_JWN_ALLOW &&
          res.reason == FTPD_JWN_R_OWN && s.calls == 0,
          "own job with an empty job id: ALLOW/OWN");

    /* foreign: the table */
    {
        struct {
            int rc_read, rc_update, op, decision, reason, level, calls;
        } t[] = {
            { 4, 0, FTPD_JWN_OP_LIST, FTPD_JWN_HIDE, FTPD_JWN_R_NOPROFILE,
              FTPD_JWN_LV_NOPROFILE, 1 },
            { 4, 0, FTPD_JWN_OP_RETR, FTPD_JWN_HIDE, FTPD_JWN_R_NOPROFILE,
              FTPD_JWN_LV_NOPROFILE, 1 },
            { 4, 0, FTPD_JWN_OP_DELE, FTPD_JWN_HIDE, FTPD_JWN_R_NOPROFILE,
              FTPD_JWN_LV_NOPROFILE, 1 },
            { 8, 0, FTPD_JWN_OP_LIST, FTPD_JWN_HIDE, FTPD_JWN_R_NONE,
              FTPD_JWN_LV_NONE, 1 },
            { 8, 0, FTPD_JWN_OP_LISTJOB, FTPD_JWN_HIDE, FTPD_JWN_R_NONE,
              FTPD_JWN_LV_NONE, 1 },
            { 8, 0, FTPD_JWN_OP_DELE, FTPD_JWN_HIDE, FTPD_JWN_R_NONE,
              FTPD_JWN_LV_NONE, 1 },
            { 0, 8, FTPD_JWN_OP_LIST, FTPD_JWN_ALLOW, FTPD_JWN_R_READ,
              FTPD_JWN_LV_READ, 1 },
            { 0, 8, FTPD_JWN_OP_LISTJOB, FTPD_JWN_ALLOW, FTPD_JWN_R_READ,
              FTPD_JWN_LV_READ, 1 },
            { 0, 8, FTPD_JWN_OP_RETR, FTPD_JWN_ALLOW, FTPD_JWN_R_READ,
              FTPD_JWN_LV_READ, 1 },
            { 0, 8, FTPD_JWN_OP_DELE, FTPD_JWN_NOCANCEL, FTPD_JWN_R_READ,
              FTPD_JWN_LV_READ, 2 },
            { 0, 0, FTPD_JWN_OP_LIST, FTPD_JWN_ALLOW, FTPD_JWN_R_READ,
              FTPD_JWN_LV_READ, 1 },
            { 0, 0, FTPD_JWN_OP_RETR, FTPD_JWN_ALLOW, FTPD_JWN_R_READ,
              FTPD_JWN_LV_READ, 1 },
            { 0, 0, FTPD_JWN_OP_DELE, FTPD_JWN_ALLOW, FTPD_JWN_R_UPDATE,
              FTPD_JWN_LV_UPDATE, 2 },
            { 12, 0, FTPD_JWN_OP_LIST, FTPD_JWN_HIDE, FTPD_JWN_R_ERR_RC,
              FTPD_JWN_LV_ERR, 1 },
            { 12, 0, FTPD_JWN_OP_DELE, FTPD_JWN_HIDE, FTPD_JWN_R_ERR_RC,
              FTPD_JWN_LV_ERR, 1 },
            { 5, 0, FTPD_JWN_OP_RETR, FTPD_JWN_HIDE, FTPD_JWN_R_ERR_RC,
              FTPD_JWN_LV_ERR, 1 },
            { -1, 0, FTPD_JWN_OP_RETR, FTPD_JWN_HIDE, FTPD_JWN_R_ERR_RC,
              FTPD_JWN_LV_ERR, 1 },
            { 0, 4, FTPD_JWN_OP_DELE, FTPD_JWN_HIDE, FTPD_JWN_R_ERR_RC,
              FTPD_JWN_LV_ERR, 2 },
        };
        char msg[64];

        for (i = 0; i < sizeof(t) / sizeof(t[0]); i++) {
            setup(&ctx, &s, "MIKE1", t[i].rc_read, t[i].rc_update);
            ftpd_jwn_check(&ctx, t[i].op, "ABC", "JOBA", "JOB00001", &res);
            sprintf(msg, "foreign row %d", (int)i);
            CHECK(res.decision == t[i].decision && res.reason == t[i].reason
                  && res.level == t[i].level
                  && res.racf_calls == t[i].calls && s.calls == t[i].calls
                  && res.elen == 17 && is(s.entity, "ABC.JOBA.JOB00001"),
                  msg);
        }
    }

    /* an ownerless job is foreign, with the entity .JOBN.JOB00001 */
    setup(&ctx, &s, "MIKE1", 0, 0);
    ftpd_jwn_check(&ctx, FTPD_JWN_OP_RETR, "", "JOBN", "JOB00001", &res);
    CHECK(res.decision == FTPD_JWN_ALLOW && res.reason == FTPD_JWN_R_READ &&
          is(s.entity, ".JOBN.JOB00001") && res.elen == 14,
          "an ownerless job is checked as .JOBN.JOB00001");
    setup(&ctx, &s, "MIKE1", 8, 0);
    CHECK(ftpd_jwn_check(&ctx, FTPD_JWN_OP_RETR, "", "JOBN", "JOB00001",
                         &res) == FTPD_JWN_HIDE && res.reason == FTPD_JWN_R_NONE,
          "an ownerless job without READ on * is hidden");
    setup(&ctx, &s, "MIKE1", 0, 0);
    CHECK(ftpd_jwn_check(&ctx, FTPD_JWN_OP_RETR, NULL, "JOBN", "JOB00001",
                         &res) == FTPD_JWN_ALLOW && s.calls == 1,
          "a NULL owner is an ownerless job");

    /* entity errors come before the stub */
    {
        static const char *const owners[] = { "A.B", "*", "AB%", "abc",
            "ABCDEFGHI", "ABC     ", " ", "A\rB" };
        static const char *const names[] = { "A.B", "J*", "", "jobn",
            "JOBNAMEXX", "JOB N" };
        ok = 1;
        for (i = 0; i < sizeof(owners) / sizeof(owners[0]); i++) {
            setup(&ctx, &s, "MIKE1", 0, 0);
            ftpd_jwn_check(&ctx, FTPD_JWN_OP_RETR, owners[i], "JOBN",
                           "JOB00001", &res);
            if (res.decision != FTPD_JWN_HIDE ||
                res.reason != FTPD_JWN_R_ERR_COMPONENT || s.calls != 0 ||
                res.rc != -1 || res.elen != 0)
                ok = 0;
        }
        for (i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
            setup(&ctx, &s, "MIKE1", 0, 0);
            ftpd_jwn_check(&ctx, FTPD_JWN_OP_RETR, "ABC", names[i],
                           "JOB00001", &res);
            if (res.decision != FTPD_JWN_HIDE ||
                res.reason != FTPD_JWN_R_ERR_COMPONENT || s.calls != 0)
                ok = 0;
            setup(&ctx, &s, "MIKE1", 0, 0);
            ftpd_jwn_check(&ctx, FTPD_JWN_OP_RETR, "ABC", "JOBN", names[i],
                           &res);
            if (res.decision != FTPD_JWN_HIDE ||
                res.reason != FTPD_JWN_R_ERR_COMPONENT || s.calls != 0)
                ok = 0;
        }
        CHECK(ok, "a foreign job with a bad component: HIDE, ERR_COMPONENT, "
                  "rc -1, the stub is not called");
    }

    /* fail closed: a stale ALLOW in res never survives */
    for (op = 0; op < 4; op++) {
        ok = 1;
        res.decision = FTPD_JWN_ALLOW;
        res.reason = FTPD_JWN_R_OWN;
        setup(&ctx, &s, "MIKE1", 12, 12);
        if (ftpd_jwn_check(&ctx, op, "ABC", "JOBA", "JOB00001", &res) !=
                FTPD_JWN_HIDE || res.decision != FTPD_JWN_HIDE ||
                res.reason != FTPD_JWN_R_ERR_RC)
            ok = 0;
        res.decision = FTPD_JWN_ALLOW;
        res.reason = FTPD_JWN_R_OWN;
        if (ftpd_jwn_check(&ctx, op, "A.B", "JOBA", "JOB00001", &res) !=
                FTPD_JWN_HIDE || res.decision != FTPD_JWN_HIDE)
            ok = 0;
        res.decision = FTPD_JWN_ALLOW;
        ctx.acee_present = 0;
        if (ftpd_jwn_check(&ctx, op, "ABC", "JOBA", "JOB00001", &res) !=
                FTPD_JWN_HIDE || res.decision != FTPD_JWN_HIDE)
            ok = 0;
        CHECK(ok, "res starts closed: a stale ALLOW is overwritten");
    }
    CHECK(ftpd_jwn_check(NULL, FTPD_JWN_OP_LIST, "A", "B", "C", &res) ==
          FTPD_JWN_HIDE && res.decision == FTPD_JWN_HIDE, "no context hides");
    CHECK(ftpd_jwn_check(&ctx, FTPD_JWN_OP_LIST, "A", "B", "C", NULL) ==
          FTPD_JWN_HIDE, "no result is not a crash");

    /* foreign job, no checker */
    setup(&ctx, &s, "MIKE1", 0, 0);
    ctx.auth_fn = NULL;
    CHECK(ftpd_jwn_check(&ctx, FTPD_JWN_OP_RETR, "ABC", "JOBA", "JOB00001",
                         &res) == FTPD_JWN_HIDE &&
          res.reason == FTPD_JWN_R_ERR_RC && res.racf_calls == 0,
          "a foreign job without a checker is hidden");
    CHECK(ftpd_jwn_check(&ctx, FTPD_JWN_OP_RETR, "MIKE1", "JOBA", "JOB00001",
                         &res) == FTPD_JWN_ALLOW,
          "an own job needs no checker");

    /* the prefix traps */
    setup(&ctx, &s, "IBMUSER", 8, 8);
    CHECK(decided(&ctx, FTPD_JWN_OP_RETR, "IBMUSER2", FTPD_JWN_HIDE,
                  FTPD_JWN_R_NONE, 1, &s),
          "IBMUSER2 is a foreign job to IBMUSER");
    setup(&ctx, &s, "MIKE1", 8, 8);
    CHECK(decided(&ctx, FTPD_JWN_OP_RETR, "MIKE10", FTPD_JWN_HIDE,
                  FTPD_JWN_R_NONE, 1, &s),
          "MIKE10 is a foreign job to MIKE1");
}

static void
test_match(void)
{
    CHECK(ftpd_jwn_match("IBMUSER", "IBMUSER") == 1, "same owner");
    CHECK(ftpd_jwn_match("IBMUSER", "IBMUSER2") == 0,
          "IBMUSER does not match IBMUSER2");
    CHECK(ftpd_jwn_match("ABC", "ABCD") == 0, "ABC does not match ABCD");
    CHECK(ftpd_jwn_match("ABCD", "ABC") == 0, "ABCD does not match ABC");
    CHECK(ftpd_jwn_match("*", "ABC") == 1, "* matches any owner");
    CHECK(ftpd_jwn_match("*", "") == 1, "* matches no owner");
    CHECK(ftpd_jwn_match("ABC", "") == 0, "an ownerless job matches no name");
    CHECK(ftpd_jwn_match("ABC*", "ABCD") == 0, "ABC* is no prefix pattern");
    CHECK(ftpd_jwn_match("ABC*", "ABC") == 0, "ABC* matches nothing");
    CHECK(ftpd_jwn_match("ABC*", "ABC*") == 0, "not even itself");
    CHECK(ftpd_jwn_match("**", "ABC") == 0, "** is not *");
    CHECK(ftpd_jwn_match("", "ABC") == 0, "an empty pattern shows nothing");
    CHECK(ftpd_jwn_match(NULL, "ABC") == 0, "NULL pattern");
    CHECK(ftpd_jwn_match("ABC", NULL) == 0, "NULL owner");
}

static void
test_narrows(void)
{
    char c[16];

    CHECK(ftpd_jwn_narrows("IBMUSER", "", c, sizeof(c)) == FTPD_JWN_NR_RESET
          && is(c, ""), "empty resets");
    CHECK(ftpd_jwn_narrows("IBMUSER", "IBMUSER", c, sizeof(c)) ==
          FTPD_JWN_NR_OWN && is(c, "IBMUSER"), "the own userid");
    CHECK(ftpd_jwn_narrows("IBMUSER", "ibmuser", c, sizeof(c)) ==
          FTPD_JWN_NR_OWN && is(c, "IBMUSER"), "lower case, as IBMUSER");
    CHECK(ftpd_jwn_narrows("IBMUSER", "*", c, sizeof(c)) == FTPD_JWN_NR_ALL
          && is(c, "*"), "*");
    CHECK(ftpd_jwn_narrows("IBMUSER", "ABC", c, sizeof(c)) ==
          FTPD_JWN_NR_OTHER && is(c, "ABC"), "a foreign userid");
    CHECK(ftpd_jwn_narrows("IBMUSER", "abc", c, sizeof(c)) ==
          FTPD_JWN_NR_OTHER && is(c, "ABC"), "a foreign userid, lower case");
    CHECK(ftpd_jwn_narrows("IBMUSER", "ABCDEFGH", c, sizeof(c)) ==
          FTPD_JWN_NR_OTHER && is(c, "ABCDEFGH"), "8 characters");
    CHECK(ftpd_jwn_narrows("IBMUSER", "$#@", c, sizeof(c)) ==
          FTPD_JWN_NR_OTHER, "national characters");
    CHECK(ftpd_jwn_narrows("IBMUSER", "IBMUSER2", c, sizeof(c)) ==
          FTPD_JWN_NR_OTHER && is(c, "IBMUSER2"),
          "IBMUSER2 is a foreign userid, not the own");

    CHECK(ftpd_jwn_narrows("IBMUSER", "ABCDEFGHI", c, sizeof(c)) ==
          FTPD_JWN_NR_BAD && is(c, ""), "9 characters: 501, not truncated");
    CHECK(ftpd_jwn_narrows("IBMUSER", "IBMUSERXX", c, sizeof(c)) ==
          FTPD_JWN_NR_BAD, "longer than 8 is never cut down to the own");
    CHECK(ftpd_jwn_narrows("IBMUSER", "A.B", c, sizeof(c)) == FTPD_JWN_NR_BAD,
          "A.B");
    CHECK(ftpd_jwn_narrows("IBMUSER", "AB%", c, sizeof(c)) == FTPD_JWN_NR_BAD,
          "AB%");
    CHECK(ftpd_jwn_narrows("IBMUSER", "ABC*", c, sizeof(c)) == FTPD_JWN_NR_BAD,
          "ABC* is no pattern any more");
    CHECK(ftpd_jwn_narrows("IBMUSER", "**", c, sizeof(c)) == FTPD_JWN_NR_BAD,
          "**");
    CHECK(ftpd_jwn_narrows("IBMUSER", "A B", c, sizeof(c)) == FTPD_JWN_NR_BAD,
          "blank inside");
    CHECK(ftpd_jwn_narrows("IBMUSER", " ", c, sizeof(c)) == FTPD_JWN_NR_BAD,
          "one blank");
    CHECK(ftpd_jwn_narrows("IBMUSER", "A\rB", c, sizeof(c)) ==
          FTPD_JWN_NR_BAD, "control character");
    CHECK(ftpd_jwn_narrows("IBMUSER", NULL, c, sizeof(c)) == FTPD_JWN_NR_BAD,
          "NULL value");
    CHECK(ftpd_jwn_narrows("IBMUSER", "ABC", c, 4) == FTPD_JWN_NR_BAD,
          "a result buffer that is too small");
    CHECK(ftpd_jwn_narrows(NULL, "ABC", c, sizeof(c)) == FTPD_JWN_NR_OTHER,
          "no user: nothing is own");
    CHECK(ftpd_jwn_narrows("", "", c, sizeof(c)) == FTPD_JWN_NR_RESET,
          "empty resets for any user");
}

static void
test_printable(void)
{
    char out[FTPD_JWN_PRTSZ];
    char buf[4];
    char in[2];
    int i;
    int ok = 1;

    for (i = 0; i < 256; i++) {
        int j;

        in[0] = (char)i;
        in[1] = '\0';
        ftpd_jwn_printable(out, sizeof(out), i ? in : "");
        if (i == 0) {
            if (out[0] != '\0')
                ok = 0;
            continue;
        }
        for (j = 0; out[j]; j++) {
            if (out[j] != '?' && strchr(FTPD_JWN_CHARS, out[j]) == NULL)
                ok = 0;
        }
        if (strchr(FTPD_JWN_CHARS, in[0]) != NULL) {
            if (out[0] != in[0] || out[1] != '\0')
                ok = 0;
        } else if (out[0] != '?' || out[1] != '\0') {
            ok = 0;
        }
    }
    CHECK(ok, "all 256 byte values: only the set and '?' come out");

    ftpd_jwn_printable(out, sizeof(out), "AB\r\nC");
    CHECK(is(out, "AB??C"), "CR LF in the middle");
    ftpd_jwn_printable(out, sizeof(out), "A\033B");
    CHECK(is(out, "A?B"), "ESC in the middle");
    ftpd_jwn_printable(out, sizeof(out), "A.B");
    CHECK(is(out, "A?B"), "a user id with a dot");
    ftpd_jwn_printable(out, sizeof(out), "ABC");
    CHECK(is(out, "ABC"), "a good value stays");
    ftpd_jwn_printable(out, sizeof(out), "abc");
    CHECK(is(out, "???"), "lower case is shown as unknown");
    ftpd_jwn_printable(out, sizeof(out), "ABCDEFGHIJKL");
    CHECK(is(out, "ABCDEFGH"), "12 characters become 8");
    ftpd_jwn_printable(out, sizeof(out), "");
    CHECK(is(out, ""), "empty stays empty");
    ftpd_jwn_printable(out, sizeof(out), NULL);
    CHECK(is(out, ""), "NULL is empty");

    memset(buf, 'x', sizeof(buf));
    ftpd_jwn_printable(buf, 1, "ABC");
    CHECK(buf[0] == '\0' && buf[1] == 'x', "sz 1: the empty string");
    memset(buf, 'x', sizeof(buf));
    ftpd_jwn_printable(buf, 0, "ABC");
    CHECK(buf[0] == 'x', "sz 0: nothing written");
    memset(buf, 'x', sizeof(buf));
    ftpd_jwn_printable(buf, 3, "ABCDEF");
    CHECK(is(buf, "AB") && buf[3] == 'x', "sz 3: two characters and NUL");
}

static ftpd_jwn_res_t
res_of(int decision, int reason, int calls)
{
    ftpd_jwn_res_t r;

    r.decision = decision;
    r.reason = reason;
    r.level = 0;
    r.rc = -1;
    r.elen = 0;
    r.racf_calls = calls;
    return r;
}

static void
test_tally(void)
{
    ftpd_jwn_tally_t t;
    ftpd_jwn_sess_t st;
    ftpd_jwn_res_t err = res_of(FTPD_JWN_HIDE, FTPD_JWN_R_ERR_RC, 1);
    ftpd_jwn_res_t errc = res_of(FTPD_JWN_HIDE, FTPD_JWN_R_ERR_COMPONENT, 0);
    ftpd_jwn_res_t none = res_of(FTPD_JWN_HIDE, FTPD_JWN_R_NONE, 1);
    ftpd_jwn_res_t nopr = res_of(FTPD_JWN_HIDE, FTPD_JWN_R_NOPROFILE, 1);
    ftpd_jwn_res_t nocan = res_of(FTPD_JWN_NOCANCEL, FTPD_JWN_R_READ, 2);
    ftpd_jwn_res_t own = res_of(FTPD_JWN_ALLOW, FTPD_JWN_R_OWN, 0);
    ftpd_jwn_res_t frd = res_of(FTPD_JWN_ALLOW, FTPD_JWN_R_READ, 1);
    ftpd_jwn_res_t noacee = res_of(FTPD_JWN_HIDE, FTPD_JWN_R_NOACEE, 0);
    int i, lines, lerr, ok;

    /* L1: 1000 ERR hits in one listing */
    ftpd_jwn_tally_init(&t);
    memset(&st, 0, sizeof(st));
    lines = 0;
    for (i = 0; i < 1000; i++) {
        if (ftpd_jwn_tally(&t, &st, FTPD_JWN_OP_LIST,
                           i & 1 ? &err : &errc) == FTPD_JWN_LINE_ERROR)
            lines++;
    }
    CHECK(lines == 8, "1000 ERR hits write 8 lines");
    CHECK(ftpd_jwn_tally_more(&t) == 992, "... and 992 more ERR jobs");
    CHECK(t.err == 1000 && t.hidden == 0 && t.own == 0, "all 1000 counted");
    CHECK(t.racf_calls == 500, "RACF calls counted");
    CHECK(st.denied == 0 && st.denied_more == 0,
          "ERR is not in the session limit");

    ftpd_jwn_tally_init(&t);
    lines = 0;
    for (i = 0; i < 8; i++) {
        if (ftpd_jwn_tally(&t, &st, FTPD_JWN_OP_RETR, &err) ==
                FTPD_JWN_LINE_ERROR)
            lines++;
    }
    CHECK(lines == 8 && ftpd_jwn_tally_more(&t) == 0,
          "8 ERR hits: 8 lines and no summary line");
    ftpd_jwn_tally(&t, &st, FTPD_JWN_OP_RETR, &err);
    CHECK(ftpd_jwn_tally_more(&t) == 1, "the 9th is the first one 'more'");

    /* own jobs never write and never fail */
    ftpd_jwn_tally_init(&t);
    lines = 0;
    for (i = 0; i < 1000; i++) {
        if (ftpd_jwn_tally(&t, &st, FTPD_JWN_OP_LIST, &own) !=
                FTPD_JWN_LINE_NONE)
            lines++;
    }
    CHECK(lines == 0 && t.own == 1000 && t.err == 0 && t.racf_calls == 0,
          "1000 own jobs: no line, own 1000, ERR 0, no RACF call");

    /* 500 own and 500 foreign ERR */
    ftpd_jwn_tally_init(&t);
    lines = 0;
    for (i = 0; i < 1000; i++) {
        if (ftpd_jwn_tally(&t, &st, FTPD_JWN_OP_LIST,
                           i & 1 ? &err : &own) == FTPD_JWN_LINE_ERROR)
            lines++;
    }
    CHECK(lines == 8 && ftpd_jwn_tally_more(&t) == 492 && t.own == 500,
          "500 own, 500 ERR: 8 lines, 492 more, own 500");

    /* NOACEE: one line per command, not an ERR, counted as hidden */
    ftpd_jwn_tally_init(&t);
    lines = 0;
    for (i = 0; i < 1000; i++) {
        if (ftpd_jwn_tally(&t, &st, FTPD_JWN_OP_LIST, &noacee) ==
                FTPD_JWN_LINE_NOACEE)
            lines++;
    }
    CHECK(lines == 1 && t.hidden == 1000 && t.err == 0 && t.own == 0 &&
          ftpd_jwn_tally_more(&t) == 0, "NOACEE: one line, hidden 1000");
    CHECK(st.denied == 0, "NOACEE is not in the session limit");
    ftpd_jwn_tally_init(&t);
    CHECK(ftpd_jwn_tally(&t, &st, FTPD_JWN_OP_RETR, &noacee) ==
          FTPD_JWN_LINE_NOACEE, "the next command writes it again");

    /* the session limit */
    memset(&st, 0, sizeof(st));
    lines = 0;
    for (i = 0; i < 20; i++) {
        static const int ops[] = { FTPD_JWN_OP_RETR, FTPD_JWN_OP_DELE,
                                   FTPD_JWN_OP_LISTJOB };

        ftpd_jwn_tally_init(&t);
        if (ftpd_jwn_tally(&t, &st, ops[i % 3], &none) == FTPD_JWN_LINE_WARN)
            lines++;
    }
    CHECK(lines == 16, "20 refusals write 16 lines");
    CHECK(ftpd_jwn_tally_session_more(&st) == 4, "... and 4 more denied");

    memset(&st, 0, sizeof(st));
    for (i = 0; i < 16; i++) {
        ftpd_jwn_tally_init(&t);
        ftpd_jwn_tally(&t, &st, FTPD_JWN_OP_RETR, &none);
    }
    CHECK(ftpd_jwn_tally_session_more(&st) == 0, "16: no summary line");
    ftpd_jwn_tally_init(&t);
    ftpd_jwn_tally(&t, &st, FTPD_JWN_OP_RETR, &none);
    CHECK(ftpd_jwn_tally_session_more(&st) == 1, "17: one more");

    /* NONE, NOPROFILE and NOCANCEL share the limit */
    memset(&st, 0, sizeof(st));
    lines = 0;
    for (i = 0; i < 18; i++) {
        const ftpd_jwn_res_t *r = i % 3 == 0 ? &none :
                                  i % 3 == 1 ? &nopr : &nocan;

        ftpd_jwn_tally_init(&t);
        if (ftpd_jwn_tally(&t, &st, FTPD_JWN_OP_DELE, r) == FTPD_JWN_LINE_WARN)
            lines++;
    }
    CHECK(lines == 16 && ftpd_jwn_tally_session_more(&st) == 2,
          "NONE, NOPROFILE and NOCANCEL count together");

    /* ERR after the limit is still written; the counter stays */
    ftpd_jwn_tally_init(&t);
    lerr = ftpd_jwn_tally(&t, &st, FTPD_JWN_OP_RETR, &err);
    CHECK(lerr == FTPD_JWN_LINE_ERROR && st.denied_more == 2,
          "ERR after the session limit: a line, denied_more unchanged");
    ftpd_jwn_tally_init(&t);
    CHECK(ftpd_jwn_tally(&t, &st, FTPD_JWN_OP_RETR, &noacee) ==
          FTPD_JWN_LINE_NOACEE && st.denied_more == 2,
          "NOACEE after the session limit: a line");

    /* a listing never writes WARN lines and never touches the session */
    memset(&st, 0, sizeof(st));
    ftpd_jwn_tally_init(&t);
    ok = 1;
    for (i = 0; i < 1000; i++) {
        if (ftpd_jwn_tally(&t, &st, FTPD_JWN_OP_LIST,
                           i & 1 ? &none : &nopr) != FTPD_JWN_LINE_NONE)
            ok = 0;
    }
    CHECK(ok && st.denied == 0 && st.denied_more == 0 && t.hidden == 1000,
          "1000 refusals in a listing: no line, session unchanged, hidden 1000");

    /* visible foreign jobs cost RACF calls and nothing else */
    ftpd_jwn_tally_init(&t);
    CHECK(ftpd_jwn_tally(&t, &st, FTPD_JWN_OP_LIST, &frd) ==
          FTPD_JWN_LINE_NONE && t.racf_calls == 1 && t.own == 0 &&
          t.hidden == 0, "a visible foreign job counts only its RACF calls");
    ftpd_jwn_tally_init(&t);
    ftpd_jwn_tally(&t, &st, FTPD_JWN_OP_RETR, &nocan);
    CHECK(t.hidden == 0 && t.racf_calls == 2,
          "NOCANCEL is not hidden");

    CHECK(ftpd_jwn_tally(NULL, &st, FTPD_JWN_OP_LIST, &none) ==
          FTPD_JWN_LINE_NONE, "no counter is no line");
    CHECK(ftpd_jwn_tally_more(NULL) == 0 &&
          ftpd_jwn_tally_session_more(NULL) == 0, "NULL counters");
    ftpd_jwn_tally_init(&t);
    CHECK(ftpd_jwn_tally(&t, NULL, FTPD_JWN_OP_RETR, &none) ==
          FTPD_JWN_LINE_NONE, "no session counter: no WARN line");
}

/* --------------------------------------------------------------------
** The per-command cache.  RAKF compares the first 8 characters of the
** entity, so jobs with the same 8-character key get one answer; the cache
** asks RACF once per key.
** ----------------------------------------------------------------- */
static int
check_job(const ftpd_jwn_ctx_t *ctx, int op, const char *owner,
          const char *jobname, const char *jobid, ftpd_jwn_res_t *res)
{
    return ftpd_jwn_check(ctx, op, owner, jobname, jobid, res);
}

/* A RACF that compares 12 characters of the entity against the profile
** ABCDEFGH.PROD: READ for what starts with it, no profile otherwise. */
static int
auth_w12(void *arg, const char *entity, int want_update)
{
    (void)arg;
    (void)want_update;
    return strncmp(entity, "ABCDEFGH.PROD", 13) == 0 ? 0 : 4;
}

static void
test_cache(void)
{
    ftpd_jwn_ctx_t ctx;
    ftpd_jwn_cache_t cache;
    ftpd_jwn_res_t res;
    stub_t st;
    char owner[16];
    char jobid[16];
    int i, d1, d2, calls;

    /* one key, many jobs: one question */
    setup(&ctx, &st, "MIKE1", 0, 8);
    ftpd_jwn_cache_init(&cache);
    ctx.cache = &cache;
    for (i = 0; i < 10; i++) {
        sprintf(jobid, "JOB%05d", i);
        d1 = check_job(&ctx, FTPD_JWN_OP_LIST, "ABCDEFGH", "JOBA", jobid,
                       &res);
        CHECK(d1 == FTPD_JWN_ALLOW && res.level == FTPD_JWN_LV_READ,
              "cache: every job of one key gets READ");
    }
    CHECK(st.calls == 1, "cache: 10 jobs with one key ask RACF once");
    CHECK(res.racf_calls == 0, "cache: a hit reports no RACF call");

    /* the same jobs without a cache ask each time */
    setup(&ctx, &st, "MIKE1", 0, 8);
    for (i = 0; i < 10; i++) {
        sprintf(jobid, "JOB%05d", i);
        check_job(&ctx, FTPD_JWN_OP_LIST, "ABCDEFGH", "JOBA", jobid, &res);
    }
    CHECK(st.calls == 10, "no cache: 10 jobs ask 10 times");

    /* k keys, N jobs: k questions */
    setup(&ctx, &st, "MIKE1", 0, 8);
    ftpd_jwn_cache_init(&cache);
    ctx.cache = &cache;
    for (i = 0; i < 30; i++) {
        sprintf(owner, "OWNER%03d", i % 3);
        sprintf(jobid, "JOB%05d", i);
        check_job(&ctx, FTPD_JWN_OP_LIST, owner, "JOBA", jobid, &res);
    }
    CHECK(st.calls == 3, "cache: 30 jobs, 3 keys, 3 questions");

    /* no profile and no access are remembered as well */
    setup(&ctx, &st, "MIKE1", 4, 8);
    ftpd_jwn_cache_init(&cache);
    ctx.cache = &cache;
    d1 = check_job(&ctx, FTPD_JWN_OP_LIST, "ABCDEFGH", "JOBA", "JOB00001",
                   &res);
    d2 = check_job(&ctx, FTPD_JWN_OP_LIST, "ABCDEFGH", "JOBB", "JOB00002",
                   &res);
    CHECK(d1 == FTPD_JWN_HIDE && d2 == FTPD_JWN_HIDE && st.calls == 1 &&
          res.reason == FTPD_JWN_R_NOPROFILE,
          "cache: RC 4 is remembered, reason kept");
    setup(&ctx, &st, "MIKE1", 8, 8);
    ftpd_jwn_cache_init(&cache);
    ctx.cache = &cache;
    check_job(&ctx, FTPD_JWN_OP_LIST, "ABCDEFGH", "JOBA", "JOB00001", &res);
    d2 = check_job(&ctx, FTPD_JWN_OP_LIST, "ABCDEFGH", "JOBB", "JOB00002",
                   &res);
    CHECK(d2 == FTPD_JWN_HIDE && st.calls == 1 &&
          res.reason == FTPD_JWN_R_NONE,
          "cache: RC 8 is remembered, reason kept");

    /* an unusable answer is never remembered */
    setup(&ctx, &st, "MIKE1", 12, 8);
    ftpd_jwn_cache_init(&cache);
    ctx.cache = &cache;
    for (i = 0; i < 3; i++)
        check_job(&ctx, FTPD_JWN_OP_LIST, "ABCDEFGH", "JOBA", "JOB00001",
                  &res);
    CHECK(st.calls == 3 && res.decision == FTPD_JWN_HIDE &&
          res.reason == FTPD_JWN_R_ERR_RC,
          "cache: an RC of 12 is asked again each time");

    /* own jobs never reach the cache */
    setup(&ctx, &st, "MIKE1", 0, 0);
    ftpd_jwn_cache_init(&cache);
    ctx.cache = &cache;
    d1 = check_job(&ctx, FTPD_JWN_OP_LIST, "MIKE1", "JOBA", "JOB00001",
                   &res);
    CHECK(d1 == FTPD_JWN_ALLOW && st.calls == 0 && cache.n == 0,
          "cache: an own job asks nothing and stores nothing");

    /* a component error never reaches it either */
    d1 = check_job(&ctx, FTPD_JWN_OP_LIST, "AB.CD", "JOBA", "JOB00001",
                   &res);
    CHECK(d1 == FTPD_JWN_HIDE && st.calls == 0 && cache.n == 0,
          "cache: a bad component asks nothing and stores nothing");

    /* a level found for listing is not the level for deleting */
    setup(&ctx, &st, "MIKE1", 0, 8);
    ftpd_jwn_cache_init(&cache);
    ctx.cache = &cache;
    check_job(&ctx, FTPD_JWN_OP_LIST, "ABCDEFGH", "JOBA", "JOB00001", &res);
    calls = st.calls;
    d1 = check_job(&ctx, FTPD_JWN_OP_DELE, "ABCDEFGH", "JOBA", "JOB00001",
                   &res);
    CHECK(calls == 1 && st.update_calls == 1 && d1 == FTPD_JWN_NOCANCEL,
          "cache: READ found by LIST does not answer a DELE");
    d2 = check_job(&ctx, FTPD_JWN_OP_DELE, "ABCDEFGH", "JOBA", "JOB00002",
                   &res);
    CHECK(d2 == FTPD_JWN_NOCANCEL && st.update_calls == 1,
          "cache: the DELE answer is remembered for DELE");

    /* the 17th key: asked every time, same answer, nothing evicted */
    setup(&ctx, &st, "MIKE1", 0, 0);
    ftpd_jwn_cache_init(&cache);
    ctx.cache = &cache;
    for (i = 0; i < 17; i++) {
        sprintf(owner, "OWNER%03d", i);
        check_job(&ctx, FTPD_JWN_OP_LIST, owner, "JOBA", "JOB00001", &res);
    }
    CHECK(st.calls == 17 && cache.n == FTPD_JWN_CACHEMAX,
          "cache: 17 keys, 16 stored");
    calls = st.calls;
    for (i = 0; i < 2; i++) {
        d1 = check_job(&ctx, FTPD_JWN_OP_LIST, "OWNER016", "JOBA",
                       "JOB00001", &res);
        CHECK(d1 == FTPD_JWN_ALLOW && res.level == FTPD_JWN_LV_READ,
              "cache: the 17th key gets the same level without the cache");
    }
    CHECK(st.calls == calls + 2, "cache: the 17th key asks each time");
    check_job(&ctx, FTPD_JWN_OP_LIST, "OWNER000", "JOBA", "JOB00001", &res);
    CHECK(st.calls == calls + 2, "cache: the 16 stored keys are not evicted");

    /* The limit of the key: a RAKF that compares 12 characters tells
    ** ABCDEFGH.PRODX from ABCDEFGH.TESTX, an 8-character key does not.  RAKF
    ** 2.0.0 compares 8 (Test 18, Fall 14); this is why the guide says not to
    ** raise the entity maximum above 8 without changing the key. */
    setup(&ctx, &st, "MIKE1", 0, 8);
    ctx.auth_fn = auth_w12;
    d1 = check_job(&ctx, FTPD_JWN_OP_LIST, "ABCDEFGH", "PRODX", "JOB00001",
                   &res);
    d2 = check_job(&ctx, FTPD_JWN_OP_LIST, "ABCDEFGH", "TESTX", "JOB00002",
                   &res);
    CHECK(d1 == FTPD_JWN_ALLOW && d2 == FTPD_JWN_HIDE,
          "without the cache a width-12 RACF tells the two jobs apart");
    ftpd_jwn_cache_init(&cache);
    ctx.cache = &cache;
    d1 = check_job(&ctx, FTPD_JWN_OP_LIST, "ABCDEFGH", "PRODX", "JOB00001",
                   &res);
    d2 = check_job(&ctx, FTPD_JWN_OP_LIST, "ABCDEFGH", "TESTX", "JOB00002",
                   &res);
    CHECK(d1 == d2 && FTPD_JWN_KEYLEN == 8,
          "the 8-character key gives both jobs one level");
}

int
main(void)
{
    test_components();
    test_mine();
    test_level();
    test_check();
    test_match();
    test_narrows();
    test_printable();
    test_tally();
    test_cache();

    return mbt_test_summary("TSTJWN");
}
