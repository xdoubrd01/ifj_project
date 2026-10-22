#ifndef EXP_PARSER_H
#define EXP_PARSER_H

#include "../src/scanner.h"
#include "../src/symtable.h"

// Function to parse an expression using the precedence parser
// Returns 0 on success, or an error code on failure
int parse_expression(hashtable *ts, Token *next_token);

#endif // EXP_PARSER_H