#ifndef EXP_PARSER_H
#define EXP_PARSER_H

#include "../src/scanner.h"
#include "../src/symtable.h"
#include "expr_tree.h" // AI - UPDATED: result is an expression tree

// AI - UPDATED START
/*
 * Parses an expression using the precedence parser and builds a typed tree.
 *
 * st      - symbol table used for variable lookup
 * pending - NULL, or a token the parser has already read (identifier in
 *           CALL_OR_EXP -> IDENTIFIER CALL_OR_EXP_OPT); its s_val is taken over
 * tok     - in:  current lookahead token (first token after pending)
 *           out: first token NOT belonging to the expression (owned by caller)
 * out     - on success the expression tree (caller frees it by expr_free),
 *           on error NULL
 *
 * Returns 0 on success, or an error code on failure
 * (1 lexical, 2 syntax, 3 undefined variable, 6 type error, 99 internal).
 */
int parse_expression(SymTable *st, Token *pending, Token *tok, ExprNode **out);
// AI - UPDATED END

#endif // EXP_PARSER_H
