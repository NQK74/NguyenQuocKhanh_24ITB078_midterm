#ifndef ENTRY_H
#define ENTRY_H

/*
 * entry.h — one file-system entry ready for display.
 *
 * Built from a single lstat() call.  Symlink targets are read via
 * readlink().  The caller owns the Entry and must call entry_free()
 * when done.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include "options.h"

typedef struct {
    char        *name;         /* basename (display name), heap-allocated  */
    char        *path;         /* full path for lstat/opendir, heap-alloc  */
    struct stat  st;           /* result of lstat()                        */
    char        *link_target;  /* readlink() result for symlinks, or NULL  */
    int          error;        /* errno if lstat failed, 0 otherwise       */
} Entry;

/*
 * Populate *ent by lstat-ing "dir/name".  If dir is NULL, name is
 * used as-is (for command-line operands).  For symlinks, also reads
 * the link target via readlink().
 *
 * Returns 0 on success.  On failure returns -1 and sets ent->error
 * to the errno value.  The caller is responsible for printing the
 * error message (so it can use the original operand name).
 * Even on failure, entry_free() is safe to call.
 */
int    entry_build(Entry *ent, const char *dir, const char *name);

/* Free heap memory owned by the entry (name, path, link_target). */
void   entry_free(Entry *ent);

/*
 * Return the time to use for sorting/display, chosen by -c (ctime),
 * -u (atime), or default (mtime).
 */
time_t entry_time(const Entry *ent, const Options *opts);

#endif /* ENTRY_H */
