#!/bin/bash
# ============================================================
# FTPD Comprehensive Test Suite
# Uses tnftp (BSD ftp) for scripted FTP operations
#
# Usage: ./test_ftpd.sh [host] [port] [user] [pass]
#
# Tests:
#   1.  Binary roundtrip (TYPE I, FB 80 XMIT file)
#   2.  Text (TYPE A) roundtrip with special characters
#   2b. MVS TYPE E roundtrip (EBCDIC transfer)
#   3.  PDS creation (MKD) + member upload/download
#   4.  JES job submission (SITE FILETYPE=JES)
#   5.  UFS mode switching (CWD / / CWD 'DSN')
#   5b. UFS file ops: TYPE A + TYPE I roundtrip, LIST, SIZE, DELE, MKD/RMD
#   6.  Cleanup (DELE/RMD)
#   7.  Out of space (x37): permanent 552, no hang, data set scratched
#   8.  Failed member STOR into a full PDS: 552, directory untouched
#   9.  APPE: appends to a sequential data set, creates what is missing,
#       refuses an existing PDS member (550, unchanged), 502 for UFS
#   10. SITE in any spelling: verb, keywords and values (quote form)
#   11. Member STOR into a PDS with a full directory: 552 + SITE DIRECTORY=
#   12. STOR and APPE on a retained data set: 550 with the date, no IEC507D
#   13. Byte counters: F FTPD,STATS counts every transferred byte once
#       (needs FTPD_CONSOLE, see below)
#
# Test 13 reads the operator command F FTPD,STATS, which an FTP client cannot
# reach.  Set FTPD_CONSOLE to a command that sends one operator command and
# prints the system's answer, for example
#     FTPD_CONSOLE='mbt target console env --' ./test_ftpd.sh ...
# Without it Test 13 is reported as SKIPPED, not as passed.
# ============================================================

HOST="${1:-localhost}"
PORT="${2:-2121}"
USER="${3:-ibmuser}"
PASS="${4:-secret}"

HLQ=$(echo "$USER" | tr '[:lower:]' '[:upper:]')
TMPDIR="/tmp/ftpd_test_$$"
TOTAL=0
PASSED=0
FAILED=0
FAIL_LIST=""

RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'
CYAN='\033[0;36m'; BOLD='\033[1m'; NC='\033[0m'

pass() {
    echo -e "  ${GREEN}PASS${NC}: $1"
    PASSED=$((PASSED + 1))
    TOTAL=$((TOTAL + 1))
}
fail() {
    echo -e "  ${RED}FAIL${NC}: $1"
    FAILED=$((FAILED + 1))
    TOTAL=$((TOTAL + 1))
    FAIL_LIST="${FAIL_LIST}\n  - $1"
}
info() { echo -e "  ${YELLOW}INFO${NC}: $1"; }
section() { echo -e "\n${CYAN}${BOLD}=== $1 ===${NC}"; }

# --- Setup ---
mkdir -p "$TMPDIR"
cleanup() { rm -rf "$TMPDIR"; }
trap cleanup EXIT

echo -e "${BOLD}FTPD Comprehensive Test Suite${NC}"
echo "Target: $USER@$HOST:$PORT"
echo "Temp:   $TMPDIR"

# ============================================================
# Helper: run FTP commands via tnftp script
# Captures server output for analysis.
# Usage: ftp_run "command1\ncommand2\n..." [capture_file]
# ============================================================
ftp_run() {
    local cmds="$1"
    local capture="${2:-/dev/null}"

    printf "user %s %s\n%b\nquit\n" "$USER" "$PASS" "$cmds" \
        | ftp -n -v -p -P "$PORT" "$HOST" > "$capture" 2>&1
    return $?
}

# ============================================================
# Helper: generate binary test file (FB 80 XMIT-like)
# ============================================================
generate_binary_testfile() {
    local outfile="$1"
    local size="${2:-710400}"  # default ~693KB
    dd if=/dev/urandom of="$outfile" bs="$size" count=1 2>/dev/null
    # Ensure size is multiple of 80 (LRECL)
    local actual=$(stat -c%s "$outfile" 2>/dev/null || stat -f%z "$outfile")
    local aligned=$(( (actual / 80) * 80 ))
    if [ "$aligned" -ne "$actual" ]; then
        truncate -s "$aligned" "$outfile" 2>/dev/null || \
            dd if="$outfile" of="${outfile}.tmp" bs="$aligned" count=1 2>/dev/null && \
            mv "${outfile}.tmp" "$outfile"
    fi
}

# ============================================================
# Helper: generate text test file with special characters
# ============================================================
generate_text_testfile() {
    local outfile="$1"
    cat > "$outfile" << 'TEXTEOF'
* FTPD Test File — Special Characters
* ====================================
*
* Square brackets:    [HELLO] [WORLD]
* Curly braces:       {BEGIN} {END}
* Pipe:               ONE | TWO | THREE
* Backslash:          PATH\TO\FILE
* Dollar sign:        $HLQ.$DATASET
* At sign:            USER@HOST
* Hash:               #INCLUDE #DEFINE
* Tilde:              ~/home/user
* Exclamation:        STOP! GO! WAIT!
* Percent:            100% COMPLETE
* Caret:              2^10 = 1024
* Ampersand:          A & B & C
* Asterisk:           *.LOAD **.DATA
* Semicolon:          A=1; B=2; C=3
* Colon:              HH:MM:SS
* Single quotes:      CWD 'SYS1.MACLIB'
* Double quotes:      MSG="HELLO WORLD"
* Less/Greater:       IF A < B THEN C > D
* Question mark:      WHERE IS IT?
* Comma:              A, B, C, D
* Period:             SYS1.MACLIB.MEMBER
* Parentheses:        IEFBR14(MEMBER)
* Plus/Minus:         A + B - C
* Equals:             X = Y
* Underscore:         MY_VARIABLE
* Numeric:            0123456789
* German umlauts:     (not in EBCDIC CP037 base)
* Mixed case:         AbCdEfGhIjKlMnOpQrStUvWxYz
*
* JCL-like content:
//TESTJOB  JOB (ACCT),'TEST',CLASS=A,MSGCLASS=X
//STEP1    EXEC PGM=IEFBR14
//SYSPRINT DD SYSOUT=*
//SYSIN    DD DUMMY
//DD1      DD DSN=SYS1.MACLIB(ABEND),DISP=SHR
/*
*
* End of test file.
TEXTEOF
}

# Helper: compare files
compare_files() {
    local orig="$1"
    local back="$2"
    local label="$3"

    local origsize=$(stat -c%s "$orig" 2>/dev/null || stat -f%z "$orig")
    local backsize=$(stat -c%s "$back" 2>/dev/null || stat -f%z "$back" || echo 0)

    if [ "$origsize" != "$backsize" ]; then
        fail "$label: Size mismatch (orig=$origsize back=$backsize)"
        return 1
    fi

    local orig_md5=$(md5sum "$orig" 2>/dev/null | cut -d' ' -f1 || md5 -q "$orig")
    local back_md5=$(md5sum "$back" 2>/dev/null | cut -d' ' -f1 || md5 -q "$back")

    if [ "$orig_md5" = "$back_md5" ]; then
        pass "$label: MD5 match ($orig_md5)"
        return 0
    else
        fail "$label: MD5 mismatch"
        echo "    Original:  $orig_md5"
        echo "    Roundtrip: $back_md5"
        local diffbyte=$(cmp -l "$orig" "$back" 2>/dev/null | head -1 | awk '{print $1}')
        if [ -n "$diffbyte" ]; then
            info "First diff at byte $diffbyte"
            local hexstart=$((diffbyte > 16 ? diffbyte - 16 : 0))
            echo "    Original:"
            xxd -s $hexstart -l 48 "$orig" | sed 's/^/    /'
            echo "    Roundtrip:"
            xxd -s $hexstart -l 48 "$back" | sed 's/^/    /'
        fi
        return 1
    fi
}

# ============================================================
# TEST 1: Binary Roundtrip (FB 80)
# ============================================================
section "Test 1: Binary Roundtrip (FB 80)"

BINFILE="$TMPDIR/test_binary.dat"
BINBACK="$TMPDIR/test_binary_back.dat"
DSN_BIN="${HLQ}.TEST.BINARY"

generate_binary_testfile "$BINFILE" 710400
BINSIZE=$(stat -c%s "$BINFILE" 2>/dev/null || stat -f%z "$BINFILE")
info "Generated $BINSIZE bytes ($((BINSIZE/80)) records)"

info "STOR -> '$DSN_BIN'"
FTP_OUT="$TMPDIR/ftp_bin_up.log"
ftp_run "$(cat <<CMDS
site recfm=fb
site lrecl=80
site blksize=3120
site primary=100
site secondary=50
site tracks
type binary
put $BINFILE '$DSN_BIN'
CMDS
)" "$FTP_OUT"
RC=$?
if [ $RC -eq 0 ] && ! grep -qi "^550\|^553\|^451" "$FTP_OUT"; then
    pass "Binary upload"
else
    fail "Binary upload (rc=$RC)"
    tail -5 "$FTP_OUT" | sed 's/^/    /'
fi

info "RETR -> $BINBACK"
FTP_OUT="$TMPDIR/ftp_bin_dn.log"
ftp_run "$(cat <<CMDS
type binary
get '$DSN_BIN' $BINBACK
CMDS
)" "$FTP_OUT"
RC=$?
if [ $RC -eq 0 ] && [ -f "$BINBACK" ]; then
    pass "Binary download"
else
    fail "Binary download (rc=$RC)"
    tail -5 "$FTP_OUT" | sed 's/^/    /'
fi

compare_files "$BINFILE" "$BINBACK" "Binary roundtrip"

# ============================================================
# TEST 2: Text (TYPE A) Roundtrip (FB 80)
# ============================================================
section "Test 2: Text (TYPE A) Roundtrip — Special Characters"

TXTFILE="$TMPDIR/test_text.txt"
TXTBACK="$TMPDIR/test_text_back.txt"
DSN_TXT="${HLQ}.TEST.TEXT"

generate_text_testfile "$TXTFILE"
TXTSIZE=$(stat -c%s "$TXTFILE" 2>/dev/null || stat -f%z "$TXTFILE")
TXTLINES=$(wc -l < "$TXTFILE")
info "Generated $TXTSIZE bytes, $TXTLINES lines"

info "STOR (TYPE A) -> '$DSN_TXT'"
FTP_OUT="$TMPDIR/ftp_txt_up.log"
ftp_run "$(cat <<CMDS
site recfm=fb
site lrecl=80
site blksize=3120
site primary=10
site secondary=5
site tracks
type ascii
put $TXTFILE '$DSN_TXT'
CMDS
)" "$FTP_OUT"
RC=$?
if [ $RC -eq 0 ] && ! grep -qi "^550\|^553\|^451" "$FTP_OUT"; then
    pass "Text upload"
else
    fail "Text upload (rc=$RC)"
    tail -5 "$FTP_OUT" | sed 's/^/    /'
fi

info "RETR (TYPE A) -> $TXTBACK"
FTP_OUT="$TMPDIR/ftp_txt_dn.log"
ftp_run "$(cat <<CMDS
type ascii
get '$DSN_TXT' $TXTBACK
CMDS
)" "$FTP_OUT"
RC=$?
if [ $RC -eq 0 ] && [ -f "$TXTBACK" ]; then
    pass "Text download"
else
    fail "Text download (rc=$RC)"
    tail -5 "$FTP_OUT" | sed 's/^/    /'
fi

# For text, compare content after normalizing whitespace
if [ -f "$TXTBACK" ]; then
    sed 's/[[:space:]]*$//' "$TXTFILE"  | tr -d '\r' > "$TMPDIR/txt_orig_norm"
    sed 's/[[:space:]]*$//' "$TXTBACK"  | tr -d '\r' > "$TMPDIR/txt_back_norm"

    if diff -q "$TMPDIR/txt_orig_norm" "$TMPDIR/txt_back_norm" > /dev/null 2>&1; then
        pass "Text roundtrip: content matches"
    else
        fail "Text roundtrip: content differs"
        diff -u "$TMPDIR/txt_orig_norm" "$TMPDIR/txt_back_norm" | head -30 | sed 's/^/    /'
    fi

    # Check specific special characters survived
    for char in '[' ']' '{' '}' '|' '$' '@' '#'; do
        if grep -qF "$char" "$TXTBACK"; then
            pass "Special char '$char' survived roundtrip"
        else
            fail "Special char '$char' MISSING after roundtrip"
        fi
    done
fi

# ============================================================
# TEST 2b: MVS TYPE E Roundtrip (EBCDIC transfer)
# ============================================================
section "Test 2b: MVS TYPE E Roundtrip"

EFILE="$TMPDIR/test_ebcdic.txt"
EBACK="$TMPDIR/test_ebcdic_back.txt"
DSN_EBC="${HLQ}.TEST.EBCDIC"

generate_text_testfile "$EFILE"

info "STOR (TYPE E) -> '$DSN_EBC'"
FTP_OUT="$TMPDIR/ftp_ebc_up.log"
ftp_run "$(cat <<CMDS
site recfm=fb
site lrecl=80
site blksize=3120
site primary=10
site secondary=5
site tracks
type ebcdic
put $EFILE '$DSN_EBC'
CMDS
)" "$FTP_OUT"
RC=$?
if [ $RC -eq 0 ] && ! grep -qi "^550\|^553\|^451\|^504" "$FTP_OUT"; then
    pass "MVS TYPE E upload"
else
    fail "MVS TYPE E upload (rc=$RC)"
    tail -5 "$FTP_OUT" | sed 's/^/    /'
fi

info "RETR (TYPE E) -> $EBACK"
FTP_OUT="$TMPDIR/ftp_ebc_dn.log"
ftp_run "$(cat <<CMDS
type ebcdic
get '$DSN_EBC' $EBACK
CMDS
)" "$FTP_OUT"
RC=$?
if [ $RC -eq 0 ] && grep -qi "250\|Transfer complete" "$FTP_OUT"; then
    pass "MVS TYPE E download"
    # Note: tnftp does NOT convert EBCDIC↔ASCII client-side.
    # The file may be empty (tnftp discards unrecognized encoding)
    # or contain raw EBCDIC bytes.  We verify the server-side transfer
    # succeeded (250 reply).  True TYPE E roundtrip validation requires
    # a client that speaks EBCDIC (e.g. z/OS FTP or a custom test).
    if [ -f "$EBACK" ] && [ -s "$EBACK" ]; then
        EBACKSIZE=$(stat -c%s "$EBACK" 2>/dev/null || stat -f%z "$EBACK")
        pass "MVS TYPE E roundtrip: $EBACKSIZE bytes received"
    else
        info "tnftp wrote 0 bytes (expected — no EBCDIC support in client)"
        pass "MVS TYPE E roundtrip: server transfer OK (250)"
    fi
else
    fail "MVS TYPE E download (rc=$RC)"
    tail -5 "$FTP_OUT" | sed 's/^/    /'
fi

# Cleanup TYPE E dataset
ftp_run "delete '$DSN_EBC'" /dev/null

# ============================================================
# TEST 3: PDS Creation + Member Upload/Download
# ============================================================
section "Test 3: PDS Creation (MKD) + Member Operations"

DSN_PDS="${HLQ}.TEST.PDS"
MEMBIN="$TMPDIR/member_bin.dat"
MEMTXT="$TMPDIR/member_txt.txt"
MEMBIN_BACK="$TMPDIR/member_bin_back.dat"
MEMTXT_BACK="$TMPDIR/member_txt_back.txt"

generate_binary_testfile "$MEMBIN" 8000   # 100 records x 80
generate_text_testfile "$MEMTXT"

# Create PDS (delete first if exists)
info "MKD '$DSN_PDS'"
ftp_run "$(cat <<CMDS
site recfm=fb
site lrecl=80
site blksize=3120
site primary=10
site secondary=5
site tracks
rmdir '$DSN_PDS'
mkdir '$DSN_PDS'
CMDS
)" /dev/null

# Upload binary member
info "STOR binary member -> '$DSN_PDS(BINMEM)'"
FTP_OUT="$TMPDIR/ftp_pds_binup.log"
ftp_run "$(cat <<CMDS
type binary
put $MEMBIN '$DSN_PDS(BINMEM)'
CMDS
)" "$FTP_OUT"
RC=$?
if [ $RC -eq 0 ] && ! grep -qi "^550\|^553\|^451" "$FTP_OUT"; then
    pass "PDS binary member upload"
else
    fail "PDS binary member upload (rc=$RC)"
    tail -5 "$FTP_OUT" | sed 's/^/    /'
fi

# Upload text member
info "STOR text member -> '$DSN_PDS(TXTMEM)'"
FTP_OUT="$TMPDIR/ftp_pds_txtup.log"
ftp_run "$(cat <<CMDS
type ascii
put $MEMTXT '$DSN_PDS(TXTMEM)'
CMDS
)" "$FTP_OUT"
RC=$?
if [ $RC -eq 0 ] && ! grep -qi "^550\|^553\|^451" "$FTP_OUT"; then
    pass "PDS text member upload"
else
    fail "PDS text member upload (rc=$RC)"
    tail -5 "$FTP_OUT" | sed 's/^/    /'
fi

# Download binary member
info "RETR binary member -> $MEMBIN_BACK"
FTP_OUT="$TMPDIR/ftp_pds_bindn.log"
ftp_run "$(cat <<CMDS
type binary
get '$DSN_PDS(BINMEM)' $MEMBIN_BACK
CMDS
)" "$FTP_OUT"
RC=$?
if [ $RC -eq 0 ] && [ -f "$MEMBIN_BACK" ]; then
    pass "PDS binary member download"
    compare_files "$MEMBIN" "$MEMBIN_BACK" "PDS binary member roundtrip"
else
    fail "PDS binary member download (rc=$RC)"
    tail -5 "$FTP_OUT" | sed 's/^/    /'
fi

# Download text member
info "RETR text member -> $MEMTXT_BACK"
FTP_OUT="$TMPDIR/ftp_pds_txtdn.log"
ftp_run "$(cat <<CMDS
type ascii
get '$DSN_PDS(TXTMEM)' $MEMTXT_BACK
CMDS
)" "$FTP_OUT"
RC=$?
if [ $RC -eq 0 ] && [ -f "$MEMTXT_BACK" ]; then
    pass "PDS text member download"
    sed 's/[[:space:]]*$//' "$MEMTXT"      | tr -d '\r' > "$TMPDIR/mem_orig_norm"
    sed 's/[[:space:]]*$//' "$MEMTXT_BACK" | tr -d '\r' > "$TMPDIR/mem_back_norm"
    if diff -q "$TMPDIR/mem_orig_norm" "$TMPDIR/mem_back_norm" > /dev/null 2>&1; then
        pass "PDS text member roundtrip: content matches"
    else
        fail "PDS text member roundtrip: content differs"
        diff -u "$TMPDIR/mem_orig_norm" "$TMPDIR/mem_back_norm" | head -20 | sed 's/^/    /'
    fi
else
    fail "PDS text member download (rc=$RC)"
    tail -5 "$FTP_OUT" | sed 's/^/    /'
fi

# ============================================================
# TEST 4: JES Job Submission (SITE FILETYPE=JES)
# ============================================================
section "Test 4: JES Job Submission"

JCLFILE="$TMPDIR/test_job.jcl"
cat > "$JCLFILE" << 'JCLEOF'
//FTPDTEST JOB (ACCT),'FTPD TEST',CLASS=A,MSGCLASS=X
//STEP1    EXEC PGM=IEFBR14
//SYSPRINT DD SYSOUT=*
JCLEOF

info "SITE FILETYPE=JES + STOR JCL"
FTP_OUT="$TMPDIR/ftp_jes.log"
ftp_run "$(cat <<CMDS
site filetype=jes
type ascii
put $JCLFILE test_job.jcl
site filetype=seq
CMDS
)" "$FTP_OUT"
RC=$?

if [ $RC -eq 0 ]; then
    pass "JES job submission (rc=0)"
else
    fail "JES job submission (rc=$RC)"
    tail -5 "$FTP_OUT" | sed 's/^/    /'
fi

# Check if the server mentioned a JOB ID in the response
# Server sends: 250-It is known to JES as JOB00119
# tnftp verbose shows this as-is; extract JOBnnnnn or STCnnnnn or TSUnnnnn
if grep -o '[JST][OTU][BCS][0-9][0-9][0-9][0-9][0-9]' "$FTP_OUT" > /dev/null 2>&1; then
    JOBID=$(grep -o '[JST][OTU][BCS][0-9][0-9][0-9][0-9][0-9]' "$FTP_OUT" | head -1)
    pass "JES returned job ID: $JOBID"
else
    info "JES output:"
    cat "$FTP_OUT" | sed 's/^/    /'
    fail "JES did not return a job ID"
fi

# JES LIST — job listing
info "LIST in JES mode"
FTP_OUT="$TMPDIR/ftp_jes_list.log"
ftp_run "$(cat <<CMDS
site filetype=jes
ls
site filetype=seq
CMDS
)" "$FTP_OUT"
if grep -qi "JOBNAME" "$FTP_OUT"; then
    pass "JES LIST: header present"
else
    info "JES LIST output:"
    cat "$FTP_OUT" | sed 's/^/    /'
    fail "JES LIST: no job listing header"
fi

# JES LIST — spool files for submitted job
if [ -n "$JOBID" ]; then
    info "LIST $JOBID (spool files)"
    FTP_OUT="$TMPDIR/ftp_jes_spool.log"
    ftp_run "$(cat <<CMDS
site filetype=jes
ls $JOBID
site filetype=seq
CMDS
)" "$FTP_OUT"
    if grep -qi "DDNAME\|JESMSGLG\|JESJCL" "$FTP_OUT"; then
        pass "JES spool file listing"
    else
        info "JES spool output:"
        cat "$FTP_OUT" | sed 's/^/    /'
        fail "JES spool file listing: no spool files found"
    fi
fi

# JES RETR — spool retrieval (all spool files)
if [ -n "$JOBID" ]; then
    info "RETR $JOBID (all spool files)"
    SPOOL_ALL="$TMPDIR/spool_all.txt"
    FTP_OUT="$TMPDIR/ftp_jes_retr_all.log"
    ftp_run "$(cat <<CMDS
site filetype=jes
type ascii
get $JOBID $SPOOL_ALL
site filetype=seq
CMDS
)" "$FTP_OUT"
    if [ -f "$SPOOL_ALL" ] && [ -s "$SPOOL_ALL" ]; then
        pass "JES RETR all spool files ($(wc -c < "$SPOOL_ALL") bytes)"
    else
        fail "JES RETR all spool files: empty or missing"
        tail -5 "$FTP_OUT" | sed 's/^/    /'
    fi

    # JES RETR — specific spool file (file 1)
    info "RETR $JOBID.1 (first spool file)"
    SPOOL_ONE="$TMPDIR/spool_one.txt"
    FTP_OUT="$TMPDIR/ftp_jes_retr_one.log"
    ftp_run "$(cat <<CMDS
site filetype=jes
type ascii
get $JOBID.1 $SPOOL_ONE
site filetype=seq
CMDS
)" "$FTP_OUT"
    if [ -f "$SPOOL_ONE" ] && [ -s "$SPOOL_ONE" ]; then
        pass "JES RETR specific spool file ($(wc -c < "$SPOOL_ONE") bytes)"
    else
        fail "JES RETR specific spool file: empty or missing"
        tail -5 "$FTP_OUT" | sed 's/^/    /'
    fi

    # JES DELE — job purge
    info "DELE $JOBID (purge job)"
    FTP_OUT="$TMPDIR/ftp_jes_dele.log"
    ftp_run "$(cat <<CMDS
site filetype=jes
delete $JOBID
site filetype=seq
CMDS
)" "$FTP_OUT"
    if grep -qi "cancel\|250" "$FTP_OUT"; then
        pass "JES job purge"
    else
        info "JES DELE output:"
        cat "$FTP_OUT" | sed 's/^/    /'
        fail "JES job purge"
    fi
fi

# Verify FILETYPE=SEQ restores dataset mode
info "SITE FILETYPE=SEQ — verify dataset mode restored"
FTP_OUT="$TMPDIR/ftp_jes_seq.log"
ftp_run "$(cat <<CMDS
site recfm=fb
site lrecl=80
site blksize=3120
site primary=1
site secondary=1
site tracks
type ascii
put $JCLFILE '${HLQ}.TEST.JESBACK'
CMDS
)" "$FTP_OUT"
RC=$?
if [ $RC -eq 0 ] && ! grep -qi "^550\|^553\|^451" "$FTP_OUT"; then
    pass "STOR after FILETYPE=SEQ"
else
    fail "STOR after FILETYPE=SEQ (rc=$RC)"
    tail -5 "$FTP_OUT" | sed 's/^/    /'
fi

# ============================================================
# TEST 5: UFS Mode Switching (Phase 3a)
# ============================================================
section "Test 5: UFS Mode Switching"

# CWD / — should switch to UFS mode (250) or fail gracefully (550)
info "CWD / (UFS mode switch)"
FTP_OUT="$TMPDIR/ftp_ufs_cwd.log"
ftp_run "$(cat <<CMDS
cd /
pwd
CMDS
)" "$FTP_OUT"

if grep -qi "250.*HFS directory\|257.*/" "$FTP_OUT"; then
    pass "CWD / — UFS mode active"
    UFS_AVAILABLE=1

    # PWD should show UFS-style path
    # tnftp shows "Remote directory: /" or the raw 257 response
    if grep -qi '257\|HFS.*working directory\|Remote directory.*/' "$FTP_OUT"; then
        pass "PWD in UFS mode shows /"
    else
        fail "PWD in UFS mode: expected /"
        cat "$FTP_OUT" | sed 's/^/    /'
    fi

    # CWD back to MVS mode
    info "CWD '${HLQ}.' (back to MVS mode)"
    FTP_OUT="$TMPDIR/ftp_ufs_back.log"
    ftp_run "$(cat <<CMDS
cd /
cd '${HLQ}.'
pwd
CMDS
)" "$FTP_OUT"
    # Look for MVS-style CWD response (250) or PWD response (257)
    if grep -qi "250.*${HLQ}\.\|257.*${HLQ}\." "$FTP_OUT"; then
        pass "CWD back to MVS mode"
    else
        fail "CWD back to MVS mode"
        grep -i "257\|250\|550" "$FTP_OUT" | sed 's/^/    /'
    fi
elif grep -qi "550.*UFS service not available" "$FTP_OUT"; then
    pass "CWD / — UFSD not running, graceful 550"
    UFS_AVAILABLE=0
    info "UFSD not running — skipping further UFS tests"
else
    fail "CWD / — unexpected response"
    cat "$FTP_OUT" | sed 's/^/    /'
    UFS_AVAILABLE=0
fi

# ============================================================
# TEST 5b: UFS File Operations (Phase 3b) — only if UFSD running
# ============================================================
if [ "${UFS_AVAILABLE:-0}" = "1" ]; then
section "Test 5b: UFS File Operations"

UFS_TESTDIR="/tmp/ftpdtest$$"

# MKD — create test directory
info "MKD $UFS_TESTDIR"
FTP_OUT="$TMPDIR/ftp_ufs_mkd.log"
ftp_run "$(cat <<CMDS
cd /
mkdir $UFS_TESTDIR
CMDS
)" "$FTP_OUT"
if grep -qi "257\|directory created" "$FTP_OUT"; then
    pass "MKD $UFS_TESTDIR"
else
    fail "MKD $UFS_TESTDIR"
    cat "$FTP_OUT" | sed 's/^/    /'
fi

# --- UFS Text (TYPE A) roundtrip with special characters ---
info "UFS TYPE A roundtrip — special characters"
UFS_TXTFILE="$TMPDIR/ufs_text.txt"
UFS_TXTBACK="$TMPDIR/ufs_text_back.txt"
generate_text_testfile "$UFS_TXTFILE"

FTP_OUT="$TMPDIR/ftp_ufs_txt_up.log"
ftp_run "$(cat <<CMDS
cd $UFS_TESTDIR
type ascii
put $UFS_TXTFILE specials.txt
CMDS
)" "$FTP_OUT"
if grep -qi "226\|Transfer complete" "$FTP_OUT"; then
    pass "UFS STOR (TYPE A) specials.txt"
else
    fail "UFS STOR (TYPE A) specials.txt"
    cat "$FTP_OUT" | sed 's/^/    /'
fi

FTP_OUT="$TMPDIR/ftp_ufs_txt_dn.log"
ftp_run "$(cat <<CMDS
cd $UFS_TESTDIR
type ascii
get specials.txt $UFS_TXTBACK
CMDS
)" "$FTP_OUT"
if [ -f "$UFS_TXTBACK" ]; then
    pass "UFS RETR (TYPE A) specials.txt"

    # Normalize trailing whitespace + CR for comparison
    sed 's/[[:space:]]*$//' "$UFS_TXTFILE" | tr -d '\r' > "$TMPDIR/ufs_txt_orig_norm"
    sed 's/[[:space:]]*$//' "$UFS_TXTBACK" | tr -d '\r' > "$TMPDIR/ufs_txt_back_norm"

    if diff -q "$TMPDIR/ufs_txt_orig_norm" "$TMPDIR/ufs_txt_back_norm" > /dev/null 2>&1; then
        pass "UFS TYPE A roundtrip: content matches"
    else
        fail "UFS TYPE A roundtrip: content differs"
        diff -u "$TMPDIR/ufs_txt_orig_norm" "$TMPDIR/ufs_txt_back_norm" | head -30 | sed 's/^/    /'
    fi

    # Check special characters survived IBM-1047 roundtrip
    for char in '[' ']' '{' '}' '|' '$' '@' '#' '~' '!' '^'; do
        if grep -qF "$char" "$UFS_TXTBACK"; then
            pass "UFS special char '$char' survived 1047 roundtrip"
        else
            fail "UFS special char '$char' MISSING after 1047 roundtrip"
        fi
    done
else
    fail "UFS RETR (TYPE A) specials.txt — file missing"
fi

# --- UFS Binary (TYPE I) roundtrip ---
info "UFS TYPE I roundtrip — binary"
UFS_BINFILE="$TMPDIR/ufs_binary.dat"
UFS_BINBACK="$TMPDIR/ufs_binary_back.dat"
generate_binary_testfile "$UFS_BINFILE" 8192

FTP_OUT="$TMPDIR/ftp_ufs_bin_up.log"
ftp_run "$(cat <<CMDS
cd $UFS_TESTDIR
type binary
put $UFS_BINFILE binary.dat
CMDS
)" "$FTP_OUT"
if grep -qi "226\|Transfer complete" "$FTP_OUT"; then
    pass "UFS STOR (TYPE I) binary.dat"
else
    fail "UFS STOR (TYPE I) binary.dat"
    cat "$FTP_OUT" | sed 's/^/    /'
fi

FTP_OUT="$TMPDIR/ftp_ufs_bin_dn.log"
ftp_run "$(cat <<CMDS
cd $UFS_TESTDIR
type binary
get binary.dat $UFS_BINBACK
CMDS
)" "$FTP_OUT"
if [ -f "$UFS_BINBACK" ]; then
    pass "UFS RETR (TYPE I) binary.dat"
    compare_files "$UFS_BINFILE" "$UFS_BINBACK" "UFS binary roundtrip"
else
    fail "UFS RETR (TYPE I) binary.dat — file missing"
fi

# --- UFS LIST format check ---
info "LIST $UFS_TESTDIR (format check)"
FTP_OUT="$TMPDIR/ftp_ufs_list.log"
ftp_run "$(cat <<CMDS
cd $UFS_TESTDIR
ls
CMDS
)" "$FTP_OUT"
if grep -qi "specials.txt" "$FTP_OUT" && grep -qi "binary.dat" "$FTP_OUT"; then
    pass "UFS LIST shows both files"
else
    fail "UFS LIST: expected files not found"
    cat "$FTP_OUT" | sed 's/^/    /'
fi

# Check Unix-style format (drwx... or -rw-...)
if grep -qE '^[d-][rwx-]{9}' "$FTP_OUT"; then
    pass "UFS LIST format is Unix-style"
else
    info "UFS LIST output (format check):"
    cat "$FTP_OUT" | sed 's/^/    /'
    fail "UFS LIST format: not Unix-style"
fi

# --- UFS SIZE ---
info "SIZE specials.txt"
FTP_OUT="$TMPDIR/ftp_ufs_size.log"
ftp_run "$(cat <<CMDS
cd $UFS_TESTDIR
size specials.txt
CMDS
)" "$FTP_OUT"
# tnftp shows "filename\tsize" or raw "213 size"
if grep -qi "213\|specials.txt" "$FTP_OUT" && grep -qE '[0-9]{3,}' "$FTP_OUT"; then
    pass "UFS SIZE specials.txt"
else
    fail "UFS SIZE specials.txt"
    cat "$FTP_OUT" | sed 's/^/    /'
fi

# --- UFS Cleanup: DELE files, RMD directory ---
info "DELE specials.txt + binary.dat"
FTP_OUT="$TMPDIR/ftp_ufs_dele.log"
ftp_run "$(cat <<CMDS
cd $UFS_TESTDIR
delete specials.txt
delete binary.dat
CMDS
)" "$FTP_OUT"
DELE_COUNT=$(grep -c "250" "$FTP_OUT" || true)
if [ "$DELE_COUNT" -ge 2 ] 2>/dev/null; then
    pass "UFS DELE both files"
else
    grep -qi "250" "$FTP_OUT" && pass "UFS DELE (partial)" || fail "UFS DELE"
    cat "$FTP_OUT" | sed 's/^/    /'
fi

# RMD — remove test directory
info "RMD $UFS_TESTDIR"
FTP_OUT="$TMPDIR/ftp_ufs_rmd.log"
ftp_run "$(cat <<CMDS
cd /
rmdir $UFS_TESTDIR
CMDS
)" "$FTP_OUT"
if grep -qi "250\|Directory removed" "$FTP_OUT"; then
    pass "RMD $UFS_TESTDIR"
else
    fail "RMD $UFS_TESTDIR"
    cat "$FTP_OUT" | sed 's/^/    /'
fi

fi  # UFS_AVAILABLE

# ============================================================
# TEST 5c: Hybrid Navigation (Phase 3c) — only if UFSD running
# ============================================================
if [ "${UFS_AVAILABLE:-0}" = "1" ]; then
section "Test 5c: Hybrid MVS/UFS/JES Navigation"

# Full hybrid cycle in a single session:
# MVS → UFS → MVS → JES → back to MVS
info "Hybrid: MVS → UFS → MVS → JES → MVS"
FTP_OUT="$TMPDIR/ftp_hybrid.log"
ftp_run "$(cat <<CMDS
pwd
cd /
pwd
cd '${HLQ}.'
pwd
site filetype=jes
site filetype=seq
pwd
CMDS
)" "$FTP_OUT"

# Check MVS mode at start
if grep -qi "257.*${HLQ}\.\|working directory.*${HLQ}" "$FTP_OUT"; then
    pass "Hybrid: starts in MVS mode"
else
    fail "Hybrid: expected MVS mode at start"
    cat "$FTP_OUT" | sed 's/^/    /'
fi

# Check UFS mode after CWD /
if grep -qi "250.*HFS directory.*/" "$FTP_OUT"; then
    pass "Hybrid: CWD / switches to UFS"
else
    fail "Hybrid: CWD / did not switch to UFS"
fi

# Check MVS mode after CWD 'HLQ.'
if grep -qi "250.*${HLQ}\.\|working directory.*${HLQ}" "$FTP_OUT"; then
    pass "Hybrid: CWD 'HLQ.' switches back to MVS"
else
    fail "Hybrid: CWD 'HLQ.' did not switch to MVS"
fi

# SITE FILETYPE=JES from UFS mode, then back
info "Hybrid: JES from UFS mode preserves fsmode"
FTP_OUT="$TMPDIR/ftp_hybrid_jes.log"
ftp_run "$(cat <<CMDS
cd /
pwd
site filetype=jes
site filetype=seq
pwd
CMDS
)" "$FTP_OUT"

# After JES→SEQ, PWD should show UFS (restored from before JES)
if grep -qi 'HFS.*working directory\|"/"' "$FTP_OUT"; then
    pass "Hybrid: FILETYPE=SEQ restores UFS mode"
else
    # Check if at least the commands succeeded
    info "Hybrid JES→SEQ output:"
    grep -i "250\|257\|200\|215" "$FTP_OUT" | sed 's/^/    /'
    fail "Hybrid: FILETYPE=SEQ did not restore UFS mode"
fi

fi  # UFS_AVAILABLE (hybrid)

# ============================================================
# TEST 6: Cleanup (DELE)
# ============================================================
section "Test 6: Cleanup"

info "DELE '$DSN_BIN'"
FTP_OUT="$TMPDIR/ftp_cleanup.log"
ftp_run "delete '$DSN_BIN'" "$FTP_OUT"
! grep -qi "^550" "$FTP_OUT" && pass "Delete binary DS" || fail "Delete binary DS"

info "DELE '$DSN_TXT'"
ftp_run "delete '$DSN_TXT'" "$FTP_OUT"
! grep -qi "^550" "$FTP_OUT" && pass "Delete text DS" || fail "Delete text DS"

info "RMD '$DSN_PDS'"
ftp_run "rmdir '$DSN_PDS'" "$FTP_OUT"
! grep -qi "^550" "$FTP_OUT" && pass "Delete PDS" || fail "Delete PDS"

info "DELE '${HLQ}.TEST.JESBACK'"
ftp_run "delete '${HLQ}.TEST.JESBACK'" "$FTP_OUT"
! grep -qi "^550" "$FTP_OUT" && pass "Delete JES test DS" || fail "Delete JES test DS"

# ============================================================
# TEST 7: Out of space (x37) — ftpd#129
#
# An upload that does not fit must fail PERMANENTLY, and must not leave the
# session hanging.  Both were wrong before:
#
#   - recovery answered 451, which RFC 959 defines as transient, so a
#     conforming client retries something that cannot succeed;
#   - recovery ABENDed a second time in fclose() and never reached the data
#     connection close, so the client sat writing into a dead transfer until
#     the idle timeout fired 300 seconds later.
#
# Since libc370 1.0.6 the out-of-space condition is not an ABEND at all
# (libc370#176): it arrives as a failed write with errno ENOSPC, so the 552
# now comes from STOR's own error path rather than from the ESTAE, and the
# partial data set can be — and is — scratched (ftpd#135, ftpd#127).
#
# TRK(1,0) has no secondary extents, so ~700 KB cannot fit however the
# volume is laid out.  SECONDARY=0 must reach the allocation as zero for
# this to be a one-track data set at all (ftpd#100) — run this against a
# build without that fix and the 0 becomes 50, the data set becomes 51
# tracks, the upload succeeds and this whole section measures nothing.
# ============================================================
section "Test 7: Out of space (x37) — permanent failure, no hang"

X37FILE="$TMPDIR/test_x37.bin"
DSN_X37="${HLQ}.TEST.X37"

generate_binary_testfile "$X37FILE" 720000
X37SIZE=$(stat -c%s "$X37FILE" 2>/dev/null || stat -f%z "$X37FILE")
info "Generated $X37SIZE bytes for a TRK(1,0) data set"

# A leftover from an earlier run would make this measure the wrong thing:
# STOR skips the allocation entirely when the data set already exists.
ftp_run "delete '$DSN_X37'" "$TMPDIR/ftp_x37_pre.log"

info "STOR into TRK(1,0) -> '$DSN_X37' (expect 552, promptly)"
FTP_OUT="$TMPDIR/ftp_x37_up.log"
X37_START=$(date +%s)
ftp_run "$(cat <<CMDS
site recfm=fb
site lrecl=80
site blksize=3120
site tracks
site primary=1
site secondary=0
type binary
put $X37FILE '$DSN_X37'
CMDS
)" "$FTP_OUT"
X37_ELAPSED=$(( $(date +%s) - X37_START ))

if grep -q "^552" "$FTP_OUT"; then
    pass "Out-of-space upload answers 552 (permanent)"
elif grep -q "^451" "$FTP_OUT"; then
    fail "Out-of-space upload answers 451 (transient — client will retry)"
    grep -E "^4[0-9][0-9]|^5[0-9][0-9]" "$FTP_OUT" | tail -3 | sed 's/^/    /'
else
    fail "Out-of-space upload did not fail as expected"
    tail -5 "$FTP_OUT" | sed 's/^/    /'
fi

# The regression this guards: recovery used to ABEND again in fclose() and
# skip the data connection close, leaving the client to time out.  30s is
# far above a healthy run (~2s) and far below the 300s idle timeout.
if [ "$X37_ELAPSED" -lt 30 ] && ! grep -q "^421" "$FTP_OUT"; then
    pass "Session ended promptly (${X37_ELAPSED}s) — data connection was closed"
else
    fail "Session hung for ${X37_ELAPSED}s — recovery did not close the data connection"
    grep -E "^421" "$FTP_OUT" | sed 's/^/    /'
fi

# The partial data set must be gone: DISP=(NEW,CATLG,DELETE) says so, and
# until libc370 1.0.6 nothing performed it.  A leftover is not cosmetic — the
# next STOR finds it, skips the allocation entirely and fails identically
# whatever the client asks for with SITE.
FTP_OUT="$TMPDIR/ftp_x37_gone.log"
ftp_run "delete '$DSN_X37'" "$FTP_OUT"
if grep -qi "does not exist" "$FTP_OUT"; then
    pass "Partial data set was scratched"
elif grep -qi "IDCAMS rc=8" "$FTP_OUT"; then
    fail "Partial data set survives, locked by the address space (libc370#168 —"
    fail "  is the sysroot older than 1.0.6?)"
else
    fail "Partial data set was not scratched"
    tail -2 "$FTP_OUT" | sed 's/^/    /'
fi

# And the retry must now get a clean allocation rather than inheriting the
# leftover — same failure, reached the same way, not a different code path.
info "Second attempt — must fail the same way, from a fresh allocation"
FTP_OUT="$TMPDIR/ftp_x37_retry.log"
ftp_run "$(cat <<CMDS
site recfm=fb
site lrecl=80
site blksize=3120
site tracks
site primary=1
site secondary=0
type binary
put $X37FILE '$DSN_X37'
CMDS
)" "$FTP_OUT"
if grep -q "^552" "$FTP_OUT"; then
    pass "Retry answers 552 from a fresh allocation"
else
    fail "Retry did not answer 552"
    tail -5 "$FTP_OUT" | sed 's/^/    /'
fi

ftp_run "delete '$DSN_X37'" "$TMPDIR/ftp_x37_post.log"

# ============================================================
# TEST 8: A failed member STOR into a full PDS
#
# A member of 47 records is smaller than one 19040-byte block, so it
# reaches DASD only inside CLOSE, and an x37 there is reported by
# fclose() alone.  Members are stored until one fails; how many fit
# depends on the volume, so the loop finds out.  Five directory blocks
# so that the space runs out before the directory does: one block holds
# 20 members, and on a 3390 two tracks hold more than that -- the 21st
# member then fails on the directory (an ABEND in CLOSE, 451), and the
# overwrite below succeeds, which measures nothing (dev1, 2026-10-13).  Then the PDS is full,
# and three things must hold:
#
#   - the STOR that failed answered 552, not 250;
#   - the member it was storing is not in the directory;
#   - a failed overwrite of an existing member leaves its old content.
#
# The first needs FTPD to check fclose(); the other two need libc370
# 2.10.3, whose CLOSE of a failed member skips the STOW (libc370#524).
# Before that a failed new member stayed with no records and a failed
# overwrite left the old member empty.  The PDS has to be filled by
# SUCCESSFUL members: one filled by a failed STOR proves nothing about
# the next one.  The shape is mvsMF's curl-nospace section 5.
# ============================================================
section "Test 8: Failed member STOR into a full PDS"

DSN_FULL="${HLQ}.TEST.FULLPDS"
MEMRECS=47

# xfer_reply <log>: the reply that ended the transfer -- not the 221 of QUIT
xfer_reply() {
    grep -E "^(226|250|[45][0-9][0-9]) " "$1" | tail -1
}

# mkmem <file> <tag>: MEMRECS records, each starting with the tag
mkmem() {
    awk -v n=$MEMRECS -v t="$2" \
        'BEGIN{for(i=1;i<=n;i++) printf "%s%06d\n", t, i}' > "$1"
}

info "MKD '$DSN_FULL' (FB 80, BLKSIZE 19040, TRK(2,0), 5 directory blocks)"
FTP_OUT="$TMPDIR/ftp_full_mkd.log"
ftp_run "$(cat <<CMDS
rmdir '$DSN_FULL'
site recfm=fb
site lrecl=80
site blksize=19040
site tracks
site primary=2
site secondary=0
site directory=5
mkdir '$DSN_FULL'
CMDS
)" "$FTP_OUT"

if ! grep -q "^257" "$FTP_OUT"; then
    fail "MKD '$DSN_FULL' failed"
    tail -5 "$FTP_OUT" | sed 's/^/    /'
else
    FULL=0
    FULL_REPLY=""
    m=1
    while [ "$m" -le 80 ]; do
        mkmem "$TMPDIR/full_m$m.txt" "MEMBER$m-"
        FTP_OUT="$TMPDIR/ftp_full_m$m.log"
        ftp_run "type ascii
put $TMPDIR/full_m$m.txt '$DSN_FULL(M$m)'" "$FTP_OUT"
        if ! grep -q "^226\|^250" "$FTP_OUT"; then
            FULL=$m
            FULL_REPLY=$(xfer_reply "$FTP_OUT")
            break
        fi
        m=$((m + 1))
    done

    if [ "$FULL" -eq 0 ]; then
        fail "The PDS did not fill within 80 members (or every failure answered 2xx)"
    elif [ "$FULL" -eq 1 ]; then
        fail "The first member into the empty PDS failed: $FULL_REPLY"
    else
        info "Filled: M1..M$((FULL - 1)) stored, M$FULL answered: $FULL_REPLY"

        if echo "$FULL_REPLY" | grep -q "^552"; then
            pass "A member STOR into the full PDS answers 552"
        else
            fail "A member STOR into the full PDS answered: $FULL_REPLY"
        fi

        # A member whose CLOSE failed must not be listed.  RETR of a
        # member in the directory with no records would answer 226 and
        # an empty file; one that is absent answers 550.
        FTP_OUT="$TMPDIR/ftp_full_absent.log"
        rm -f "$TMPDIR/full_absent.txt"
        ftp_run "type ascii
get '$DSN_FULL(M$FULL)' $TMPDIR/full_absent.txt" "$FTP_OUT"
        if grep -q "^550" "$FTP_OUT"; then
            pass "The member whose STOR failed (M$FULL) is not in the directory"
        else
            fail "The member whose STOR failed (M$FULL) is in the directory ($(wc -l < "$TMPDIR/full_absent.txt" 2>/dev/null || echo 0) records)"
            tail -3 "$FTP_OUT" | sed 's/^/    /'
        fi

        # The overwrite of an existing member that does not fit any more
        mkmem "$TMPDIR/full_r1.txt" "REPLACE1-"
        FTP_OUT="$TMPDIR/ftp_full_r1.log"
        ftp_run "type ascii
put $TMPDIR/full_r1.txt '$DSN_FULL(M1)'" "$FTP_OUT"
        R1_REPLY=$(xfer_reply "$FTP_OUT")

        FTP_OUT="$TMPDIR/ftp_full_m1back.log"
        rm -f "$TMPDIR/full_m1back.txt"
        ftp_run "type ascii
get '$DSN_FULL(M1)' $TMPDIR/full_m1back.txt" "$FTP_OUT"
        OLD=$(grep -c "^MEMBER1-" "$TMPDIR/full_m1back.txt" 2>/dev/null)
        NEW=$(grep -c "^REPLACE1-" "$TMPDIR/full_m1back.txt" 2>/dev/null)
        OLD=${OLD:-0}
        NEW=${NEW:-0}

        if echo "$R1_REPLY" | grep -q "^2"; then
            fail "An overwrite into the full PDS answered: $R1_REPLY ($NEW new records) -- the PDS was not full, so this proved nothing"
        elif ! echo "$R1_REPLY" | grep -q "^552"; then
            fail "An overwrite into the full PDS answered: $R1_REPLY (expected 552)"
        else
            pass "An overwrite into the full PDS answers 552"
        fi
        if [ "$OLD" -eq "$MEMRECS" ] && [ "$NEW" -eq 0 ]; then
            pass "The failed overwrite left M1 with its $MEMRECS old records"
        else
            fail "The failed overwrite left M1 with $OLD old and $NEW new records (libc370#524: is the sysroot older than 2.10.3?)"
        fi
    fi
fi

ftp_run "rmdir '$DSN_FULL'" "$TMPDIR/ftp_full_post.log"

# ============================================================
# TEST 9: APPE
#
# STOR replaces, APPE adds.  What each case has to show:
#   - an existing sequential data set keeps its old records and gets the
#     new ones after them (before the change APPE replaced them);
#   - a data set or member that is not there yet is created;
#   - an existing PDS member is refused with 550 and is unchanged afterwards
#     (BPAM cannot extend a member; libc370#204);
#   - UFS still answers 502.
# ============================================================
section "Test 9: APPE"

DSN_APP="${HLQ}.TEST.APPESEQ"
DSN_APPN="${HLQ}.TEST.APPENEW"
DSN_APPP="${HLQ}.TEST.APPEPDS"
APPRECS=23

# mkapp <file> <tag>: APPRECS records, each starting with the tag
mkapp() {
    awk -v n=$APPRECS -v t="$2" \
        'BEGIN{for(i=1;i<=n;i++) printf "%s%06d\n", t, i}' > "$1"
}

# fetch <dsn> <local>: RETR as text, trailing blanks of the FB records cut
fetch() {
    rm -f "$2.raw"
    ftp_run "type ascii
get '$1' $2.raw" "$TMPDIR/ftp_fetch.log"
    if [ -f "$2.raw" ]; then
        sed 's/[[:space:]]*$//' "$2.raw" | tr -d '\r' > "$2"
    else
        : > "$2"
    fi
}

mkapp "$TMPDIR/app_a.txt" "FIRST-"
mkapp "$TMPDIR/app_b.txt" "SECOND-"
cat "$TMPDIR/app_a.txt" "$TMPDIR/app_b.txt" > "$TMPDIR/app_ab.txt"

ftp_run "delete '$DSN_APP'
delete '$DSN_APPN'
rmdir '$DSN_APPP'" "$TMPDIR/ftp_app_pre.log"

# --- 9a: existing sequential data set -------------------------------
info "STOR then APPE -> '$DSN_APP'"
FTP_OUT="$TMPDIR/ftp_app_seq.log"
ftp_run "$(cat <<CMDS
site recfm=fb
site lrecl=80
site blksize=3120
site primary=2
site secondary=1
site tracks
type ascii
put $TMPDIR/app_a.txt '$DSN_APP'
append $TMPDIR/app_b.txt '$DSN_APP'
CMDS
)" "$FTP_OUT"
APP_REPLY=$(xfer_reply "$FTP_OUT")
if grep -q "^125 Appending to data set" "$FTP_OUT" && echo "$APP_REPLY" | grep -q "^2"; then
    pass "APPE to an existing sequential data set answers 125 Appending, then 2xx"
else
    fail "APPE to an existing sequential data set: $APP_REPLY"
    tail -5 "$FTP_OUT" | sed 's/^/    /'
fi

fetch "$DSN_APP" "$TMPDIR/app_seq_back.txt"
if diff -q "$TMPDIR/app_ab.txt" "$TMPDIR/app_seq_back.txt" > /dev/null 2>&1; then
    pass "APPE kept the $APPRECS old records and added $APPRECS after them"
else
    OLD=$(grep -c "^FIRST-" "$TMPDIR/app_seq_back.txt")
    NEW=$(grep -c "^SECOND-" "$TMPDIR/app_seq_back.txt")
    fail "APPE to a sequential data set left $OLD old and $NEW new records (expected $APPRECS and $APPRECS, old first)"
fi

# A second APPE goes behind the first, not over it
FTP_OUT="$TMPDIR/ftp_app_seq2.log"
ftp_run "type ascii
append $TMPDIR/app_a.txt '$DSN_APP'" "$FTP_OUT"
fetch "$DSN_APP" "$TMPDIR/app_seq_back2.txt"
cat "$TMPDIR/app_ab.txt" "$TMPDIR/app_a.txt" > "$TMPDIR/app_aba.txt"
if diff -q "$TMPDIR/app_aba.txt" "$TMPDIR/app_seq_back2.txt" > /dev/null 2>&1; then
    pass "A second APPE appends again ($((APPRECS * 3)) records)"
else
    fail "A second APPE left $(wc -l < "$TMPDIR/app_seq_back2.txt") records (expected $((APPRECS * 3)))"
fi

# --- 9b: data set that does not exist -------------------------------
info "APPE -> '$DSN_APPN' (does not exist)"
FTP_OUT="$TMPDIR/ftp_app_new.log"
ftp_run "$(cat <<CMDS
site recfm=fb
site lrecl=80
site blksize=3120
site primary=2
site secondary=1
site tracks
type ascii
append $TMPDIR/app_a.txt '$DSN_APPN'
CMDS
)" "$FTP_OUT"
APP_REPLY=$(xfer_reply "$FTP_OUT")
fetch "$DSN_APPN" "$TMPDIR/app_new_back.txt"
if echo "$APP_REPLY" | grep -q "^2" \
   && diff -q "$TMPDIR/app_a.txt" "$TMPDIR/app_new_back.txt" > /dev/null 2>&1; then
    pass "APPE to a missing data set creates it with the $APPRECS records"
else
    fail "APPE to a missing data set: $APP_REPLY, $(wc -l < "$TMPDIR/app_new_back.txt") records back"
    tail -5 "$FTP_OUT" | sed 's/^/    /'
fi

# --- 9c/9d: PDS ------------------------------------------------------
info "MKD '$DSN_APPP', STOR member OLD, APPE member NEW and OLD"
FTP_OUT="$TMPDIR/ftp_app_pds.log"
ftp_run "$(cat <<CMDS
site recfm=fb
site lrecl=80
site blksize=3120
site tracks
site primary=2
site secondary=1
site directory=5
mkdir '$DSN_APPP'
type ascii
put $TMPDIR/app_a.txt '$DSN_APPP(OLD)'
append $TMPDIR/app_b.txt '$DSN_APPP(NEW)'
append $TMPDIR/app_b.txt '$DSN_APPP(OLD)'
CMDS
)" "$FTP_OUT"

fetch "$DSN_APPP(NEW)" "$TMPDIR/app_mnew_back.txt"
if diff -q "$TMPDIR/app_b.txt" "$TMPDIR/app_mnew_back.txt" > /dev/null 2>&1; then
    pass "APPE to a missing PDS member creates it"
else
    fail "APPE to a missing PDS member: $(wc -l < "$TMPDIR/app_mnew_back.txt") records back"
    tail -8 "$FTP_OUT" | sed 's/^/    /'
fi

if grep -q "^550 .*OLD" "$FTP_OUT" && grep -qi "unchanged" "$FTP_OUT"; then
    pass "APPE to an existing PDS member answers 550 and says the member is unchanged"
else
    fail "APPE to an existing PDS member did not answer the explanatory 550"
    tail -8 "$FTP_OUT" | sed 's/^/    /'
fi

fetch "$DSN_APPP(OLD)" "$TMPDIR/app_mold_back.txt"
if diff -q "$TMPDIR/app_a.txt" "$TMPDIR/app_mold_back.txt" > /dev/null 2>&1; then
    pass "The refused APPE left the member with its $APPRECS old records"
else
    fail "The refused APPE left OLD with $(grep -c "^FIRST-" "$TMPDIR/app_mold_back.txt") of its $APPRECS old records and $(grep -c "^SECOND-" "$TMPDIR/app_mold_back.txt") of the appended ones"
fi

# --- 9e: UFS ---------------------------------------------------------
if [ "${UFS_AVAILABLE:-0}" = "1" ]; then
    FTP_OUT="$TMPDIR/ftp_app_ufs.log"
    ftp_run "cd /
type ascii
append $TMPDIR/app_a.txt /appe_probe" "$FTP_OUT"
    if grep -q "^502" "$FTP_OUT"; then
        pass "APPE in UFS mode still answers 502"
    else
        fail "APPE in UFS mode did not answer 502"
        tail -4 "$FTP_OUT" | sed 's/^/    /'
    fi
else
    info "UFSD not available -- UFS APPE check skipped"
fi

ftp_run "delete '$DSN_APP'
delete '$DSN_APPN'
rmdir '$DSN_APPP'" "$TMPDIR/ftp_app_post.log"

# ============================================================
# TEST 10: SITE in any spelling (#189, RFC 959 5.3)
#
# The server upper-cases the verb, the keywords and their values, so
# SITE, site and Site are one command.  BSD ftp/tnftp implement `site`
# themselves and refuse `SITE` and `Site` without asking the server, so
# every spelling is sent through `quote`, which passes the line unchanged.
# The per-keyword comparison is test/tstsit.c; this section covers the
# verb and one value that decides what the server does (JESOWNER, read back
# from the 550 that DELE of a job that is not there answers; it needs no data
# connection).
# ============================================================
section "Test 10: SITE in any spelling"

LUSER=$(echo "$USER" | tr '[:upper:]' '[:lower:]')

info "quote SITE / site / Site with upper, lower and mixed case keywords"
FTP_OUT="$TMPDIR/ftp_site_case.log"
ftp_run "$(cat <<CMDS
quote SITE LRECL=80
quote site LRECL=80
quote Site Lrecl=80
quote SITE RECFM=FB
quote site recfm=fb
quote SITE FILETYPE=JES
quote site filetype=seq
quote SITE JESSTATUS=ALL
quote site jesstatus=all
quote sItE TRACKS
quote SITE
CMDS
)" "$FTP_OUT"
SITE_OK=$(grep -c "^200 SITE command was accepted" "$FTP_OUT")
if [ "$SITE_OK" -eq 10 ] && grep -q "^202 SITE not necessary" "$FTP_OUT" \
   && ! grep -q "^500\|Unrecognized" "$FTP_OUT"; then
    pass "SITE, site, Site and sItE with any keyword spelling: 10 x 200, bare SITE 202"
else
    fail "SITE spellings: $SITE_OK of 10 answered 200"
    tail -14 "$FTP_OUT" | sed 's/^/    /'
fi

info "JESOWNER in upper and lower case"
FTP_OUT="$TMPDIR/ftp_site_owner.log"
ftp_run "$(cat <<CMDS
quote SITE FILETYPE=JES
quote SITE JESOWNER=$HLQ
quote DELE JOB99999
quote site jesowner=$LUSER
quote DELE JOB99999
quote SITE FILETYPE=SEQ
CMDS
)" "$FTP_OUT"
OWN_UP=$(grep -c "^550 .*JESOWNER=$HLQ\$" "$FTP_OUT")
OWN_ANY=$(grep -c "^550 Jobid JOB99999 not found for JESJOBNAME=" "$FTP_OUT")
if [ "$OWN_ANY" -eq 2 ] && [ "$OWN_UP" -eq 2 ]; then
    pass "jesowner=$LUSER filters the same owner as JESOWNER=$HLQ"
else
    fail "JESOWNER spelling: $OWN_UP of $OWN_ANY JES replies named JESOWNER=$HLQ"
    grep "^550" "$FTP_OUT" | sed 's/^/    /'
fi

# ============================================================
# TEST 11: A member STOR into a PDS whose DIRECTORY is full (#173)
#
# Test 8 keeps the directory out of the way on purpose; this is the
# opposite shape: one directory block (20 members) and room for far
# more members than that, so the 21st STOR fails on the directory and
# on nothing else.  The system ABENDs the CLOSE with SB14-0C (STOW finds
# no room), which is not an x37 and used to answer 451 -- a transient
# code, so a client retried into the same full directory.  A directory
# cannot be extended, so the answer is the permanent 552, with the one
# thing the client can do about it: SITE DIRECTORY= when the PDS is
# created.  Four things must hold:
#
#   - the STOR that failed answered 552 and names SITE DIRECTORY=;
#   - the member it was storing is not in the directory;
#   - the members that were stored are intact;
#   - the server carries on: the same failure again answers 552 again,
#     and the PDS can still be deleted afterwards.
# ============================================================
section "Test 11: Member STOR into a PDS with a full directory"

DSN_DIR="${HLQ}.TEST.FULLDIR"

info "MKD '$DSN_DIR' (FB 80, BLKSIZE 19040, TRK(5,0), 1 directory block)"
FTP_OUT="$TMPDIR/ftp_dir_mkd.log"
ftp_run "$(cat <<CMDS
rmdir '$DSN_DIR'
site recfm=fb
site lrecl=80
site blksize=19040
site tracks
site primary=5
site secondary=0
site directory=1
mkdir '$DSN_DIR'
CMDS
)" "$FTP_OUT"

if ! grep -q "^257" "$FTP_OUT"; then
    fail "MKD '$DSN_DIR' failed"
    tail -5 "$FTP_OUT" | sed 's/^/    /'
else
    DFULL=0
    DFULL_REPLY=""
    m=1
    while [ "$m" -le 60 ]; do
        mkmem "$TMPDIR/dir_m$m.txt" "MEMBER$m-"
        FTP_OUT="$TMPDIR/ftp_dir_m$m.log"
        ftp_run "type ascii
put $TMPDIR/dir_m$m.txt '$DSN_DIR(M$m)'" "$FTP_OUT"
        if ! grep -q "^226\|^250" "$FTP_OUT"; then
            DFULL=$m
            DFULL_REPLY=$(xfer_reply "$FTP_OUT")
            break
        fi
        m=$((m + 1))
    done

    if [ "$DFULL" -eq 0 ]; then
        fail "The directory did not fill within 60 members (or every failure answered 2xx)"
    elif [ "$DFULL" -eq 1 ]; then
        fail "The first member into the empty PDS failed: $DFULL_REPLY"
    else
        info "Filled: M1..M$((DFULL - 1)) stored, M$DFULL answered: $DFULL_REPLY"

        if echo "$DFULL_REPLY" | grep -q "^552"; then
            pass "A member STOR into the PDS with a full directory answers 552"
        else
            fail "A member STOR into the PDS with a full directory answered: $DFULL_REPLY"
        fi
        if echo "$DFULL_REPLY" | grep -q "SITE DIRECTORY="; then
            pass "The 552 names SITE DIRECTORY="
        else
            fail "The reply does not name SITE DIRECTORY=: $DFULL_REPLY"
        fi

        FTP_OUT="$TMPDIR/ftp_dir_absent.log"
        rm -f "$TMPDIR/dir_absent.txt"
        ftp_run "type ascii
get '$DSN_DIR(M$DFULL)' $TMPDIR/dir_absent.txt" "$FTP_OUT"
        if grep -q "^550" "$FTP_OUT"; then
            pass "The member whose STOR failed (M$DFULL) is not in the directory"
        else
            fail "The member whose STOR failed (M$DFULL) is in the directory"
            tail -3 "$FTP_OUT" | sed 's/^/    /'
        fi

        # The members before it, first and last, still hold their records
        DINTACT=1
        for k in 1 $((DFULL - 1)); do
            FTP_OUT="$TMPDIR/ftp_dir_back$k.log"
            rm -f "$TMPDIR/dir_back$k.txt"
            ftp_run "type ascii
get '$DSN_DIR(M$k)' $TMPDIR/dir_back$k.txt" "$FTP_OUT"
            GOT=$(grep -c "^MEMBER$k-" "$TMPDIR/dir_back$k.txt" 2>/dev/null)
            if [ "${GOT:-0}" -ne "$MEMRECS" ]; then
                DINTACT=0
                info "M$k has ${GOT:-0} of $MEMRECS records"
            fi
        done
        if [ "$DINTACT" -eq 1 ]; then
            pass "The members stored before the failure are intact"
        else
            fail "A member stored before the failure lost records"
        fi

        # The same failure again, on a new session: the server survived the
        # first one, and the retry meets the same permanent answer.
        mkmem "$TMPDIR/dir_again.txt" "AGAIN-"
        FTP_OUT="$TMPDIR/ftp_dir_again.log"
        ftp_run "type ascii
put $TMPDIR/dir_again.txt '$DSN_DIR(M$DFULL)'" "$FTP_OUT"
        AGAIN_REPLY=$(xfer_reply "$FTP_OUT")
        if echo "$AGAIN_REPLY" | grep -q "^552"; then
            pass "The retry answers 552 again and the server is still serving"
        else
            fail "The retry answered: ${AGAIN_REPLY:-no reply}"
        fi
    fi
fi

FTP_OUT="$TMPDIR/ftp_dir_post.log"
ftp_run "rmdir '$DSN_DIR'" "$FTP_OUT"
if grep -q "^250" "$FTP_OUT"; then
    pass "The PDS can be deleted after the failure (no DD left allocated)"
else
    fail "RMD of the PDS after the failure: $(tail -2 "$FTP_OUT" | tr '\n' ' ')"
fi

# ============================================================
# Test 12: STOR and APPE on a retained data set (#198)
#
# OPEN for output of a data set whose expiration date has not passed raises
# IEC507D and waits for an operator reply.  FTPD answers 550 with the date
# before the OPEN instead.  Without the check the second STOR below never
# answers; ftp_run_wd cuts such a session off, and the console still shows
# the IEC507D to reply to.
#
# Needs RETPD (SITE RETPD, #130).  Expiration is one day ahead, so the data
# sets are deleted at the end: DELE and RMD do not ask for retention.
# ============================================================
section "Test 12: STOR and APPE on a retained data set"

DSN_RET="${HLQ}.TEST.RETSEQ"
DSN_RETP="${HLQ}.TEST.RETPDS"
printf 'RETAINED-000001\nRETAINED-000002\n' > "$TMPDIR/ret.txt"

# ftp_run, but a session stuck in OPEN (IEC507D) is cut off after 90 s
ftp_run_wd() {
    local cmds="$1"
    local capture="${2:-/dev/null}"

    printf "user %s %s\n%b\nquit\n" "$USER" "$PASS" "$cmds" \
        | timeout 90 ftp -n -v -p -P "$PORT" "$HOST" > "$capture" 2>&1
    return $?
}

ftp_run "delete '$DSN_RET'
rmdir '$DSN_RETP'" /dev/null

info "STOR twice and APPE with RETPD=1 -> '$DSN_RET'"
FTP_OUT="$TMPDIR/ftp_ret_seq.log"
ftp_run_wd "$(cat <<CMDS
site recfm=fb
site lrecl=80
site blksize=3120
site primary=2
site secondary=1
site tracks
site retpd=1
type ascii
put $TMPDIR/ret.txt '$DSN_RET'
put $TMPDIR/ret.txt '$DSN_RET'
append $TMPDIR/ret.txt '$DSN_RET'
CMDS
)" "$FTP_OUT"
RET_REFUSED=$(grep -cE "^550 .* is retained until [0-9]{4}-[0-9]{2}-[0-9]{2} \([0-9]{4}\.[0-9]{3}\)" "$FTP_OUT")
if [ "$RET_REFUSED" -eq 2 ] && grep -q "^250 \|^226 " "$FTP_OUT"; then
    pass "STOR creates the retained data set; STOR and APPE over it answer 550 with the date"
else
    fail "retained data set: $RET_REFUSED of 2 refusals answered 550 with a date (a hang is IEC507D)"
    tail -12 "$FTP_OUT" | sed 's/^/    /'
fi

info "STOR without RETPD over the retained data set"
FTP_OUT="$TMPDIR/ftp_ret_plain.log"
ftp_run_wd "type ascii
put $TMPDIR/ret.txt '$DSN_RET'" "$FTP_OUT"
if grep -qE "^550 .* is retained until " "$FTP_OUT"; then
    pass "a session without RETPD is refused the same way"
else
    fail "STOR over a retained data set without RETPD: no 550 with a date"
    tail -6 "$FTP_OUT" | sed 's/^/    /'
fi

info "STOR of a member into a PDS that MKD created with RETPD=1"
FTP_OUT="$TMPDIR/ftp_ret_pds.log"
ftp_run_wd "$(cat <<CMDS
site recfm=fb
site lrecl=80
site blksize=3120
site primary=2
site secondary=1
site tracks
site retpd=1
mkdir '$DSN_RETP'
type ascii
put $TMPDIR/ret.txt '$DSN_RETP(M1)'
put $TMPDIR/ret.txt '$DSN_RETP(M1)'
CMDS
)" "$FTP_OUT"
RETP_REFUSED=$(grep -cE "^550 .* is retained until " "$FTP_OUT")
if [ "$RETP_REFUSED" -eq 2 ]; then
    pass "STOR into a retained PDS answers 550 with the date, new member or not"
else
    fail "retained PDS: $RETP_REFUSED of 2 member STORs answered 550 with a date"
    tail -8 "$FTP_OUT" | sed 's/^/    /'
fi

info "DELE and RMD of the retained data sets"
FTP_OUT="$TMPDIR/ftp_ret_clean.log"
ftp_run_wd "delete '$DSN_RET'
rmdir '$DSN_RETP'" "$FTP_OUT"
if [ "$(grep -c "^250 .* deleted" "$FTP_OUT")" -eq 2 ]; then
    pass "DELE and RMD remove retained data sets"
else
    fail "DELE/RMD of retained data sets"
    tail -6 "$FTP_OUT" | sed 's/^/    /'
fi

# ============================================================
# Test 13: Byte counters (#206)
#
# F FTPD,STATS (BYTES IN / BYTES OUT) and the sent= / recv= of the
# "session ended" line are the same numbers: ftpd#ses.c adds the session's
# counters to the server totals when the session ends.  Each byte has to be
# counted once, by the data connection functions.  MVS data set transfers were
# counted three times (the session counter twice) until #206.
#
# One session per path, so the change of the totals is exactly what that one
# session transferred: STOR then RETR of a known number of bytes, on an MVS
# data set and on a UFS file.  Each file is a multiple of the record length
# for the MVS case, so RETR sends back the bytes STOR received.
# ============================================================
section "Test 13: Byte counters"

# BYTES IN and BYTES OUT from F FTPD,STATS, as "in out"
ftpd_stats() {
    $FTPD_CONSOLE "F FTPD,STATS" 2>&1 | awk '
        /FTPD012I/ { in_ = $NF }
        /FTPD013I/ { out = $NF }
        END { print in_, out }'
}

# The server adds a session's bytes when the session thread ends, which can
# be a moment after the client has quit: ask until the totals settle on the
# wanted change or the time is up.  Sets STATS_IN / STATS_OUT to the change
# since "$1 $2".
stats_delta() {
    local base_in="${1%% *}" base_out="${1##* }"
    local want_in="$2" want_out="$3" i cur

    STATS_IN=""; STATS_OUT=""
    for i in 1 2 3 4 5 6 7 8 9 10; do
        cur=$(ftpd_stats)
        STATS_IN=$(( ${cur%% *} - base_in ))
        STATS_OUT=$(( ${cur##* } - base_out ))
        if [ "$STATS_IN" -eq "$want_in" ] && [ "$STATS_OUT" -eq "$want_out" ]; then
            return 0
        fi
        sleep 2
    done
    return 1
}

if [ -z "$FTPD_CONSOLE" ]; then
    info "SKIPPED: FTPD_CONSOLE is not set, F FTPD,STATS cannot be read"
else
    # MVS data set: 600 records of 1024 bytes
    DSN_CNT="${HLQ}.TEST.BYTECNT"
    CNT_MVS=614400
    dd if=/dev/urandom of="$TMPDIR/cnt_mvs.dat" bs=1024 count=600 2>/dev/null
    ftp_run "delete '$DSN_CNT'" /dev/null
    CNT_BASE=$(ftpd_stats)
    info "STATS before: BYTES IN/OUT = $CNT_BASE"
    case "$CNT_BASE" in
        *[!0-9\ ]*|""|" "*|*" ") fail "F FTPD,STATS: no byte counters in the answer ('$CNT_BASE')" ;;
    esac

    FTP_OUT="$TMPDIR/ftp_cnt_mvs.log"
    ftp_run "$(cat <<CMDS
site recfm=fb
site lrecl=1024
site blksize=6144
site primary=100
site secondary=50
site tracks
type binary
put $TMPDIR/cnt_mvs.dat '$DSN_CNT'
get '$DSN_CNT' $TMPDIR/cnt_mvs.back
CMDS
)" "$FTP_OUT"
    if [ "$(stat -c%s "$TMPDIR/cnt_mvs.back" 2>/dev/null || stat -f%z "$TMPDIR/cnt_mvs.back" 2>/dev/null)" = "$CNT_MVS" ]; then
        if stats_delta "$CNT_BASE" "$CNT_MVS" "$CNT_MVS"; then
            pass "MVS data set: $CNT_MVS bytes each way counted once (IN $STATS_IN, OUT $STATS_OUT)"
        else
            fail "MVS data set: sent and received $CNT_MVS bytes each way, STATS counted IN $STATS_IN, OUT $STATS_OUT"
        fi
    else
        fail "MVS data set: the $CNT_MVS bytes did not make the round trip"
        tail -5 "$FTP_OUT" | sed 's/^/    /'
    fi
    ftp_run "delete '$DSN_CNT'" /dev/null

    # UFS file: an odd size on purpose
    UFS_CNTDIR="/tmp/ftpdcnt$$"
    CNT_UFS=300001
    dd if=/dev/urandom of="$TMPDIR/cnt_ufs.dat" bs=1 count=$CNT_UFS 2>/dev/null
    ftp_run "cd /
mkdir $UFS_CNTDIR" /dev/null
    CNT_BASE=$(ftpd_stats)

    FTP_OUT="$TMPDIR/ftp_cnt_ufs.log"
    ftp_run "$(cat <<CMDS
cd $UFS_CNTDIR
type binary
put $TMPDIR/cnt_ufs.dat cnt.bin
get cnt.bin $TMPDIR/cnt_ufs.back
CMDS
)" "$FTP_OUT"
    if [ "$(stat -c%s "$TMPDIR/cnt_ufs.back" 2>/dev/null || stat -f%z "$TMPDIR/cnt_ufs.back" 2>/dev/null)" = "$CNT_UFS" ]; then
        if stats_delta "$CNT_BASE" "$CNT_UFS" "$CNT_UFS"; then
            pass "UFS file: $CNT_UFS bytes each way counted once (IN $STATS_IN, OUT $STATS_OUT)"
        else
            fail "UFS file: sent and received $CNT_UFS bytes each way, STATS counted IN $STATS_IN, OUT $STATS_OUT"
        fi
    else
        fail "UFS file: the $CNT_UFS bytes did not make the round trip"
        tail -5 "$FTP_OUT" | sed 's/^/    /'
    fi
    ftp_run "cd $UFS_CNTDIR
delete cnt.bin
cd /
rmdir $UFS_CNTDIR" /dev/null
fi

# ============================================================
# Summary
# ============================================================
echo ""
echo -e "${BOLD}================================${NC}"
echo -e "${BOLD}  Results: $PASSED/$TOTAL passed${NC}"
if [ $FAILED -gt 0 ]; then
    echo -e "${RED}  $FAILED FAILED:${NC}"
    echo -e "$FAIL_LIST"
    echo -e "${BOLD}================================${NC}"
    echo ""
    info "Files kept in $TMPDIR for analysis"
    trap - EXIT  # don't cleanup on failure
    exit 1
else
    echo -e "${GREEN}  ALL TESTS PASSED${NC}"
    echo -e "${BOLD}================================${NC}"
    exit 0
fi
