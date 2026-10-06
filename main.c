/*
 * main.c — entry point for ls.
 *
 * Parses options, determines the list of operands (default "."),
 * delegates to list_operands(), and returns the exit status.
 */

#include <locale.h>

#include "options.h"
#include "list.h"

int main(int argc, char **argv)
{
    Options opts;
    int first_operand;
    int status;

    /* Use the user's locale so UTF-8 file names print correctly with -q. */
    setlocale(LC_ALL, "");

    first_operand = parse_options(argc, argv, &opts);

    /*
     * argv + first_operand points to the first file/dir operand.
     * argc - first_operand is the number of operands.
     * If there are none, list_operands() defaults to ".".
     */
    status = list_operands(argv + first_operand,
                           argc - first_operand, &opts);

    return status;
}
