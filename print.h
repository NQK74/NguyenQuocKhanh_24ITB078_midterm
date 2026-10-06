#ifndef PRINT_H
#define PRINT_H

/*
 * print.h — all output formatting for ls.
 *
 * Handles short format (one name per line), long format (-l/-n),
 * the "total" line, -F suffixes, -q/-w character display, and
 * -h/-k size formatting.
 */

#include <sys/types.h>
#include "entry.h"
#include "options.h"

/* Print entries one-per-line.  Respects -i, -s, -F, -q/-w. */
void print_short(Entry **entries, int count, const Options *opts);

/* Print entries in long format.  Computes column widths for alignment. */
void print_long(Entry **entries, int count, const Options *opts);

/* Print "total N" line (sum of st_blocks).  Respects -h/-k. */
void print_total(Entry **entries, int count, const Options *opts);

/* Print a filename, replacing non-printables with '?' when -q is on. */
void print_name(const char *name, const Options *opts);

/* Return the -F suffix character for a mode, or '\0' if none. */
char type_indicator(mode_t mode);

#endif /* PRINT_H */
