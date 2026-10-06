/*
 * entry.c — build and free file-system entries.
 *
 * Each Entry is populated by a single lstat() call.  For symlinks
 * we also readlink() the target.  The caller owns the Entry and
 * must call entry_free() when done.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <unistd.h>
#include <sys/stat.h>

#include "entry.h"

int entry_build(Entry *ent, const char *dir, const char *name)
{
    char pathbuf[PATH_MAX];
    int  pathlen;

    memset(ent, 0, sizeof(*ent));

    /* Duplicate the display name (basename) */
    ent->name = strdup(name);
    if (!ent->name) {
        ent->error = errno;
        return -1;
    }

    /*
     * Build the full path used for lstat/opendir.
     * If dir is NULL the name IS the path (command-line operand).
     * Avoid double-slash when dir ends with '/'.
     */
    if (dir == NULL || dir[0] == '\0') {
        pathlen = snprintf(pathbuf, sizeof(pathbuf), "%s", name);
    } else {
        const char *sep = (dir[strlen(dir) - 1] == '/') ? "" : "/";
        pathlen = snprintf(pathbuf, sizeof(pathbuf),
                           "%s%s%s", dir, sep, name);
    }

    if (pathlen >= (int)sizeof(pathbuf)) {
        ent->error = ENAMETOOLONG;
        return -1;
    }

    ent->path = strdup(pathbuf);
    if (!ent->path) {
        ent->error = errno;
        return -1;
    }

    /*
     * lstat: get file metadata WITHOUT following symlinks.
     * This is critical — following symlinks during recursion could
     * create infinite loops, and we want to show symlink info, not
     * the target's info.
     */
    if (lstat(ent->path, &ent->st) < 0) {
        ent->error = errno;
        return -1;
    }

    /*
     * For symlinks, read where they point so we can display
     * "name -> target" in long format.
     */
    if (S_ISLNK(ent->st.st_mode)) {
        char linkbuf[PATH_MAX];
        ssize_t len = readlink(ent->path, linkbuf, sizeof(linkbuf) - 1);
        if (len >= 0) {
            linkbuf[len] = '\0';   /* readlink does NOT null-terminate */
            ent->link_target = strdup(linkbuf);
            /* strdup failure is non-fatal; we just won't show the target */
        }
    }

    return 0;
}

void entry_free(Entry *ent)
{
    /* free(NULL) is safe per the C standard */
    free(ent->name);
    free(ent->path);
    free(ent->link_target);
    ent->name = NULL;
    ent->path = NULL;
    ent->link_target = NULL;
}

time_t entry_time(const Entry *ent, const Options *opts)
{
    if (opts->f_ctime)
        return ent->st.st_ctime;
    if (opts->f_atime)
        return ent->st.st_atime;
    return ent->st.st_mtime;       /* default: modification time */
}
