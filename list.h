#ifndef LIST_H
#define LIST_H

/*
 * list.h — process command-line operands and list directories.
 *
 * Separates file operands from directory operands, sorts each group,
 * prints files first, then lists each directory's contents.
 * Handles -d (directories as files) and -R (recursive descent).
 */

#include "options.h"

/*
 * Main entry point after option parsing.  Process operands[0..count-1].
 * If count == 0, lists ".".
 * Returns 0 on full success, >0 if any error occurred.
 */
int list_operands(char **operands, int count, const Options *opts);

#endif /* LIST_H */
