/*
 * list.c — process operands and list directories.
 *
 * Flow:
 *   list_operands()
 *     1. lstat each operand; classify as file or directory.
 *     2. Sort file operands, print them.
 *     3. Sort directory operands, call list_dir_contents() for each.
 *
 *   list_dir_contents()
 *     1. opendir/readdir to collect entries.
 *     2. Filter by -a/-A.
 *     3. Sort, print "total" (if -l/-s), print entries.
 *     4. If -R, recurse into subdirectories.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "list.h"
#include "entry.h"
#include "sort.h"
#include "print.h"

/* ---- internal helpers ------------------------------------------- */

/*
 * Read all directory entries, filter by -a/-A, and build an array
 * of Entry pointers.  Returns the count of entries, or -1 if
 * opendir fails.  Sets *status to 1 on any per-entry errors.
 */
static int read_directory(const char *path, const Options *opts,
                          Entry ***out_entries, int *status)
{
    DIR *dir;
    struct dirent *dp;
    Entry **entries = NULL;
    int count = 0;
    int cap = 0;

    dir = opendir(path);
    if (!dir) {
        fprintf(stderr, "ls: %s: %s\n", path, strerror(errno));
        *status = 1;
        return -1;
    }

    /*
     * Read entries one by one.  Setting errno = 0 before readdir()
     * lets us distinguish "end of directory" (returns NULL, errno
     * unchanged) from "error" (returns NULL, errno set).
     */
    errno = 0;
    while ((dp = readdir(dir)) != NULL) {
        const char *name = dp->d_name;

        /* Filter hidden entries based on -a/-A */
        if (name[0] == '.') {
            if (!opts->f_all && !opts->f_almost)
                continue;          /* skip all dotfiles (default) */
            if (opts->f_almost &&
                (strcmp(name, ".") == 0 || strcmp(name, "..") == 0))
                continue;          /* -A: keep dotfiles but skip . and .. */
        }

        /* Grow the array if needed (doubling strategy) */
        if (count >= cap) {
            cap = cap ? cap * 2 : 32;
            Entry **tmp = realloc(entries, (size_t)cap * sizeof(Entry *));
            if (!tmp) {
                fprintf(stderr, "ls: %s\n", strerror(errno));
                goto fail;
            }
            entries = tmp;
        }

        entries[count] = malloc(sizeof(Entry));
        if (!entries[count]) {
            fprintf(stderr, "ls: %s\n", strerror(errno));
            goto fail;
        }

        if (entry_build(entries[count], path, name) < 0) {
            /* lstat failed; print error, skip this entry */
            fprintf(stderr, "ls: %s: %s\n",
                    entries[count]->path ? entries[count]->path : name,
                    strerror(entries[count]->error));
            entry_free(entries[count]);
            free(entries[count]);
            *status = 1;
            errno = 0;
            continue;
        }

        count++;
        errno = 0;
    }

    /* Check if readdir terminated with an error */
    if (errno != 0) {
        fprintf(stderr, "ls: %s: %s\n", path, strerror(errno));
        *status = 1;
    }

    closedir(dir);
    *out_entries = entries;
    return count;

fail:
    /* Cleanup on allocation failure */
    for (int i = 0; i < count; i++) {
        entry_free(entries[i]);
        free(entries[i]);
    }
    free(entries);
    closedir(dir);
    *status = 1;
    return -1;
}

/*
 * List the contents of one directory.
 *
 * header:     if true, print "path:" before the listing.
 * need_blank: in/out flag — if *need_blank is true, print a blank
 *             line before the header (to separate blocks).
 *             Set to true after printing anything.
 *
 * Returns 0 on success, 1 if any error occurred.
 */
static int list_dir_contents(const char *path, const Options *opts,
                             int header, int *need_blank)
{
    Entry **entries = NULL;
    int count;
    int status = 0;

    count = read_directory(path, opts, &entries, &status);
    if (count < 0)
        return 1;          /* opendir failed; error already printed */

    /* Blank line separator between listing blocks */
    if (*need_blank)
        putchar('\n');

    /* "dirname:" header when listing multiple dirs or recursing */
    if (header)
        printf("%s:\n", path);

    *need_blank = 1;

    /* Sort according to active flags */
    sort_entries(entries, count, opts);

    /* "total N" line for -l/-n/-s */
    if (opts->f_long || opts->f_numeric || opts->f_size)
        print_total(entries, count, opts);

    /* Print entries */
    if (opts->f_long || opts->f_numeric)
        print_long(entries, count, opts);
    else
        print_short(entries, count, opts);

    /*
     * -R: recurse into subdirectories.
     * Skip "." and ".." to avoid infinite loops.
     * Only recurse into actual directories, NOT symlinks to
     * directories (lstat gives us S_IFLNK for symlinks, so
     * S_ISDIR is false — this check is sufficient).
     */
    if (opts->f_recurse) {
        for (int i = 0; i < count; i++) {
            if (S_ISDIR(entries[i]->st.st_mode) &&
                strcmp(entries[i]->name, ".") != 0 &&
                strcmp(entries[i]->name, "..") != 0) {
                if (list_dir_contents(entries[i]->path, opts,
                                      1, need_blank) != 0)
                    status = 1;
            }
        }
    }

    /* Free all entries */
    for (int i = 0; i < count; i++) {
        entry_free(entries[i]);
        free(entries[i]);
    }
    free(entries);

    return status;
}

/* ---- public API ------------------------------------------------- */

int list_operands(char **operands, int count, const Options *opts)
{
    Entry **files = NULL;        /* non-directory operands           */
    Entry **dir_ents = NULL;     /* directory operands               */
    int nfiles = 0, ndirs = 0;
    int status = 0;              /* exit status: 0 = OK, 1 = error  */
    int need_blank = 0;          /* print blank line before next block */
    int show_header;

    /* Default to "." when no operands given */
    char *default_op[] = { "." };
    if (count == 0) {
        operands = default_op;
        count = 1;
    }

    files    = calloc((size_t)count, sizeof(Entry *));
    dir_ents = calloc((size_t)count, sizeof(Entry *));
    if (!files || !dir_ents) {
        fprintf(stderr, "ls: %s\n", strerror(errno));
        free(files);
        free(dir_ents);
        return 1;
    }

    /* Classify each operand as file or directory */
    for (int i = 0; i < count; i++) {
        Entry *ent = malloc(sizeof(Entry));
        if (!ent) {
            fprintf(stderr, "ls: %s\n", strerror(errno));
            status = 1;
            continue;
        }

        if (entry_build(ent, NULL, operands[i]) < 0) {
            /* Can't stat this operand — print error and skip */
            fprintf(stderr, "ls: %s: %s\n", operands[i],
                    strerror(ent->error ? ent->error : errno));
            entry_free(ent);
            free(ent);
            status = 1;
            continue;
        }

        /*
         * Decide whether to descend into this operand.
         *   -d → treat everything as a plain file (no descent).
         *   Otherwise, check if it's a directory.  For symlink
         *   operands, follow the link with stat() to see if the
         *   target is a directory (real ls follows symlinks given
         *   as operands, unlike readdir entries).
         */
        int is_dir = 0;
        if (!opts->f_dironly) {
            if (S_ISDIR(ent->st.st_mode)) {
                is_dir = 1;
            } else if (S_ISLNK(ent->st.st_mode)) {
                struct stat target;
                if (stat(ent->path, &target) == 0 &&
                    S_ISDIR(target.st_mode))
                    is_dir = 1;
            }
        }

        if (is_dir)
            dir_ents[ndirs++] = ent;
        else
            files[nfiles++] = ent;
    }

    /* Sort and print non-directory operands first */
    sort_entries(files, nfiles, opts);
    if (nfiles > 0) {
        if (opts->f_long || opts->f_numeric)
            print_long(files, nfiles, opts);
        else
            print_short(files, nfiles, opts);
        need_blank = 1;
    }

    /* Sort directory operands */
    sort_entries(dir_ents, ndirs, opts);

    /*
     * Show "dirname:" headers when there are multiple listing
     * blocks (files + dirs, or multiple dirs, or -R recursion).
     */
    show_header = (nfiles > 0 || ndirs > 1 || opts->f_recurse);

    for (int i = 0; i < ndirs; i++) {
        if (list_dir_contents(dir_ents[i]->path, opts,
                              show_header, &need_blank) != 0)
            status = 1;
    }

    /* Cleanup */
    for (int i = 0; i < nfiles; i++) {
        entry_free(files[i]);
        free(files[i]);
    }
    for (int i = 0; i < ndirs; i++) {
        entry_free(dir_ents[i]);
        free(dir_ents[i]);
    }
    free(files);
    free(dir_ents);

    return status;
}
