/*
 * IFJ26 compiler - parser_test.c
 * Temporary helper: runs the parser on stdin.
 * Authors: (doplnit login a jmeno)
 *
 * AI-generated segment (Claude), prompt: "docasny main, ktery spusti parser
 * na stdin, abych mohl testovat navratove kody".
 */
#include "parser.h"
#include "scanner.h"
#include <stdio.h>

int main(void) {
    scanner_init(stdin);
    parse();
    scanner_free();
    return 0;
}