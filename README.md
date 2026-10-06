# myls — Simplified `ls(1)` Clone

A simplified clone of the UNIX `ls` command, written from scratch in C
(C99 + POSIX APIs) as a university midterm project. Behaviour follows the
provided NetBSD `ls` manual page (options `-AacdFfhiklnqRrSstuw`).

## Implemented Options

| Flag | Description |
|------|-------------|
| `-A` | Show all entries except `.` and `..` (always on for the super-user) |
| `-a` | Show all entries, including `.` and `..` |
| `-c` | Use last status-change time (ctime) for sorting/display |
| `-d` | List directories as plain files, do not descend |
| `-F` | Append type indicator (`/` dir, `*` exec, `@` link, `=` socket, `\|` FIFO), directly after the name (`link@ -> target` in long format) |
| `-f` | Do not sort output (implies `-a`) |
| `-h` | Human-readable sizes (`1.5K`, `23M`) — overrides `-k` |
| `-i` | Print inode number before each entry |
| `-k` | Show block counts in 1024-byte units — overrides `-h` |
| `-l` | Long listing format |
| `-n` | Like `-l` but show numeric UID/GID |
| `-q` | Replace non-printable characters with `?` (default on terminals); valid UTF-8 letters are kept |
| `-R` | Recurse into subdirectories |
| `-r` | Reverse sort order |
| `-S` | Sort by file size, largest first |
| `-s` | Print block count per entry (unit: 512, or `-k`, or `$BLOCKSIZE`) |
| `-t` | Sort by modification time, newest first |
| `-u` | Use last access time (atime) for sorting/display |
| `-w` | Print raw non-printable characters (default when piped) |

Combined flags (e.g. `-laR`) and repeated flags work correctly.
Mutually exclusive flags resolve by the "last one wins" rule.

### Environment variables

- `BLOCKSIZE` — unit for `-s` and the `total` line when neither `-h` nor
  `-k` is given. A number, optionally with suffix `k`/`m`/`g` (e.g. `1024`,
  `1K`, `4096`). Default is 512. Invalid values fall back to 512.
- `TZ` — time zone used for the time column in long format.
- `LANG` / `LC_ALL` — locale used to decide which characters are printable
  for `-q` (so UTF-8 file names such as `Tài liệu.txt` are shown correctly).

### Design choices where the man page is silent

- `-f` also implies `-a` (as in NetBSD `ls`).
- A symbolic link given as an operand is followed (and listed as a
  directory) unless `-d` is given.
- Entries are sorted by name with `strcmp` (byte order); with `-t`, ties
  are broken by name.

## File Structure

```
myls/
├── main.c         Entry point: parse options → list operands → exit
├── options.c/.h   Command-line flag parsing (getopt) and Options struct
├── entry.c/.h     Entry struct (one file/dir) — lstat, readlink, free
├── sort.c/.h      qsort comparators (name, size, time) with sort dispatch
├── print.c/.h     Output formatting: short, long, total, mode, sizes, time
├── list.c/.h      Directory listing, operand handling, -R recursion
├── Makefile        Build system (gcc, -Wall -Wextra -std=c99)
├── .gitignore      Ignore binary, *.o, editor junk
├── README.md       This file
└── tests/
    └── run_tests.sh   Automated diff-based tests against system ls
```

## Building

Requires GCC and GNU Make on a Linux/POSIX system.

```bash
make            # build the binary (./myls)
make clean      # remove binary and object files
```

Compile flags: `-Wall -Wextra -std=c99 -D_DEFAULT_SOURCE`

For debug/sanitizer builds:

```bash
make CFLAGS="-Wall -Wextra -std=c99 -D_DEFAULT_SOURCE -g -fsanitize=address,undefined"
```

## Running

```bash
./myls                      # list current directory
./myls -l /etc              # long format
./myls -la                  # all files, long format
./myls -lhS /usr/bin        # long, human-readable sizes, sorted by size
./myls -R /etc 2>/dev/null  # recursive listing
./myls -laR /tmp            # all files, long, recursive
./myls -id /                # inode number, treat / as plain file
./myls -F /dev              # type indicators
./myls file1 dir1 file2     # mixed file and directory operands
```

### Example Output

```
$ ./myls -la
total 336
drwxr-xr-x 3 user user  4096 Oct  2 01:16 .
drwxr-xr-x 8 user user  4096 Oct  1 12:34 ..
-rw-r--r-- 1 user user    28 Oct  1 12:06 .gitignore
-rw-r--r-- 1 user user   443 Oct  1 12:06 Makefile
-rw-r--r-- 1 user user  4606 Oct  1 12:12 README.md
-rw-r--r-- 1 user user  2705 Oct  1 12:08 entry.c
-rw-r--r-- 1 user user  1538 Oct  1 12:07 entry.h
...

$ ./myls -lhS *.c
-rw-r--r-- 1 user user  14K Oct  1 12:37 print.c
-rw-r--r-- 1 user user 8.9K Oct  1 12:13 list.c
-rw-r--r-- 1 user user 3.3K Oct  1 12:37 options.c
-rw-r--r-- 1 user user 2.6K Oct  1 12:08 entry.c
-rw-r--r-- 1 user user 2.4K Oct  1 12:08 sort.c
-rw-r--r-- 1 user user  809 Oct  1 12:37 main.c

$ BLOCKSIZE=1K ./myls -s main.c print.c
 4 main.c
16 print.c

$ ./myls -lF /tmp/lnk_demo
lrwxrwxrwx 1 user user 6 Oct  2 01:16 /tmp/lnk_demo@ -> main.c
```

*(Sizes and dates will differ on your machine.)*

## Testing

```bash
chmod +x tests/run_tests.sh
./tests/run_tests.sh
```

The script builds a temporary directory tree (regular files, directories,
symlinks, a FIFO, hidden files, a file with spaces, files dated in the past
and in the future) and compares `myls` against the system `ls` for 41 flag
combinations, including `BLOCKSIZE` values. Notes:

- GNU `ls` is only a reference. The script sets `POSIXLY_CORRECT=1` so GNU
  `ls` uses 512-byte blocks like the man page, and adds `-A` to the
  reference when run as root.
- Every test entry gets a distinct timestamp, because GNU `ls` breaks
  same-second ties by nanoseconds while `myls` breaks them by name.
- One intentional difference is normalised in the `-laFi` test: with `-lF`
  the man page puts `@` right after a symlink's name (`la@ -> a`), while
  GNU `ls` prints `la -> a/`.

All 41 tests pass both as root and as a normal user. The code was also run
under `-fsanitize=address,undefined` on edge cases (missing operands,
unreadable directories, symlink loops, very long names and paths, invalid
options, non-UTF-8 file names) without errors.

## Known Limitations

- **Linux and NetBSD**: uses POSIX APIs (`lstat`, `readlink`, `opendir`,
  etc.); `<sys/sysmacros.h>` is included only on Linux. The Makefile works
  with both BSD make and GNU make. On Windows use WSL or a VM. Not tested
  on macOS.
- **Whiteout files**: the `S_IFWHT` type exists only on BSDs. The code
  compiles the whiteout check conditionally (`#ifdef S_IFWHT`).
- **Unreadable subdirectories with `-R`**: the error message is printed,
  but the `dir:` header line is not shown before it (real `ls` prints it).
- **`total` line with `-s`**: printed whenever `-s` is used, not only when
  output is a terminal.
- **Column output**: only one-entry-per-line mode is implemented (no
  multi-column output when stdout is a terminal).
- **No color support**: output is plain text only.

## GitHub Repository

`<URL_PLACEHOLDER>` *(replace with your repo URL)*

## License

University coursework — not for redistribution.
