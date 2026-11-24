// Project:    IFJ26
// Filename:   exp_parser.h
// Author:     Jakub Grásgruber <xgrasgj00>

#ifndef EXP_PARSER_H
#define EXP_PARSER_H

#include "expr_tree.h" // AI - UPDATED: result is an expression tree
#include "scanner.h"
#include "symtable.h"

// prompt: "Look at origin/development (src/parser.c, src/scanner.h,
//          src/symtable.h). Adapt parse_expression so it returns an
//          expression tree instead of printing code, and so it fits the
//          lookahead the parser uses (identifier already read in CALL_OR_EXP).
//          Propose the interface first."
// AI - UPDATED: was int parse_expression(hashtable *ts, Token *next_token,
//               Token *next_token) (duplicate parameter, did not compile)
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

#endif // EXP_PARSER_H
