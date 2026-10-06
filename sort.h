#ifndef SORT_H
#define SORT_H

/*
 * sort.h — sort an array of Entry pointers.
 *
 * Uses POSIX qsort() with a file-scope options pointer for the
 * comparators (avoids the non-portable qsort_r).
 */

#include "entry.h"
#include "options.h"

/*
 * Sort entries[0..count-1] according to the active flags:
 *   -t  → by time (newest first, ties broken by name)
 *   -S  → by size (largest first, ties broken by name)
 *   default → lexicographic by name
 *   -r  → reverses whichever order is active
 *   -f  → no sorting at all
 */
void sort_entries(Entry **entries, int count, const Options *opts);

#endif /* SORT_H */
