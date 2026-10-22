#!/usr/bin/env python3
"""Keep every path to a job's spool behind the spool policy (issue #90).

The JES mode of LIST, RETR and DELE decides per job whether the session may
see, read or delete it.  That decision lives in one place -- ftpd_jwn_check()
in ftpd#jwn.c, called through job_access() in ftpd#jes.c -- and this check
fails the build when ftpd#jes.c grows a way around it:

  (a) a strcmp/strncmp/memcmp on `->owner`.  The only ownership test is
      ftpd_jwn_mine(); a second one is how a prefix match (IBMUSER2 is not
      IBMUSER) comes back.
  (b) a call to racf_auth() anywhere but in jes_racf_auth(), the hull
      ftpd_jwn_check() asks through.
  (c) a call to jescanj() or ftpd_data_open() in a function that has not
      called job_access() or find_job() (which calls job_access()) earlier
      in its text.  Deleting a job or opening the data connection for its
      spool must come after the policy.

Exempt from (c), by name, and only these (an exemption is a change to this
file and therefore visible in the pull request):

  ftpd_jes_submit   submitting opens the data connection without any job
  ftpd_jes_list     opens the connection first; the policy runs per job in
                    the loop that follows

Limits
------
This is a text proxy: the order of the text, not dominance.  A conditional
call of job_access() satisfies (c) and is not flagged, and a renamed pointer
to racf_auth() is not seen.  Test 18 (test/test_ftpd.sh) and TSTJWN are the
proof; this stops the obvious regression.

Usage: tools/check-jes-access.py [src/ftpd#jes.c]
"""
import os
import re
import sys

ROOT = os.path.realpath(os.path.join(os.path.dirname(__file__), ".."))
DEFAULT = "src/ftpd#jes.c"

AUTH_HULL = "jes_racf_auth"
POLICY_CALLS = ("job_access", "find_job")
GUARDED_CALLS = ("jescanj", "ftpd_data_open")
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


def source_path(arg):
    """Resolve arg to a .c file inside the repository, or None."""
    full = os.path.realpath(os.path.join(ROOT, arg))
    if os.path.commonpath([ROOT, full]) != ROOT:
        return None
    if not full.endswith(".c") or not os.path.isfile(full):
        return None
    return full


def check(path):
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


def main(argv):
    arg = argv[1] if len(argv) > 1 else DEFAULT
    path = source_path(arg)
    if path is None:
        print("check-jes-access: %s is not a .c file inside the repository"
              % arg, file=sys.stderr)
        return 2
    bad, nfuncs = check(path)
    if bad:
        print("check-jes-access: %d problem(s)" % len(bad), file=sys.stderr)
        for b in bad:
            print("  " + b, file=sys.stderr)
        return 1
    print("check-jes-access: %s clean (%d functions)" % (path, nfuncs))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
