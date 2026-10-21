# FTPD RAKF Setup

FTPD has no user database of its own. Every `USER`/`PASS` is verified by RAKF,
and every data set access is checked against RAKF under the **logged-in user's**
identity. Without RAKF nobody can log in, and no configuration option turns that
off.

This guide covers what FTPD needs. It is not a RAKF manual — the authoritative
one is the RAKF User's Guide that ships with RAKF itself.

## Which RAKF this assumes

**RAKF 1.2.x** and **RAKF 2.0.0** (the release `v2.0.0` of the RAKF
repository). At 1.2.x there is **no administration tooling**: the tables are
plain text members you edit by hand and then reload. RAKF 2.0.0 keeps the two
tables and adds TSO commands for them (`ADDUSER`, `ALTUSER`, `DELUSER`,
`LISTUSER`, `ADDSD`, `RDEFINE`, `PERMIT`, `RDELETE`), a password file
`SYS1.SECURE.SHADOW`, and the mapping of a started task by its name
(section 2). Where RAKF 2.0.0 changes something, this guide says so in a box.

> Some systems (MVS/CE) add a `RAKFCL` command processor that wraps table
> editing in RACF-like syntax. It is a distribution extension, not part of RAKF,
> and nothing here depends on it.

---

## 1. The two tables

Everything lives in two members, both plain 80-column text:

| Member | Holds |
|--------|-------|
| `SYS1.SECURE.CNTL(USERS)` | who exists, their group and password |
| `SYS1.SECURE.CNTL(PROFILES)` | what is protected and who may reach it |

Both are **column-positioned**. There is no keyword syntax — a line that looks
like a RACF command is just a wrong line.

### USERS

```
         1         2         3
1234567890123456789012345678901234
IBMUSER  ADMIN   *SYS1     Y
```

| Columns | Field |
|:-------:|-------|
| 1 – 8 | userid |
| 10 – 17 | group |
| 18 | `*` if the userid has more than one group entry |
| 19 – 26 | password |
| 28 | Operations authority, `Y` or `N` |
| 30 | Special authority, `Y` or `N` — RAKF 2.0.0 only, see the box below |
| 31 – 50 | comment |

> **RAKF 2.0.0:** column 30 is the SPECIAL authority. A user needs it to run
> `ADDUSER`, `ALTUSER`, `DELUSER`, `LISTUSER`, `ADDSD`, `RDEFINE`, `PERMIT` and
> `RDELETE`; RAKF turns it on only for `Y`, so a blank column means no. A
> `USERS` table that you keep from an earlier RAKF has no entries in column 30:
> before you use these commands, put a `Y` in column 30 of every line of the
> administrator's entries and reload (`S RAKF`). The commands cannot do this
> for you, because they need the flag themselves. The password column 19 – 26
> is not used; the password is kept as a hash in `SYS1.SECURE.SHADOW`.

### PROFILES

```
         1         2         3         4         5         6
123456789012345678901234567890123456789012345678901234567890123456
DATASET SYS1.SECURE.*                               RAKFADM UPDATE
```

| Columns | Field |
|:-------:|-------|
| 1 – 8 | class — `DATASET `, `FACILITY`, `DASDVOL ` |
| 9 – 52 | resource or data set name, generic `*` allowed |
| 53 – 60 | group — **blank means universal access** |
| 61 – 66 | permission — `NONE`, `READ`, `UPDATE`, `ALTER` |

### Three rules that are easy to miss

- **Both tables must be in ascending sort order.** A sort error does not skip
  the bad line, it *inhibits initialization of the table* — so one misplaced
  entry takes down authorization for the whole system, not just for FTPD.
- **The universal entry comes first.** The table is searched from the bottom
  upwards so the most specific hit wins, which only works if the blank-group
  line for a resource precedes its group-specific lines.
- **A line starting with `*` is a comment.** RAKF's own shipped table writes the
  equivalent RACF command above each block as a comment. It is worth copying
  that habit — it is the only place the intent is readable.

### Reloading

After **any** change to either member:

```
S RAKF
```

That is the whole procedure, on every system. There is no MODIFY command; RAKF
reads the tables when it starts. A successful users-table load reports:

```
RAKFUIDS4  USER TABLE UPDATED
```

A rejected one echoes the offending card and terminates:

```
RAKFUIDS2  INPUT DATA INVALID OR OUT OF SEQ.
JSMITH   USER     ******** N                                            00000706
RAKFUIDSX  ** PROGRAM TERMINATED **
```

**Read that echoed line with care.** The password column is always printed as
`********`, whatever it really contained — including empty. So the one field
that most often causes the rejection is the one field the diagnostic will not
show you, and a bad card looks indistinguishable from a good one. When the line
looks correct, check the password column and the sort order, in that order.

Nothing is updated when this happens: the in-core table keeps its **previous**
contents, which presents as "my change had no effect" rather than as a failure.

---

## 2. The identity of the FTPD started task

FTPD does not choose its own identity. It runs under whatever RAKF assigned to
the started task when it was created, and keeps that as its resting identity
for as long as it lives. Section 5 explains why that identity matters.

What RAKF assigns depends on the release:

| RAKF | Identity of the started task `FTPD` |
|------|-------------------------------------|
| 2.0.0 | the userid in `SYS1.SECURE.CNTL(USERS)` that is named like the started procedure, `FTPD`, with the group and the Operations flag of its entry; `STC` / `STCGROUP` if there is no such userid |
| 1.2.x and any other without a mapping by STC name | `STC` / `STCGROUP`, hardcoded in `ICHSFR00` |

For a release other than these, the way a mapping is written is part of that
release — see its documentation. FTPD needs two things from it:

- **Map the name `FTPD` to an identity with Operations authority `N`.** This one
  character is load-bearing, see [section 5](#5-why-the-started-tasks-identity-matters).
- **Keep that identity least-privilege.** It is an invariant, not a
  recommendation, and it is spelled out in section 5.

If the identity is a userid in `SYS1.SECURE.CNTL(USERS)`, it is a normal RAKF
account like any other: RAKF 1.2.x requires a password in columns 19–26 and
rejects the whole reload if it is blank, and anyone who knows that password can
log in over FTP as that user. Pick a real password, treat it as a credential,
and do not permit the userid on `FTPAUTH` in the next section. On RAKF 2.0.0
the credential lives in `SYS1.SECURE.SHADOW` and the column is blank — check
which release you are on before copying either form.

After any change to the tables, `S RAKF`.

> **RAKF 2.0.0: the group of the `FTPD` user has to be allowed SVC 244.** At
> startup FTPD asks for its authorization with SVC 244, and it does so under
> the identity of the started task. SVC 244 checks the
> group of that identity against the `FACILITY` profile `SVC244`. If the group
> is refused, FTPD starts and reports `FTPD003W APF SETUP FAILED`.
>
> - In the `PROFILES` that come with RAKF 2.0.0, `SVC244` has the universal
>   entry `NONE` and READ for the groups `ADMIN`, `STARTED` and `STCGROUP`.
>   Check your own table: it is the one that counts.
> - **Do not use `STCGROUP` for the user.** In that same table `STCGROUP` has
>   ALTER on `DATASET *` and `DASDVOL *`, which is the broad authority that
>   section 5 forbids. RAKF's own seed files put the `FTPD` user into the group
>   `STARTED`, which holds nothing but READ on `SVC244`.
> - A group of your own needs its own `SVC244` line, in ascending group order
>   between the existing group lines:
>
>   ```
>            1         2         3         4         5         6
>   123456789012345678901234567890123456789012345678901234567890123456
>   FACILITYSVC244                                      STARTED READ
>   ```
>
>   or `PERMIT SVC244 CLASS(FACILITY) ID(STARTED) ACCESS(READ)`.
> - The user is created with `ADDUSER FTPD PASSWORD(pw) DFLTGRP(STARTED)`.
>   Do not code `OPERATIONS`. `LISTUSER FTPD` shows the authority it got.
>   Both commands need the SPECIAL authority (section 1). Then `S RAKF`.

> **Upgrading from FTPD 1.2.0 or earlier:** those releases logged on as
> `FTPD`/`USER` themselves at startup. FTPD 1.3.0 does not. On a RAKF without a
> mapping by STC name the started task now runs as `STC`/`STCGROUP` again, with
> Operations authority; section 5 says what that means. On RAKF 2.0.0 the
> `FTPD` line in `USERS` is used again, now as the mapping: its group decides
> the identity of the started task and has to be allowed SVC 244 (the box
> above). The documentation of FTPD 1.2.0 says `ADDUSER FTPD DFLTGRP(USER)`.
> FTPD 1.2.0 asks for SVC 244 before it logs on as `FTPD`/`USER`, so on RAKF
> 2.0.0 the group in the `FTPD` line of `USERS` is the group that is checked,
> and `USER` is not one of the groups that the shipped `SVC244` profile allows.
> Give the `FTPD` user a group that holds `SVC244`, such
> as `STARTED`, also if you stay on 1.2.0.

---

## 3. FTPAUTH — who may use FTP at all

`FTPAUTH` in the `FACILITY` class is the gate FTPD checks after the password is
verified. It has to exist in `SYS1.SECURE.CNTL(PROFILES)`, and the group of your
FTP users has to be allowed to read it.

**Look first whether it is already there.** MVS/CE ships `FTPAUTH` in `PROFILES`
with the universal entry `NONE` and the group `ADMIN` at `READ`. In that case
add only the line for your group, below the existing ones, and never the
universal `NONE` line a second time: a duplicate entry makes the table fail its
sort check, and `S RAKF` ends with `RAKFPROF3 I/P RECORDS OUT OF ORDER` and
ABEND U0300. If `FTPAUTH` is not there, add both lines, universal first:

```
         1         2         3         4         5         6
123456789012345678901234567890123456789012345678901234567890123456
*
* rdefine facility ftpauth uacc(none)
* permit ftpauth cl(facility) id(ftpuser) access(read)
*
FACILITYFTPAUTH                                             NONE
FACILITYFTPAUTH                                     FTPUSER READ
```

The first line denies everyone, the second grants one group. Order matters —
universal first, as above, and the group lines of one resource in ascending
group order. Where `FTPAUTH` already exists, the second line is the only one you
add.

**The profile must exist.** It is tempting to skip it, because FTPD lets everyone
in when it is absent — but that is not a permissive default, it is RAKF's
behaviour of granting **ALTER to undefined resources**. Skipping it also means
the userid of the started task from section 2 can log in over FTP.

### Which group

`FTPUSER` above is a dedicated group holding exactly the users who should have
FTP access — the tighter arrangement, and the reason to prefer it is that FTP
access then has its own switch instead of riding on a group that exists for
other reasons.

If you would rather let every ordinary user in, permit the shared group instead:

```
FACILITYFTPAUTH                                     USER    READ
```

Either way, do not permit the userid of the FTPD started task itself.

---

## 4. Data set access

FTPD issues a RACHECK against the `DATASET` class before every data set
operation, under the **client's** identity:

| FTP command | Access checked |
|-------------|----------------|
| `RETR` | READ |
| `STOR`, `APPE` | UPDATE |
| `DELE` | ALTER |
| `MKD`, `RMD` | ALTER |
| `RNFR` / `RNTO` | ALTER |

A common arrangement gives each user full control of their own HLQ and read
access to the system libraries:

```
         1         2         3         4         5         6
123456789012345678901234567890123456789012345678901234567890123456
*
* addsd 'IBMUSER.*'                 uacc(none)
* permit 'IBMUSER.*' id(ibmuser)    access(alter)
*
DATASET IBMUSER.*                                           NONE
DATASET IBMUSER.*                                   IBMUSER ALTER
```

### How FTPD reads the answer

`racf_auth()` returns the SAF return code, and FTPD acts on it:

| RC | Meaning | FTPD |
|----|---------|------|
| 0 | a profile permits the access | allow |
| 4 | **no profile covers the resource** | allow |
| 8 | a profile refuses the access | deny — `550` / `530` |

RC 4 is an allow, not a refusal: it is the same answer data set OPEN, IDCAMS and
the catalog act on, so refusing it would make FTP the only path on the system
that cannot reach an unprotected data set.

Note what that means in practice. RAKF grants **ALTER** to anything no profile
covers, so on a system with no `DATASET` profiles at all, every logged-in user
can do anything to any data set — and FTPD is simply reporting that faithfully.
The stock MVS/CE and TK4- configurations ship a `DATASET *` catch-all, which is
why RC 4 never occurs for the DATASET class there.

---

## 5. Why the started task's identity matters

`ICHSFR00` assigns work that carries no userid of its own:

| Type | USERID | GROUP |
|------|--------|-------|
| Batch jobs | `PROD` | `PRDGROUP` |
| Started tasks | `STC` | `STCGROUP` |

**The `STC` userid has Operations authority, hardcoded in `ICHSFR00`.**
Operations means "always allow access unless explicitly denied by a rule" — so a
started task that keeps this default can reach every data set on the system
whenever a check is made against the started task rather than a user.

FTPD no longer replaces the identity it starts with. At startup it reports the
one it has:

```
FTPD004I STC RUNS AS <user>/<group>
```

On a RAKF that maps the STC name to a low-privilege identity, that is the
identity you configured. On a RAKF without such a mapping it is
`STC/STCGROUP`, and the server runs with Operations authority:

```
FTPD004I STC RUNS AS STC/STCGROUP
```

FTPD cannot tell the two situations apart by itself — read the message. If it
names `STC`, either upgrade RAKF or accept that the started task is as
privileged as the default account.

If the address space has no security environment at all, FTPD says so:

```
FTPD004W NO SECURITY ENVIRONMENT FOR THE STC: RAKF PERMITS ANY ACCESS WITHOUT AN ACEE
```

Do not treat that as cosmetic. RAKF reads a missing ACEE as "access permitted",
so nothing FTPD does outside a user's session is checked.

### Security invariant — the started task's identity must stay least-privilege

> **This is an invariant, not a recommendation.**
>
> The identity of the started task **must not** be granted broad data set
> authority — no Operations-equivalent access, no wide `ID(FTPD) ACCESS(ALTER)`
> profiles. Its only required authority is what the started task itself needs to
> run, never access to user data. Per-user access is authorised against the
> **logged-in user's** identity.
>
> **Why it is an invariant:** after every operation that runs under a user's
> identity, and after an unexpected ABEND in a session, FTPD puts the
> address-space-wide security environment (`ASXBSENV`) back to the identity the
> started task started with. Sessions run as concurrent subtasks sharing that
> one field, so a concurrent session can be transiently pulled onto it. That is
> fail-closed **only while that identity is minimally privileged**: a session
> pulled onto it can lose authority, never gain it. Give it broad authority —
> which is exactly what `STC/STCGROUP` is — and the same transient switch
> becomes **fail-open**: a concurrent session's data set OPEN could momentarily
> authorise with elevated access.

---

## 6. Verify

1. `S RAKF` — expect `RAKFUIDS4  USER TABLE UPDATED`.
2. `/S FTPD` — expect `FTPD004I STC RUNS AS` followed by the identity you
   configured for the started task. `STC/STCGROUP` there means no mapping
   applies (section 5).
3. Log in over FTP with an ordinary userid that is in the permitted group, and
   with one that is not. The second must be refused with `530`.

Step 3 is the one worth doing. Steps 1 and 2 only prove the tables loaded and
which identity the started task has; they say nothing about whether `FTPAUTH` is actually
gating anything.

---

## 7. Troubleshooting

| Symptom | Cause |
|---------|-------|
| `RAKFUIDS2  INPUT DATA INVALID OR OUT OF SEQ.` | A malformed line, or the table is not in ascending sort order. The utility terminates and the in-core table keeps its **previous** contents — which looks exactly like "my change had no effect". The echoed card masks the password as `********`, so it cannot tell you whether that was the problem |
| Reload rejected and the echoed card looks fine | Most likely a blank password. RAKF 1.2.x requires columns 19–26 to be filled — section 2 |
| `RAKF008W illegal operation -- access denied` | The `RAKFADM` FACILITY profile denies the table-update utility |
| `FTPD004I STC RUNS AS STC/STCGROUP` | No mapping of the STC name `FTPD` applies — the RAKF release has none, or the name is not in its configuration (on RAKF 2.0.0: no userid `FTPD` in `USERS`). FTPD is running with Operations authority |
| `FTPD004W NO SECURITY ENVIRONMENT` | The address space has no ACEE. RAKF permits any access without one |
| `FTPD003W APF SETUP FAILED RC=n` | No APF authorisation and no SVC 244. Nobody can log in: every `PASS` answers `530 Login not possible: FTPD is not APF authorized`. On RAKF 2.0.0 the group of the `FTPD` user may lack READ on `FACILITY` `SVC244` — section 2 |
| `ADDUSER`, `PERMIT` and the other RAKF commands refuse to run | The user lacks the SPECIAL authority (column 30 of `USERS`, RAKF 2.0.0) — section 1 |
| `530 Login incorrect` for a valid user | Wrong password, or the user is not in `USERS`. A user added without `S RAKF` does not exist yet |
| `530 Not authorized for FTP access` | `FTPAUTH` denies this user — check the group in columns 53–60 |
| `550 Access denied to <dsn>` | A `DATASET` profile refuses the access under the **client's** identity, not FTPD's |
| Everyone can reach everything | No `DATASET` profiles. Undefined resources get ALTER — section 4 |

---

## Related

- [installation.md](installation.md)
  — installing FTPD itself
- [FTPD_CONCEPT.md](../internals/FTPD_CONCEPT.md)
  — architecture, including the session/identity model
