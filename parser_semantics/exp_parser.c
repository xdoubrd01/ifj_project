#include "mock_scanner.h"
#include <stdio.h>
#include <stdlib.h>

// index of token for precedence table
typedef enum {
  PREC_IDX_ADD_SUB,  // 0, Addition and subtraction
  PREC_IDX_MUL_DIV,  // 1, Multiplication and division
  PREC_IDX_RELATION, // 2, <, <=, >, >=
  PREC_IDX_EQ,       // 3, ==, !=
  PREC_IDX_LPAREN,   // 4, (
  PREC_IDX_RPAREN,   // 5, )
  PREC_IDX_VALUE,    // 6, Integer, double, string, identifier
  PREC_IDX_EOF,      // 7, End of file
  PREC_IDX_NONE      // 8, undefined or unrecognized token
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
char prec_table[8][8] =
    {
        // +-  */  <=>  == !=  (    )  value  EOF
        {'>', '<', '>', '>', '<', '>', '<', '>'}, // +-
        {'>', '>', '>', '>', '<', '>', '<', '>'}, // */
        {'<', '<', 'E', '>', '<', '>', '<', '>'}, // <=>
        {'<', '<', '<', 'E', '<', '>', '<', '>'}, // == !=
        {'<', '<', '<', '<', '<', '=', '<', 'E'}, // (
        {'>', '>', '>', '>', 'E', '>', 'E', '>'}, // )
        {'>', '>', '>', '>', 'E', '>', 'E', '>'}, // value
        {'<', '<', '<', '<', '<', 'E', '<', 'E'}  // EOF
}

// STACK IMPLEMENTATION FOR PRECEDENCE PARSER

// type of symbol in the stack, either terminal or non-terminal
typedef enum {
  SYM_TERMINAL,     // Terminal symbol (operator or operand)
  SYM_NON_TERMINAL, // Non-terminal symbol (result of a reduction)
  SYM_STOP // Special symbol to indicate the bottom or start of the stack
} Symbol_type;

// Structure representing an item in the precedence parser's stack
typedef struct {
  Symbol_type type;
  Tokentype token_type; // The token associated with the symbol

  // TO DO: Add any additional fields needed for the stack item

} Stack_Item;

#define STACK_MAX_SIZE 100        // Maximum size of the stack
Stack_Item stack[STACK_MAX_SIZE]; // Stack for the precedence parser
int stack_top = -1;               // Index of the top of the stack

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

Stack_Item get_top_terminal() {
  for (int i = stack_top; i >= 0; i--) {
    if (stack[i].type == SYM_TERMINAL || stack[i].type == SYM_STOP) {
      return stack[i];
    }
  }
  fprintf(stderr, "No terminal symbol found in stack\n");
  exit(99);
}

// END OF STACK IMPLEMENTATION FOR PRECEDENCE PARSER

typedef enum {
  SYM_TERMINAL,     // Terminal symbol (operator or operand)
  SYM_NON_TERMINAL, // Non-terminal symbol (result of a reduction)
  SYM_STOP // Special symbol to indicate the bottom or start of the stack
      SYM_SHIFT_MARKER // Special symbol to indicate a shift marker for
                       // reduction
} Symbol_type;

void insert_shift_marker() {
  // Find the index of the last non-terminal in the stack
  int top_index = stack_top;
  while (top_index >= 0 && stack[top_index].type != SYM_NON_TERMINAL) {
    top_index--;
  }

  if (top_index >= STACK_MAX_SIZE - 1) {
    fprintf(stderr, "Stack overflow while inserting shift marker\n");
    exit(99);
  }

  // Shift elements above the last non-terminal up by one position to make space
  // for the shift marker
  for (int i = stack_top; i > top_index; i--) {
    stack[i + 1] = stack[i];
  }

  // Insert the shift marker after the last non-terminal
  Stack_Item marker = {.type = SYM_SHIFT_MARKER, .token_type = TOKEN_EOF};
  stack[top_index + 1] = marker;
  stack_top++; // Update the top index of the stack
}

int parse_expression() {
  // Initialize the stack with a stop symbol
  Stack_Item stop_item = {.type = SYM_STOP, .token_type = TOKEN_EOF};
  push(stop_item);

  Token current_token;
  if (get_next_token(&current_token) != 0) {
    fprintf(stderr, "Lexical error while getting the first token\n");
    return 1; // Lexical error
  }

  bool expr_parsed = false;

  while (!expr_parsed) {
    // Get the top terminal from the stack and determine precedence
    Stack_Item top_terminal = get_top_terminal();
    prec_idx_t row_idx = get_precedence_index(top_terminal.token_type);
    // Get the precedence index for the current token
    prec_idx_t column_idx = get_precedence_index(current_token.type);

    if (column_idx == PREC_IDX_NONE) {
      column_idx = PREC_IDX_EOF; // Treat unrecognized tokens as EOF for
                                 // precedence purposes
    }

    char precedence_relation = prec_table[row_idx][column_idx];

    if (precedence_relation == '<') {
      // Shift operation
      // Insert a shift marker before pushing the current token onto the stack
      insert_shift_marker();
      Stack_Item new_item = {.type = SYM_TERMINAL,
                             .token_type = current_token.type};
      push(new_item);

      if (get_next_token(&current_token) != 0) {
        fprintf(stderr, "Lexical error while getting the next token\n");
        return 1; // Lexical error
      }
    } else if (precedence_relation == '>') {
      // Reduce operation
      // TO DO: Implement reduction logic based on the grammar rules
      pop();
    } else if (precedence_relation == '=') {
      Stack_Item new_item = {.type = SYM_TERMINAL,
                             .token_type = current_token.type};
      push(new_item);
      get_next_token(&current_token);

    } else if (precedence_relation == 'E') {
      fprintf(stderr, "Syntax error: unrecognized precedence relation\n");
      return 2; // Syntax error
    }

    // Check for end of parsing condition
    if (column_idx == PREC_IDX_EOF && top_terminal.token_type == TOKEN_EOF) {
      if (stack_top == 1 && stack[1].type == SYM_NON_TERMINAL) {
        // Only the stop symbol remains in the stack, parsing is complete
        expr_parsed = true;
      } else {
        fprintf(stderr, "Syntax error: unexpected end of input\n");
        return 2; // Syntax error
      }
    }
  }
  return 0; // Successful parsing
}
