# Releasing FTPD

Moved here from `CLAUDE.md`. The ecosystem-wide rules (id format, burned
ids, `DELETE`, the element-ownership wall, `LIST`) are in
`~/repos/mvs/.workspace/context/smp.md`; this file holds what is particular
to ftpd. Read both before cutting a release.

## The SMP4 FMID

The id is the release: `T` + three product letters + the three version digits.
One id per release, **spent exactly once**, and each release's SYSMOD deletes
its predecessor:

```toml
[distribution.smp]
fmid   = "TFTP120"
delete = ["TFTP110"]
```

**No version component may ever exceed 9** — a 7-character id has no room for
a second digit. At patch 9 cut the next minor, at minor 9 the next major;
ftpd 1.2.10 cannot be expressed and must not be released.

Current: `main` is 1.3.0-dev, so mbt 3 derives **`TFTP130`**, deleting
`TFTP120`; not yet checked on any stand. `TFTP111` was assigned to a 1.1.1 that
was never cut (the libc370 2.0 port made the next release 1.2.0) and is unspent
and unassigned. Burned: `TFTP100` (1.0.0–1.0.2), `TFTP110` (1.1.0, released
2026-09-14) and `TFTP120` (1.2.0, released 2026-10-13; free on the maintainer's
word, no `LIST` was run on any stand before it shipped). `docs/uninstall.md`
names every spent id and carries `TFTP130`; add each later id there.

Never re-spend an id, and never install a test package under the real one: a
test needs a throwaway id **and** throwaway module names, because SMP keys
element ownership on `MOD(name)`, not on the target library. See the root
`CLAUDE.md` for the full rule and the measurements behind it.

## The books

ML06-0001 (User's Guide) and ML06-0002 (Reference) live in `docs/books/` and
are built with typst and the `bookmaster` submodule. The document numbers are
registered in the README of `mvslovers/bookmaster`. A release attaches both
PDFs to its release page with their `SHA256SUMS` lines (the rule agreed for
all products in `~/repos/mvs/.workspace/context/books.md`).

The release workflow (`.github/workflows/release.yml`, mbt's `release3.yml`)
builds and publishes the SMP package only. It does not build or attach the
books; that is a manual step after the tag exists.

Before the tag:

1. The books' part of the release is ready: every behaviour change in the
   release has its section, and the table "What Is New in FTPD 1.3" has one
   row per change. Check it against the closed issues and merged PRs of the
   milestone, and against the source, not against the PR texts.
2. `make -C docs/books` builds both PDFs without errors and without warnings
   beyond the two known ones from the template (the experimental bundle
   export and the outline that does not converge). The `books` CI job builds
   with the pinned typst; use the same version.
3. The edition notice in `docs/books/ml06-0001.typ` and
   `docs/books/ml06-0002.typ` names the release, and the "Draft" paragraph is
   removed or kept on purpose. A change to the edition digit (`-0` Draft,
   `-1` First Edition) also renames the PDFs in `docs/books/Makefile`.
4. `docs/installation.md` and `docs/uninstall.md` agree with the books on the
   libc370 level and the FMID. `docs/uninstall.md` holds the FMID table, the
   `DELETE` list of the dummy function `TFTPDEL` and the `UCLIN` jobs: add the
   new id to the `DELETE` list, to both `UCLIN` jobs and to their `LIST`
   statements. The jobs and captures there were measured on 1.2.0; run them
   once on a throwaway container with the release candidate installed from
   the SMP package, and compare the output with the text.

After the tag, with the release page created by the workflow:

```sh
make -C docs/books
sha256sum docs/books/ml06-0001-*.pdf docs/books/ml06-0002-*.pdf > SHA256SUMS
gh release upload vX.Y.Z docs/books/ml06-000[12]-*.pdf SHA256SUMS -R mvslovers/ftpd
```

Build from the tagged commit, in a clean checkout with the submodule
initialised at the pinned commit, so the PDFs can be rebuilt byte-for-byte
from the tag. Uploading to a release is a publication: it needs the
maintainer's go-ahead naming the release and the files, like the release
itself.
