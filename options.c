/*
 * options.c — parse command-line flags for ls.
 *
 * Uses POSIX getopt().  Mutually exclusive flags are resolved here:
 * the last one on the command line wins (e.g. -l then -n → numeric).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "options.h"

/* Every flag character we accept, in one string for getopt(). */
#define OPTSTRING "AacdFfhiklnqRrSstuw"

void print_usage(void)
{
    fprintf(stderr, "usage: ls [-AacdFfhiklnqRrSstuw] [file ...]\n");
}

int parse_options(int argc, char **argv, Options *opts)
{
    int ch;

    memset(opts, 0, sizeof(*opts));

    /*
     * Detect terminal so we can pick sensible defaults:
     *   tty  → -q (replace non-printables with '?')
     *   pipe → -w (raw characters)
     */
    opts->f_istty = isatty(STDOUT_FILENO);
    if (opts->f_istty)
        opts->f_qmark = 1;
    else
        opts->f_rawchars = 1;

    while ((ch = getopt(argc, argv, OPTSTRING)) != -1) {
        switch (ch) {
        case 'A':
            opts->f_almost = 1;
            opts->f_all = 0;        /* -A and -a: last wins */
            break;
        case 'a':
            opts->f_all = 1;
            opts->f_almost = 0;
            break;
        case 'c':
            opts->f_ctime = 1;
            opts->f_atime = 0;      /* -c and -u: last wins */
            break;
        case 'd':
            opts->f_dironly = 1;
            opts->f_recurse = 0;    /* -d and -R: last wins */
            break;
        case 'F':
            opts->f_type = 1;
            break;
        case 'f':
            opts->f_nosort = 1;
            opts->f_all = 1;        /* -f implies -a */
            opts->f_almost = 0;
            break;
        case 'h':
            opts->f_humanize = 1;
            opts->f_kilo = 0;       /* -h and -k: last wins */
            break;
        case 'i':
            opts->f_inode = 1;
            break;
        case 'k':
            opts->f_kilo = 1;
            opts->f_humanize = 0;
            break;
        case 'l':
            opts->f_long = 1;
            opts->f_numeric = 0;    /* -l and -n: last wins */
            break;
        case 'n':
            opts->f_numeric = 1;
            opts->f_long = 0;
            break;
        case 'q':
            opts->f_qmark = 1;
            opts->f_rawchars = 0;   /* -q and -w: last wins */
            break;
        case 'R':
            opts->f_recurse = 1;
            opts->f_dironly = 0;
            break;
        case 'r':
            opts->f_reverse = 1;
            break;
        case 'S':
            opts->f_sortsize = 1;
            opts->f_sorttime = 0;   /* -S and -t: last wins */
            break;
        case 's':
            opts->f_size = 1;
            break;
        case 't':
            opts->f_sorttime = 1;
            opts->f_sortsize = 0;
            break;
        case 'u':
            opts->f_atime = 1;
            opts->f_ctime = 0;
            break;
        case 'w':
            opts->f_rawchars = 1;
            opts->f_qmark = 0;
            break;
        default:
            /* getopt already printed "invalid option" to stderr */
            print_usage();
            exit(2);
        }
    }

    /* The man page says -A is always set for the super-user. */
    if (geteuid() == 0 && !opts->f_all)
        opts->f_almost = 1;

    return optind; /* index of first non-option argument */
}
