#include <stdio.h>
#include <stdlib.h>
#include "mock_scanner.h"

//index of token for precedence table
typedef enum{
    PREC_IDX_ADD_SUB, // 0, Addition and subtraction
    PREC_IDX_MUL_DIV, // 1, Multiplication and division
    PREC_IDX_RELATION,   // 2, <, <=, >, >=
    PREC_IDX_EQ,   // 3, ==, !=
    PREC_IDX_LPAREN,      // 4, (
    PREC_IDX_RPAREN,      // 5, )
    PREC_IDX_VALUE,     // 6, Integer, double, string, identifier
    PREC_IDX_EOF,         // 7, End of file
    PREC_IDX_NONE        // 8, undefined or unrecognized token
} prec_idx_t;


// Function to get the precedence index (prec_idx_t) for a given token
prec_idx_t get_precedence_index(Token token) {
    switch (token.type) {
        case TOKEN_PLUS:
        case TOKEN_MINUS:
            return PREC_IDX_ADD_SUB;
        case TOKEN_MUL:
        case TOKEN_DIV:
            return PREC_IDX_MUL_DIV;
        case TOKEN_LT:
        case TOKEN_LEQ:
        case TOKEN_GT:
        case TOKEN_GEQ:
            return PREC_IDX_RELATION;
        case TOKEN_EQ:
        case TOKEN_NEQ:
            return PREC_IDX_EQ;
        case TOKEN_LPAREN:
            return PREC_IDX_LPAREN;
        case TOKEN_RPAREN:
            return PREC_IDX_RPAREN;
        case TOKEN_INT_LITERAL:
        case TOKEN_DOUBLE_LITERAL:
        case TOKEN_STRING_LITERAL:
        case TOKEN_IDENTIFIER:
            return PREC_IDX_VALUE;
        case TOKEN_EOF:
            return PREC_IDX_EOF;
        default:
            return PREC_IDX_NONE; // Undefined or unrecognized token
    }
}

/*
 * Precedence table for operators and operands
 * row = operator on stack, column = incoming operator

 * < = lower precedence (push incoming operator onto stack)
 * > = higher precedence (pop operator from stack and apply it)
 * E = error (syntax error)
 * = equal precedence (used to match parentheses "()")
 */
char prec_table[8][8] = {
    // +-  */  <=>  == !=  (    )  value  EOF
    { '>', '<', '>', '>', '<', '>', '<', '>' }, // +-
    { '>', '>', '>', '>', '<', '>', '<', '>' }, // */
    { '<', '<', 'E', '>', '<', '>', '<', '>' }, // <=>
    { '<', '<', '<', 'E', '<', '>', '<', '>' }, // == !=
    { '<', '<', '<', '<', '<', '=', '<', 'E' }, // (
    { '>', '>', '>', '>', 'E', '>', 'E', '>' }, // )
    { '>', '>', '>', '>', 'E', '>', 'E', '>' }, // value
    { '<', '<', '<', '<', '<', 'E', '<', 'E' }  // EOF
}