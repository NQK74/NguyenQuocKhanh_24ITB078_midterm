#ifndef OPTIONS_H
#define OPTIONS_H

/*
 * options.h — command-line flags for ls.
 *
 * Each flag is stored as a plain int (0/1).  Mutually exclusive flags
 * (like -l/-n, -c/-u, -d/-R, -h/-k, -q/-w) are resolved at parse
 * time: the last one on the command line wins.
 */

typedef struct {
    int f_all;       /* -a  show everything including . and ..         */
    int f_almost;    /* -A  show dotfiles except . and ..              */
    int f_long;      /* -l  long listing format                        */
    int f_numeric;   /* -n  like -l but numeric uid/gid                */
    int f_recurse;   /* -R  recurse into subdirectories                */
    int f_dironly;   /* -d  treat directories as plain files            */
    int f_inode;     /* -i  print inode number                         */
    int f_size;      /* -s  print block count per entry                */
    int f_type;      /* -F  append type indicator (/ * @ = | %)        */
    int f_reverse;   /* -r  reverse sort order                         */
    int f_sortsize;  /* -S  sort by file size (largest first)          */
    int f_sorttime;  /* -t  sort by time (newest first)                */
    int f_nosort;    /* -f  do not sort (implies -a)                   */
    int f_humanize;  /* -h  human-readable sizes (1.5K, 23M)           */
    int f_kilo;      /* -k  sizes in 1024-byte blocks                  */
    int f_rawchars;  /* -w  print raw non-printable characters         */
    int f_qmark;     /* -q  replace non-printables with '?'            */
    int f_ctime;     /* -c  use ctime instead of mtime                 */
    int f_atime;     /* -u  use atime instead of mtime                 */
    int f_istty;     /* 1 if stdout is a terminal (auto-detected)      */
} Options;

/*
 * Parse argv starting at index 1.  Fills *opts and returns the index
 * of the first non-option argument (optind after getopt finishes).
 * On unknown option, prints usage to stderr and exits with status 2.
 */
int  parse_options(int argc, char **argv, Options *opts);

/* Print "usage: ls [...]" to stderr. */
void print_usage(void);

#endif /* OPTIONS_H */
