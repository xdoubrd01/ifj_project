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

//STACK IMPLEMENTATION FOR PRECEDENCE PARSER

//type of symbol in the stack, either terminal or non-terminal
typedef enum {
    SYM_TERMINAL, // Terminal symbol (operator or operand)
    SYM_NON_TERMINAL, // Non-terminal symbol (result of a reduction)
    SYM_STOP // Special symbol to indicate the bottom or start of the stack
} Symbol_type;

// Structure representing an item in the precedence parser's stack
typedef struct {
    Symbol_type type; 
    Tokentype token_type;      // The token associated with the symbol

    // TO DO: Add any additional fields needed for the stack item

} Stack_Item;

#define STACK_MAX_SIZE 100 // Maximum size of the stack
Stack_Item stack[STACK_MAX_SIZE]; // Stack for the precedence parser
int stack_top = -1; // Index of the top of the stack

void push(Stack_Item item) {
    if (stack_top < STACK_MAX_SIZE - 1) {
        stack[++stack_top] = item;
    } else {
        fprintf(stderr, "Stack overflow\n");
        exit(99);
    }
}

Stack_Item pop() {
    if (stack_top >= 0) {
        return stack[stack_top--];
    } else {
        fprintf(stderr, "Stack underflow\n");
        exit(99);
    }
}

Stack_Item top() {
    if (stack_top >= 0) {
        return stack[stack_top];
    } else {
        fprintf(stderr, "Stack is empty\n");
        exit(99);
    }
}

// END OF STACK IMPLEMENTATION FOR PRECEDENCE PARSER