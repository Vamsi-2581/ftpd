#!/usr/bin/env python3
"""Keep every path to a job's spool behind the spool policy (issues #90, #268).

The JES mode of LIST, RETR and DELE decides per job whether the session may
see, read or delete it.  That decision lives in one place -- ftpd_jwn_check()
in ftpd#jwn.c, called through job_access() in ftpd#jes.c -- and this check
fails the build when a source of the load module grows a way around it.  The
sources are the ones mbt.toml lists for its [module.*] tables, not a fixed
file name.

In ftpd#jes.c, the one file that talks to JES today:

  (a) a strcmp/strncmp/memcmp on `->owner`.  The only ownership test is
      ftpd_jwn_mine(); a second one is how a prefix match (IBMUSER2 is not
      IBMUSER) comes back.
  (b) a call to racf_auth() anywhere but in jes_racf_auth(), the hull
      ftpd_jwn_check() asks through.
  (c) a call to jescanj(), jesdelj(), jesreque(), jesxwrtr(), jesprint() or
      ftpd_data_open() in a function that has not called job_access() or
      find_job() (which calls job_access()) earlier in its text.  Deleting a
      job, reading its spool or opening the data connection for it must come
      after the policy.

In every other module source, except the allow-list below:

  (d) a mention of the spool API -- jesopen, jesopen_ex, jesjob, jesprint,
      jesdelj, jescanj, jesreque, jesxwrtr -- in a function that does not
      call ftpd_jwn_check().  For the ones that act on a job (all but
      jesopen, jesopen_ex and jesjob) the call to ftpd_jwn_check() must come
      earlier in the function's text; for the others, anywhere in it.  A
      mention outside any function is refused.
  (e) a call to racf_auth() whose arguments name the JESSPOOL class.
  (f) in a file that uses the JESJOB type or includes <mvs/jes2.h>, rule (a).
      (ftpd#ufs.c has an `->owner` of its own, which is no spool owner.)

Exempt from (c), by name, and only these (an exemption is a change to this
file and therefore visible in the pull request):

  ftpd_jes_submit   submitting opens the data connection without any job
  ftpd_jes_list     opens the connection first; the policy runs per job in
                    the loop that follows

Allow-list: ftpd#jwn.c is the policy; it is not scanned.  The check fails if
a name on the list is not among the module's sources, so a rename cannot
quietly turn it off.

Limits
------
This is a text proxy: the order of the text, not dominance.  A conditional
call of job_access() satisfies (c) and is not flagged, and a renamed pointer
to racf_auth() is not seen.  Test 18 (test/test_ftpd.sh) and TSTJWN are the
proof; this stops the obvious regression.

Usage: tools/check-jes-access.py [mbt.toml]
"""
import glob
import os
import re
import sys

try:
    import tomllib
except ImportError:
    sys.exit("check-jes-access: needs Python 3.11+ (tomllib)")

JES_FILE = "ftpd#jes.c"
ALLOW = ("ftpd#jwn.c",)

AUTH_HULL = "jes_racf_auth"
POLICY_CALLS = ("job_access", "find_job")
GUARDED_CALLS = ("jescanj", "jesdelj", "jesreque", "jesxwrtr", "jesprint",
                 "ftpd_data_open")
# The spool API of <mvs/jes2.h> as it reaches a job's spool, split by whether
# the call acts on a job or only opens or enumerates.
SPOOL_ACT = ("jescanj", "jesdelj", "jesreque", "jesxwrtr", "jesprint")
SPOOL_SEE = ("jesopen", "jesopen_ex", "jesjob")
POLICY_CALL = "ftpd_jwn_check"
EXEMPT = ("ftpd_jes_submit", "ftpd_jes_list")
# Functions the check relies on; without them it would pass an empty file.
REQUIRED = (AUTH_HULL, "job_access", "find_job") + EXEMPT


def strip_noise(src):
    """Blank out comments, string and character literals, keep the layout."""
    out = []
    i = 0
    n = len(src)
    while i < n:
        c = src[i]
        if src.startswith("/*", i):
            j = src.find("*/", i + 2)
            j = n if j < 0 else j + 2
            out.append(re.sub(r"[^\n]", " ", src[i:j]))
            i = j
        elif src.startswith("//", i):
            j = src.find("\n", i)
            j = n if j < 0 else j
            out.append(" " * (j - i))
            i = j
        elif c in "\"'":
            j = i + 1
            while j < n and src[j] != c:
                j += 2 if src[j] == "\\" else 1
            j = min(j + 1, n)
            out.append(c + re.sub(r"[^\n]", " ", src[i + 1:j - 1]) + c)
            i = j
        else:
            out.append(c)
            i += 1
    return "".join(out)


def functions(text):
    """(name, start, end) of each top-level function body in stripped text."""
    found = []
    depth = 0
    start = None
    name = None
    for m in re.finditer(r"[{}]", text):
        if m.group() == "{":
            if depth == 0:
                head = text[:m.start()]
                # the identifier before the last "(" of the head, which is
                # the parameter list of this definition
                p = head.rstrip()
                if p.endswith(")"):
                    close = len(p) - 1
                    level = 0
                    k = close
                    while k >= 0:
                        if p[k] == ")":
                            level += 1
                        elif p[k] == "(":
                            level -= 1
                            if level == 0:
                                break
                        k -= 1
                    e = k
                    while e > 0 and p[e - 1].isspace():
                        e -= 1
                    b = e
                    while b > 0 and (p[b - 1].isalnum() or p[b - 1] == "_"):
                        b -= 1
                    name = p[b:e] or None
                else:
                    name = None
                start = m.start()
            depth += 1
        else:
            depth -= 1
            if depth == 0 and name:
                found.append((name, start, m.end()))
                name = None
    return found


def balanced(text, open_at):
    """Text of the argument list whose "(" is at open_at."""
    level = 0
    for k in range(open_at, len(text)):
        if text[k] == "(":
            level += 1
        elif text[k] == ")":
            level -= 1
            if level == 0:
                return text[open_at + 1:k]
    return text[open_at + 1:]


def line_of(src, pos):
    return src.count("\n", 0, pos) + 1


def check_jes(path):
    path = os.path.relpath(path)
    with open(path, encoding="utf-8") as f:
        src = f.read()
    text = strip_noise(src)
    funcs = functions(text)
    names = {n for n, _, _ in funcs}
    bad = []

    for req in REQUIRED:
        if req not in names:
            bad.append("%s: function %s not found; the check, or the file, "
                       "has changed shape" % (path, req))

    for m in re.finditer(r"\b(?:strcmp|strncmp|memcmp)\s*\(", text):
        args = balanced(text, m.end() - 1)
        if re.search(r"->\s*owner\b", args):
            bad.append("%s:%d: owner compared outside ftpd_jwn_mine() "
                       "(rule a)" % (path, line_of(src, m.start())))

    for m in re.finditer(r"\bracf_auth\s*\(", text):
        owner = [n for n, s, e in funcs if s <= m.start() < e]
        if owner != [AUTH_HULL]:
            bad.append("%s:%d: racf_auth() outside %s() (rule b)"
                       % (path, line_of(src, m.start()), AUTH_HULL))

    for name, start, end in funcs:
        if name in EXEMPT:
            continue
        body = text[start:end]
        policy = [m.start() for c in POLICY_CALLS
                  for m in re.finditer(r"\b%s\s*\(" % c, body)]
        first_policy = min(policy) if policy else None
        for c in GUARDED_CALLS:
            for m in re.finditer(r"\b%s\s*\(" % c, body):
                if first_policy is None or first_policy > m.start():
                    bad.append("%s:%d: %s() in %s() before job_access() or "
                               "find_job() (rule c)"
                               % (path, line_of(src, start + m.start()),
                                  c, name))

    return bad, len(funcs)


def enclosing(funcs, pos):
    for name, start, end in funcs:
        if start <= pos < end:
            return name, start, end
    return None


def check_other(path):
    """Rules (d) to (f) for a module source that is neither jes.c nor policy."""
    path = os.path.relpath(path)
    with open(path, encoding="utf-8") as f:
        src = f.read()
    text = strip_noise(src)
    funcs = functions(text)
    bad = []

    for c in SPOOL_ACT + SPOOL_SEE:
        for m in re.finditer(r"\b%s\b" % c, text):
            line = line_of(src, m.start())
            fn = enclosing(funcs, m.start())
            if fn is None:
                bad.append("%s:%d: %s outside any function; the spool API "
                           "belongs behind %s() (rule d)"
                           % (path, line, c, POLICY_CALL))
                continue
            name, start, end = fn
            body = text[start:end]
            checks = [x.start() for x in
                      re.finditer(r"\b%s\s*\(" % POLICY_CALL, body)]
            if c in SPOOL_ACT:
                ok = bool(checks) and checks[0] < m.start() - start
            else:
                ok = bool(checks)
            if not ok:
                bad.append("%s:%d: %s in %s() without %s() %s (rule d)"
                           % (path, line, c, name, POLICY_CALL,
                              "before it" if c in SPOOL_ACT else "in it"))

    for m in re.finditer(r"\bracf_auth\s*\(", text):
        args = balanced(src, m.end() - 1)    # same layout: the literals show
        if re.search(r"JESSPOOL", args):
            bad.append("%s:%d: racf_auth() on JESSPOOL outside %s "
                       "(rule e)" % (path, line_of(src, m.start()), JES_FILE))

    if re.search(r"\bJESJOB\b", text) or \
            re.search(r"#\s*include\s*<mvs/jes2\.h>", src):
        for m in re.finditer(r"\b(?:strcmp|strncmp|memcmp)\s*\(", text):
            args = balanced(text, m.end() - 1)
            if re.search(r"->\s*owner\b", args):
                bad.append("%s:%d: owner compared outside ftpd_jwn_mine() "
                           "(rule f)" % (path, line_of(src, m.start())))

    return bad, len(funcs)


def sources_of(module, root):
    """The module's C sources, the way mbt reads `sources` and `exclude`."""
    files = []
    for pattern in module.get("sources", []):
        files += glob.glob(os.path.join(root, pattern))
    dropped = set()
    for pattern in module.get("exclude", []):
        dropped |= set(glob.glob(os.path.join(root, pattern)))
    return sorted(f for f in set(files) - dropped if f.endswith(".c"))


def main(argv):
    toml = argv[1] if len(argv) > 1 else "mbt.toml"
    root = os.path.dirname(os.path.abspath(toml)) or "."
    with open(toml, "rb") as fh:
        project = tomllib.load(fh)

    # mbt.toml keys modules by name: [module.FTPD], not [[module]] name = ...
    sources = set()
    for module in project.get("module", {}).values():
        sources.update(sources_of(module, root))
    by_name = {os.path.basename(p): p for p in sources}

    # A scan of nothing ends green; refuse that.
    missing = [n for n in (JES_FILE,) + ALLOW if n not in by_name]
    if not sources or missing:
        print("check-jes-access: %s lists %d module source(s); expected "
              "%s among them" % (toml, len(sources), ", ".join(missing)
                                 or JES_FILE), file=sys.stderr)
        return 2

    bad = []
    nfuncs = 0
    scanned = 0
    for path in sorted(sources):
        name = os.path.basename(path)
        if name in ALLOW:
            continue
        scanned += 1
        found, n = (check_jes if name == JES_FILE else check_other)(path)
        bad += found
        nfuncs += n

    if bad:
        print("check-jes-access: %d problem(s)" % len(bad), file=sys.stderr)
        for b in bad:
            print("  " + b, file=sys.stderr)
        return 1
    print("check-jes-access: %d module sources from %s: %d clean, %d "
          "allow-listed (%d functions)"
          % (len(sources), toml, scanned, len(sources) - scanned, nfuncs))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
