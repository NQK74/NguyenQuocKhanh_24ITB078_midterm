#!/usr/bin/env bash
# run_tests.sh — compare myls output against system ls.
#
# Creates a temporary test directory with various file types,
# runs both ./myls and /bin/ls with identical flags, and diffs
# the output.  Reports PASS/FAIL per test case.
#
# Usage: ./tests/run_tests.sh [path-to-myls]
#
# GNU ls is only a reference: it is not identical to the NetBSD man page.
# The one intentional difference (-F on symlinks in long format) is
# filtered out explicitly; everything else must match exactly.

set -e

# GNU ls shows blocks in 1024-byte units by default; the man page we
# follow uses 512.  POSIXLY_CORRECT makes GNU ls use 512 as well.
export POSIXLY_CORRECT=1

MYLS="${1:-./myls}"
SYSLS="/bin/ls"

# The man page says -A is always on for the super-user (myls does this);
# GNU ls does not, so add -A to the reference when running as root.
# A later -a on the command line still overrides it in both programs.
if [ "$(id -u)" -eq 0 ]; then
    SYSLS="/bin/ls -A"
fi
PASS=0
FAIL=0
TESTDIR=""

# ---- helpers ----------------------------------------------------

# Intentional difference: with -lF the man page puts '@' right after the
# symlink name ("la@ -> a"); GNU ls omits it and classifies the target
# instead ("la -> a/").  Normalise both before diffing.
filter_at() { sed -e 's/@ -> / -> /' -e 's| -> \(.*\)/$| -> \1|'; }


# Portable "touch with epoch seconds" (GNU touch -d @N is not available
# on BSD).  Usage: touch_epoch <-a|-m> <file> <epoch>
epoch_fmt() {
    if date -r 0 +%s >/dev/null 2>&1 && [ "$(date -r 0 +%s 2>/dev/null)" = "0" ]; then
        date -r "$1" +%Y%m%d%H%M.%S          # BSD date
    else
        date -d "@$1" +%Y%m%d%H%M.%S         # GNU date
    fi
}
touch_epoch() {
    touch -h "$1" -t "$(epoch_fmt "$3")" "$2" 2>/dev/null || touch "$1" -t "$(epoch_fmt "$3")" "$2"
}

die()  { echo "FATAL: $*" >&2; cleanup; exit 1; }

cleanup() {
    if [ -n "$TESTDIR" ] && [ -d "$TESTDIR" ]; then
        rm -rf "$TESTDIR"
    fi
}
trap cleanup EXIT

# Run a single test: compare myls vs system ls.
# Args: test_name flag [operand]
run_test() {
    local name="$1"
    shift
    local flags="$1"
    shift
    local operand="${1:-$TESTDIR}"

    local out_my="/tmp/myls_out_$$"
    local out_sys="/tmp/sysls_out_$$"

    # Run both, capturing stdout (stderr goes to /dev/null)
    $MYLS $flags "$operand" 2>/dev/null | ${MYLS_FILTER:-cat} > "$out_my" || true
    $SYSLS $flags "$operand" 2>/dev/null | ${MYLS_FILTER:-cat} > "$out_sys" || true

    if diff -u "$out_sys" "$out_my" > /dev/null 2>&1; then
        echo "  PASS: $name"
        PASS=$((PASS + 1))
    else
        echo "  FAIL: $name"
        diff -u "$out_sys" "$out_my" | head -20
        FAIL=$((FAIL + 1))
    fi

    rm -f "$out_my" "$out_sys"
}

# ---- setup test directory ----------------------------------------

echo "=== Setting up test directory ==="

TESTDIR=$(mktemp -d /tmp/myls_test.XXXXXX) || die "mktemp failed"

# Regular files with various sizes
echo "hello world" > "$TESTDIR/file_a.txt"
dd if=/dev/zero of="$TESTDIR/big_file" bs=1024 count=100 2>/dev/null
touch "$TESTDIR/empty_file"

# Hidden files
touch "$TESTDIR/.hidden"
echo "secret" > "$TESTDIR/.config"

# Subdirectories
mkdir -p "$TESTDIR/subdir1"
mkdir -p "$TESTDIR/subdir2"
echo "nested" > "$TESTDIR/subdir1/nested.txt"
touch "$TESTDIR/subdir2/another.txt"

# Empty subdirectory
mkdir -p "$TESTDIR/empty_dir"

# Symlink to a file
ln -s file_a.txt "$TESTDIR/link_to_file"

# Symlink to a directory
ln -s subdir1 "$TESTDIR/link_to_dir"

# Broken symlink
ln -s nonexistent "$TESTDIR/broken_link"

# File with spaces in name
touch "$TESTDIR/file with spaces.txt"

# Executable file
echo '#!/bin/sh' > "$TESTDIR/script.sh"
chmod +x "$TESTDIR/script.sh"

# FIFO (named pipe)
mkfifo "$TESTDIR/my_fifo" 2>/dev/null || true

# Files with different timestamps
touch -t 202001011200 "$TESTDIR/old_file" 2>/dev/null || true
touch -t 209901011200 "$TESTDIR/future_file" 2>/dev/null || true

# Give every entry distinct mtime/atime so -t and -u never depend on
# same-second ties (GNU ls breaks ties by nanoseconds, we use the name).
i=0
for f in "$TESTDIR"/* "$TESTDIR"/.[!.]*; do
    case "$f" in */old_file|*/future_file) continue ;; esac
    i=$((i + 1))
    touch_epoch -m "$f" $((1700000000 + i * 100000))
    touch_epoch -a "$f" $((1720000000 - i * 100000))
done

# -u needs its own directory of regular files: listing a symlink reads it,
# which updates the link's atime and makes the comparison unstable.
mkdir "$TESTDIR/atime_dir"
for n in 1 2 3; do
    echo x > "$TESTDIR/atime_dir/f$n"
    touch_epoch -a "$TESTDIR/atime_dir/f$n" $((1700000000 + (4 - n) * 100000))
done

echo "Test directory: $TESTDIR"
echo ""

# ---- run tests ---------------------------------------------------

echo "=== Running tests ==="
echo ""

echo "--- Basic listing ---"
run_test "no flags"           ""
run_test "-a (all)"           "-a"
run_test "-A (almost all)"    "-A"

echo ""
echo "--- Sorting ---"
run_test "-r (reverse)"       "-r"
run_test "-S (by size)"       "-S"
run_test "-t (by time)"       "-t"
run_test "-Sr (size reverse)" "-Sr"
run_test "-tr (time reverse)" "-tr"
run_test "-f (unsorted)"      "-f"

echo ""
echo "--- Display options ---"
run_test "-i (inode)"         "-i"
run_test "-s (blocks)"        "-s"
run_test "-F (type)"          "-F"
run_test "-iF (inode+type)"   "-iF"

echo ""
echo "--- Long format ---"
run_test "-l (long)"          "-l"
run_test "-la (long all)"     "-la"
run_test "-lA (long almost)"  "-lA"
run_test "-n (numeric)"       "-n"
run_test "-li (long+inode)"   "-li"
run_test "-ls (long+blocks)"  "-ls"
run_test "-lS (long+size sort)" "-lS"
run_test "-lt (long+time sort)" "-lt"
run_test "-lr (long+reverse)" "-lr"

echo ""
echo "--- Size formatting ---"
run_test "-lh (human)"        "-lh"
run_test "-lk (kilo)"         "-lk"
run_test "-sh (blocks human)" "-sh"

echo ""
echo "--- BLOCKSIZE ---"
export BLOCKSIZE=1024
run_test "BLOCKSIZE=1024 -s"  "-s"
run_test "BLOCKSIZE=1024 -ls" "-ls"
export BLOCKSIZE=4096
run_test "BLOCKSIZE=4096 -s"  "-s"
unset BLOCKSIZE

echo ""
echo "--- Directory flags ---"
run_test "-d (dir as file)"   "-d"
run_test "-ld (long dir)"     "-ld"

echo ""
echo "--- Recursion ---"
run_test "-R (recurse)"       "-R"
run_test "-lR (long recurse)" "-lR"
run_test "-laR (all recurse)" "-laR"

echo ""
echo "--- Combined flags ---"
run_test "-laSr"              "-laSr"
MYLS_FILTER=filter_at run_test "-laFi (@ after name)" "-laFi"
unset MYLS_FILTER
run_test "-lc (ctime)"        "-lc"
run_test "-ltu (atime sort)"  "-ltu"  "$TESTDIR/atime_dir"

echo ""
echo "--- Edge cases ---"
run_test "root /"             "-d"    "/"
run_test "dot ."              ""      "$TESTDIR/."
run_test "dotdot .."          ""      "$TESTDIR/subdir1/.."

# Mixed operands: file + directory
run_test "mixed operands"     "-l"    "$TESTDIR/file_a.txt"

echo ""
echo "=== Results ==="
echo "  Passed: $PASS"
echo "  Failed: $FAIL"
echo "  Total:  $((PASS + FAIL))"

if [ "$FAIL" -gt 0 ]; then
    echo ""
    echo "Review the FAIL cases above; GNU ls and NetBSD ls can differ."
    exit 1
fi

exit 0
