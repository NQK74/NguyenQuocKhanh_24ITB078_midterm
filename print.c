/*
 * print.c — all output formatting for ls.
 *
 * Two main modes:
 *   print_short()  — one entry per line (default)
 *   print_long()   — full "-l" listing with aligned columns
 *
 * Both respect -i (inode), -s (blocks), -F (type suffix), and
 * -q/-w (non-printable character handling).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <wchar.h>
#include <wctype.h>
#include <pwd.h>
#include <grp.h>
#include <sys/types.h>
#ifdef __linux__
#include <sys/sysmacros.h>    /* major(), minor() on Linux */
#endif

#include "print.h"

/* Threshold for "old" timestamps: ~6 months in seconds. */
#define SIX_MONTHS ((365 * 24 * 60 * 60) / 2)

/* ---- helpers ---------------------------------------------------- */

/*
 * Format a mode_t into a 10-character string like "drwxr-xr-x".
 * buf must be at least 11 bytes.
 *
 * Special permission bits:
 *   setuid: owner-x becomes 's' (if x set) or 'S' (if not)
 *   setgid: group-x becomes 's' (if x set) or 'S' (if not)
 *   sticky: other-x becomes 't' (if x set) or 'T' (if not)
 */
static void format_mode(mode_t mode, char *buf)
{
    /* File type character */
    switch (mode & S_IFMT) {
    case S_IFDIR:  buf[0] = 'd'; break;
    case S_IFLNK:  buf[0] = 'l'; break;
    case S_IFBLK:  buf[0] = 'b'; break;
    case S_IFCHR:  buf[0] = 'c'; break;
    case S_IFSOCK: buf[0] = 's'; break;
    case S_IFIFO:  buf[0] = 'p'; break;
#ifdef S_IFWHT
    case S_IFWHT:  buf[0] = 'w'; break;   /* whiteout (BSDs only) */
#endif
    default:       buf[0] = '-'; break;
    }

    /* Owner read/write/execute */
    buf[1] = (mode & S_IRUSR) ? 'r' : '-';
    buf[2] = (mode & S_IWUSR) ? 'w' : '-';
    if (mode & S_ISUID)
        buf[3] = (mode & S_IXUSR) ? 's' : 'S';
    else
        buf[3] = (mode & S_IXUSR) ? 'x' : '-';

    /* Group read/write/execute */
    buf[4] = (mode & S_IRGRP) ? 'r' : '-';
    buf[5] = (mode & S_IWGRP) ? 'w' : '-';
    if (mode & S_ISGID)
        buf[6] = (mode & S_IXGRP) ? 's' : 'S';
    else
        buf[6] = (mode & S_IXGRP) ? 'x' : '-';

    /* Other read/write/execute */
    buf[7] = (mode & S_IROTH) ? 'r' : '-';
    buf[8] = (mode & S_IWOTH) ? 'w' : '-';
    if (mode & S_ISVTX)
        buf[9] = (mode & S_IXOTH) ? 't' : 'T';
    else
        buf[9] = (mode & S_IXOTH) ? 'x' : '-';

    buf[10] = '\0';
}

/*
 * Format bytes into a human-readable string (e.g. "1.5K", "23M").
 * Uses binary units: K=1024, M=1024^2, G=1024^3, ...
 *
 * Convention:
 *   < 10 of a unit  → one decimal place  ("1.5K")
 *   >= 10           → no decimals         ("23M")
 *   raw bytes       → plain number        ("512")
 */
static void format_size_human(off_t bytes, char *buf, size_t buflen)
{
    static const char units[] = "BKMGTPE";
    double size = (double)bytes;
    int i = 0;

    while (size >= 1024.0 && i < 6) {
        size /= 1024.0;
        i++;
    }

    if (i == 0)
        snprintf(buf, buflen, "%lld", (long long)bytes);
    else if (size < 10.0)
        snprintf(buf, buflen, "%.1f%c", size, units[i]);
    else
        snprintf(buf, buflen, "%.0f%c", size, units[i]);
}

/*
 * Size in bytes of one "block" for -s and the "total" line.
 *   -k          -> 1024
 *   BLOCKSIZE   -> its value; suffix k/K, m/M, g/G multiplies
 *                  (a bare suffix such as "K" means 1K)
 *   otherwise   -> 512
 * Anything we cannot parse falls back to 512.
 */
static long long block_size(const Options *opts)
{
    const char *env;
    char *end;
    long long n;

    if (opts->f_kilo)
        return 1024;

    env = getenv("BLOCKSIZE");
    if (env == NULL || env[0] == '\0')
        return 512;

    n = strtoll(env, &end, 10);
    if (end == env)
        n = 1;                       /* "K" alone means 1K */

    switch (*end) {
    case '\0':                       break;
    case 'k': case 'K': n *= 1024;   break;
    case 'm': case 'M': n *= 1024 * 1024;        break;
    case 'g': case 'G': n *= 1024LL * 1024 * 1024; break;
    default:  return 512;
    }

    return (n > 0) ? n : 512;
}

/*
 * Convert a count of 512-byte blocks (st_blocks) into a string.
 *   -h → human-readable bytes
 *   otherwise → count in block_size() units, rounded up
 */
static void format_blocks(blkcnt_t blocks, const Options *opts,
                          char *buf, size_t buflen)
{
    long long bs;

    if (opts->f_humanize) {
        format_size_human((off_t)blocks * 512, buf, buflen);
        return;
    }
    bs = block_size(opts);
    snprintf(buf, buflen, "%lld", ((long long)blocks * 512 + bs - 1) / bs);
}

/* ---- public functions ------------------------------------------- */

char type_indicator(mode_t mode)
{
    if (S_ISDIR(mode))   return '/';
    if (S_ISLNK(mode))   return '@';
    if (S_ISSOCK(mode))  return '=';
    if (S_ISFIFO(mode))  return '|';
#ifdef S_ISWHT
    if (S_ISWHT(mode))   return '%';
#endif
    /* Executable regular file */
    if (S_ISREG(mode) && (mode & (S_IXUSR | S_IXGRP | S_IXOTH)))
        return '*';
    return '\0';
}

void print_name(const char *name, const Options *opts)
{
    mbstate_t st;
    size_t left = strlen(name);

    if (!opts->f_qmark) {            /* -w: print raw bytes */
        fputs(name, stdout);
        return;
    }

    /*
     * -q: walk the name one multibyte character at a time so that
     * valid UTF-8 (e.g. Vietnamese letters) is kept, and only bytes
     * that are invalid or not printable become '?'.
     */
    memset(&st, 0, sizeof(st));
    while (left > 0) {
        wchar_t wc;
        size_t n = mbrtowc(&wc, name, left, &st);

        if (n == (size_t)-1 || n == (size_t)-2) {   /* invalid bytes */
            putchar('?');
            memset(&st, 0, sizeof(st));
            name++;
            left--;
        } else if (n == 0 || !iswprint((wint_t)wc)) { /* NUL / control */
            putchar('?');
            name += (n ? n : 1);
            left -= (n ? n : 1);
        } else {
            fwrite(name, 1, n, stdout);
            name += n;
            left -= n;
        }
    }
}

void print_total(Entry **entries, int count, const Options *opts)
{
    blkcnt_t total = 0;
    char buf[64];
    int i;

    for (i = 0; i < count; i++)
        total += entries[i]->st.st_blocks;

    format_blocks(total, opts, buf, sizeof(buf));
    printf("total %s\n", buf);
}

void print_short(Entry **entries, int count, const Options *opts)
{
    int w_ino = 0, w_blk = 0;
    char buf[64];
    int len, i;

    /* Pass 1: widest inode / block-count so the columns line up. */
    for (i = 0; i < count; i++) {
        if (opts->f_inode) {
            len = snprintf(buf, sizeof(buf), "%lu",
                           (unsigned long)entries[i]->st.st_ino);
            if (len > w_ino) w_ino = len;
        }
        if (opts->f_size) {
            format_blocks(entries[i]->st.st_blocks, opts, buf, sizeof(buf));
            len = (int)strlen(buf);
            if (len > w_blk) w_blk = len;
        }
    }

    /* Pass 2: print. */
    for (i = 0; i < count; i++) {
        Entry *e = entries[i];

        if (opts->f_inode)
            printf("%*lu ", w_ino, (unsigned long)e->st.st_ino);

        if (opts->f_size) {
            format_blocks(e->st.st_blocks, opts, buf, sizeof(buf));
            printf("%*s ", w_blk, buf);
        }

        print_name(e->name, opts);

        if (opts->f_type) {
            char c = type_indicator(e->st.st_mode);
            if (c)
                putchar(c);
        }

        putchar('\n');
    }
}

/*
 * print_long — the most complex output routine.
 *
 * We make two passes over the entries:
 *   1. Compute the maximum width of each column so we can align them.
 *   2. Print each entry using those widths.
 *
 * Column layout:
 *   [inode] [blocks] mode nlinks owner group size time name [-> target] [F]
 */
void print_long(Entry **entries, int count, const Options *opts)
{
    /* Column widths (computed in pass 1) */
    int w_ino = 0, w_blk = 0, w_nlink = 0;
    int w_owner = 0, w_group = 0, w_size = 0;
    int w_major = 0, w_minor = 0;
    int has_device = 0;   /* does any entry have a block/char device? */
    char buf[256];
    int len, i;

    /* ---- pass 1: column widths ---------------------------------- */
    for (i = 0; i < count; i++) {
        Entry *e = entries[i];

        if (opts->f_inode) {
            len = snprintf(buf, sizeof(buf), "%lu",
                           (unsigned long)e->st.st_ino);
            if (len > w_ino) w_ino = len;
        }

        if (opts->f_size) {
            format_blocks(e->st.st_blocks, opts, buf, sizeof(buf));
            len = (int)strlen(buf);
            if (len > w_blk) w_blk = len;
        }

        len = snprintf(buf, sizeof(buf), "%lu",
                       (unsigned long)e->st.st_nlink);
        if (len > w_nlink) w_nlink = len;

        /* Owner: name or numeric uid */
        if (opts->f_numeric) {
            len = snprintf(buf, sizeof(buf), "%u",
                           (unsigned)e->st.st_uid);
        } else {
            struct passwd *pw = getpwuid(e->st.st_uid);
            if (pw)
                len = (int)strlen(pw->pw_name);
            else
                len = snprintf(buf, sizeof(buf), "%u",
                               (unsigned)e->st.st_uid);
        }
        if (len > w_owner) w_owner = len;

        /* Group: name or numeric gid */
        if (opts->f_numeric) {
            len = snprintf(buf, sizeof(buf), "%u",
                           (unsigned)e->st.st_gid);
        } else {
            struct group *gr = getgrgid(e->st.st_gid);
            if (gr)
                len = (int)strlen(gr->gr_name);
            else
                len = snprintf(buf, sizeof(buf), "%u",
                               (unsigned)e->st.st_gid);
        }
        if (len > w_group) w_group = len;

        /* Size or major,minor for devices */
        if (S_ISBLK(e->st.st_mode) || S_ISCHR(e->st.st_mode)) {
            has_device = 1;
            len = snprintf(buf, sizeof(buf), "%u",
                           (unsigned)major(e->st.st_rdev));
            if (len > w_major) w_major = len;
            len = snprintf(buf, sizeof(buf), "%u",
                           (unsigned)minor(e->st.st_rdev));
            if (len > w_minor) w_minor = len;
        } else {
            if (opts->f_humanize) {
                format_size_human(e->st.st_size, buf, sizeof(buf));
                len = (int)strlen(buf);
            } else {
                len = snprintf(buf, sizeof(buf), "%lld",
                               (long long)e->st.st_size);
            }
            if (len > w_size) w_size = len;
        }
    }

    /*
     * If any device entry exists, the size column must be wide enough
     * to hold "major, minor" (major_width + 2 + minor_width).
     * If the regular size field is wider, widen the minor sub-column
     * so everything aligns.
     */
    if (has_device) {
        int dev_width = w_major + 2 + w_minor;
        if (dev_width > w_size)
            w_size = dev_width;
        else
            w_minor += (w_size - dev_width);
    }

    /* ---- pass 2: print each entry ------------------------------- */
    for (i = 0; i < count; i++) {
        Entry *e = entries[i];
        char modebuf[12];
        char timebuf[64];
        time_t file_time;
        struct tm *tm;

        /* Optional inode column */
        if (opts->f_inode)
            printf("%*lu ", w_ino, (unsigned long)e->st.st_ino);

        /* Optional block-count column */
        if (opts->f_size) {
            format_blocks(e->st.st_blocks, opts, buf, sizeof(buf));
            printf("%*s ", w_blk, buf);
        }

        /* Mode string (drwxr-xr-x) */
        format_mode(e->st.st_mode, modebuf);
        printf("%s ", modebuf);

        /* Hard-link count */
        printf("%*lu ", w_nlink, (unsigned long)e->st.st_nlink);

        /* Owner */
        if (opts->f_numeric) {
            printf("%-*u ", w_owner, (unsigned)e->st.st_uid);
        } else {
            struct passwd *pw = getpwuid(e->st.st_uid);
            if (pw)
                printf("%-*s ", w_owner, pw->pw_name);
            else
                printf("%-*u ", w_owner, (unsigned)e->st.st_uid);
        }

        /* Group */
        if (opts->f_numeric) {
            printf("%-*u ", w_group, (unsigned)e->st.st_gid);
        } else {
            struct group *gr = getgrgid(e->st.st_gid);
            if (gr)
                printf("%-*s ", w_group, gr->gr_name);
            else
                printf("%-*u ", w_group, (unsigned)e->st.st_gid);
        }

        /* Size (or major,minor for devices) */
        if (S_ISBLK(e->st.st_mode) || S_ISCHR(e->st.st_mode)) {
            printf("%*u, %*u ", w_major, (unsigned)major(e->st.st_rdev),
                                w_minor, (unsigned)minor(e->st.st_rdev));
        } else if (opts->f_humanize) {
            format_size_human(e->st.st_size, buf, sizeof(buf));
            printf("%*s ", w_size, buf);
        } else {
            printf("%*lld ", w_size, (long long)e->st.st_size);
        }

        /*
         * Timestamp: "Mon DD HH:MM" for recent files,
         *            "Mon DD  YYYY" for files > 6 months old or
         *            in the future.  This matches traditional ls.
         */
        file_time = entry_time(e, opts);
        tm = localtime(&file_time);
        if (tm) {
            time_t now = time(NULL);
            if (file_time < now - SIX_MONTHS || file_time > now)
                strftime(timebuf, sizeof(timebuf), "%b %e  %Y", tm);
            else
                strftime(timebuf, sizeof(timebuf), "%b %e %H:%M", tm);
        } else {
            /* Fallback if localtime fails (shouldn't happen). */
            snprintf(timebuf, sizeof(timebuf), "??? ?? ??:??");
        }
        printf("%s ", timebuf);

        /* Filename, then the -F indicator directly after it */
        print_name(e->name, opts);
        if (opts->f_type) {
            char c = type_indicator(e->st.st_mode);
            if (c)
                putchar(c);
        }

        /* Symlink target: "name@ -> target" */
        if (S_ISLNK(e->st.st_mode) && e->link_target) {
            printf(" -> ");
            print_name(e->link_target, opts);
        }

        putchar('\n');
    }
}
