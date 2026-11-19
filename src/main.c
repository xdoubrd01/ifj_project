/**
* @file main.c
 * @brief Reads the IFJ26 source from standard input.
 */
#include "parser.h"
#include <stdio.h>

int main(void) {
    parse(stdin);
    return 0;
}
