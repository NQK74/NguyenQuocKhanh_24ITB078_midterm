/*
 * sort.c — sort arrays of Entry pointers.
 *
 * We need to pass the Options struct to qsort comparators, but POSIX
 * qsort() has no user-data parameter (unlike the GNU extension
 * qsort_r).  The workaround: store a file-scope pointer to the
 * active Options before calling qsort().  This is safe because we
 * never sort from multiple threads.
 */

#include <stdlib.h>
#include <string.h>

#include "sort.h"

/* Set by sort_entries() before calling qsort(). */
static const Options *sort_opts;

/* ---- comparators ------------------------------------------------ */

/*
 * Default: lexicographic by basename.
 * Also used as the tie-breaker for -S and -t.
 */
static int cmp_name(const void *a, const void *b)
{
    const Entry *ea = *(const Entry *const *)a;
    const Entry *eb = *(const Entry *const *)b;
    int result = strcmp(ea->name, eb->name);

    return sort_opts->f_reverse ? -result : result;
}

/* -S: largest file first; ties broken by name. */
static int cmp_size(const void *a, const void *b)
{
    const Entry *ea = *(const Entry *const *)a;
    const Entry *eb = *(const Entry *const *)b;
    int result;

    if (eb->st.st_size > ea->st.st_size)
        result = 1;
    else if (eb->st.st_size < ea->st.st_size)
        result = -1;
    else
        return cmp_name(a, b);     /* tie-break (includes -r) */

    return sort_opts->f_reverse ? -result : result;
}

/* -t: newest first; ties broken by name. */
static int cmp_time(const void *a, const void *b)
{
    const Entry *ea = *(const Entry *const *)a;
    const Entry *eb = *(const Entry *const *)b;
    time_t ta = entry_time(ea, sort_opts);
    time_t tb = entry_time(eb, sort_opts);
    int result;

    if (tb > ta)
        result = 1;
    else if (tb < ta)
        result = -1;
    else
        return cmp_name(a, b);     /* tie-break */

    return sort_opts->f_reverse ? -result : result;
}

/* ---- public API ------------------------------------------------- */

void sort_entries(Entry **entries, int count, const Options *opts)
{
    if (opts->f_nosort || count <= 1)
        return;

    /* Store options where comparators can reach them. */
    sort_opts = opts;

    if (opts->f_sorttime)
        qsort(entries, (size_t)count, sizeof(Entry *), cmp_time);
    else if (opts->f_sortsize)
        qsort(entries, (size_t)count, sizeof(Entry *), cmp_size);
    else
        qsort(entries, (size_t)count, sizeof(Entry *), cmp_name);
}
