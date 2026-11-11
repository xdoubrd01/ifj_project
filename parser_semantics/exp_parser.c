#include "exp_parser.h"
#include "../src/scanner.h"
#include "semantic.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// index of token for precedence table
typedef enum {
  PREC_IDX_ADD_SUB,  // 0: Addition and subtraction (+ -)
  PREC_IDX_MUL_DIV,  // 1: Multiplication and division (* /)
  PREC_IDX_RELATION, // 2: <, <=, >, >=
  PREC_IDX_EQ,       // 3: ==, !=
  PREC_IDX_LPAREN,   // 4: (
  PREC_IDX_RPAREN,   // 5: )
  PREC_IDX_VALUE,    // 6: Integer, double, string, identifier
  PREC_IDX_EOF,      // 7: End of file
  PREC_IDX_UNARY,    // 8: Unary + - (OPRAVENO: index musí být 8)
  PREC_IDX_NONE      // 9: undefined or unrecognized token
} prec_idx_t;

// Function to get the precedence index (prec_idx_t) for a given token
prec_idx_t get_precedence_index(TokenType current_type, TokenType prev_type) {
  switch (current_type) {
  case TOKEN_PLUS:
  case TOKEN_MINUS:
    if (prev_type == TOKEN_EOF || prev_type == TOKEN_LPAREN ||
        prev_type == TOKEN_PLUS || prev_type == TOKEN_MINUS ||
        prev_type == TOKEN_MUL || prev_type == TOKEN_DIV ||
        prev_type == TOKEN_LT || prev_type == TOKEN_LEQ ||
        prev_type == TOKEN_GT || prev_type == TOKEN_GEQ ||
        prev_type == TOKEN_EQ || prev_type == TOKEN_NEQ) {
      return PREC_IDX_UNARY; // Unary + or - operator
    } else {
      return PREC_IDX_ADD_SUB; // Binary + or - operator
    }
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
  case TOKEN_EOF:
  case TOKEN_EOL:
  case TOKEN_SEMICOLON:
  case TOKEN_KW_DO:
  case TOKEN_KW_THEN:
  case TOKEN_COMMA:
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
 * = = equal precedence (used to match parentheses "()")
 */
char prec_table[9][9] = {
    // +-   */   <=>  ==!=  (    )   val  EOF  UNARY
    {'>', '<', '>', '>', '<', '>', '<', '>', '<'}, // +-
    {'>', '>', '>', '>', '<', '>', '<', '>', '<'}, // */
    {'<', '<', 'E', '>', '<', '>', '<', '>', '<'}, // REL <=>
    {'<', '<', '<', 'E', '<', '>', '<', '>', '<'}, // EQ == !=
    {'<', '<', '<', '<', '<', '=', '<', 'E', '<'}, // (
    {'>', '>', '>', '>', 'E', '>', 'E', '>', 'E'}, // )
    {'>', '>', '>', '>', 'E', '>', 'E', '>', 'E'}, // value
    {'<', '<', '<', '<', '<', 'E', '<', 'E', '<'}, // EOF
    {'>', '>', '>', '>', '<', '>', '<', '>', '<'}  // UNARY
};

// STACK IMPLEMENTATION FOR PRECEDENCE PARSER

// Enum representing the type of symbol in the precedence parser
typedef enum {
  SYM_TERMINAL,     // Terminal symbol (operator or operand)
  SYM_NON_TERMINAL, // Non-terminal symbol (result of a reduction)
  SYM_STOP, // Special symbol to indicate the bottom or start of the stack
  SYM_SHIFT_MARKER // Special symbol to indicate a shift marker
} Symbol_type;

// Structure representing an item in the precedence parser's stack
typedef struct {
  Symbol_type type;
  TokenType token_type; // The token associated with the symbol
  data_type data_type;  // The data type of the expression (if applicable)
  prec_idx_t prec_idx;  // The precedence index of the symbol
  Token token;          // Store the actual token for semantic analysis
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

void insert_shift_marker() {
  int top_index = stack_top;
  while (top_index >= 0 && stack[top_index].type != SYM_NON_TERMINAL) {
    top_index--;
  }

  if (top_index >= STACK_MAX_SIZE - 1) {
    fprintf(stderr, "Stack overflow while inserting shift marker\n");
    exit(99);
  }

  for (int i = stack_top; i > top_index; i--) {
    stack[i + 1] = stack[i];
  }

  Stack_Item marker = {.type = SYM_SHIFT_MARKER, .token_type = TOKEN_EOF};
  stack[top_index + 1] = marker;
  stack_top++;
}

// this function prints a string in IFJcode26 format, escaping special
// characters
void print_ifj_string(const char *str) {
  if (!str)
    return;
  while (*str) {
    unsigned char c = (unsigned char)(*str);
    if (c <= 32 || c == 35 || c == 92) { // 35 = #, 92 = \ 
            printf("\\%03d", c);
    } else {
      putchar(c);
    }
    str++;
  }
}

int parse_expression(hashtable *ts) {
  // Initialize the stack with a stop symbol
  Stack_Item stop_item = {
      .type = SYM_STOP, .token_type = TOKEN_EOF, .prec_idx = PREC_IDX_EOF};
  push(stop_item);

  TokenType prev_token_type = TOKEN_EOF; // OPRAVENO
  Token current_token;

if (first_token != NULL) {
    current_token = *first_token; // use first token
  } else {
    if (get_next_token(&current_token) != 0) {
      fprintf(stderr, "Lexical error while getting the first token\n");
      return 1; 
    }
  }

  while (1) {
    // Get the top terminal from the stack and determine precedence
    Stack_Item top_terminal = get_top_terminal();
    prec_idx_t row_idx = top_terminal.prec_idx;

    // Get the precedence index for the current token
    prec_idx_t column_idx =
        get_precedence_index(current_token.type, prev_token_type);

    if (column_idx == PREC_IDX_NONE) {
      column_idx = PREC_IDX_EOF; // Treat unrecognized tokens as EOF
    }

    // STOP CONDITION if EOF or RPAREN without LPAREN
    bool is_stop_token = (column_idx == PREC_IDX_EOF || current_token.type == TOKEN_RPAREN);

    if (is_stop_token && top_terminal.prec_idx == PREC_IDX_EOF) {
      if (stack_top == 1 && stack[1].type == SYM_NON_TERMINAL) {
        break; // END OF WHILE CYCLE
      } else {
        fprintf(stderr, "Syntax error: invalid or empty expression\n");
        return 2;
      }
    }

    // if we still here, continue with table
    char precedence_relation = prec_table[row_idx][column_idx];

    // shift
    if (precedence_relation == '<') {
      insert_shift_marker();
      Stack_Item new_item = {
          .type = SYM_TERMINAL,
          .token_type = current_token.type,
          .prec_idx = column_idx,
          .token = current_token // saves lexem
      };

      if (current_token.type == TOKEN_INT_LITERAL) {
        new_item.data_type = type_int;
      } else if (current_token.type == TOKEN_DOUBLE_LITERAL) {
        new_item.data_type = type_double;
      } else if (current_token.type == TOKEN_STRING_LITERAL) {
        new_item.data_type = type_string;
      } else if (current_token.type == TOKEN_IDENTIFIER) {
        new_item.data_type =
            sem_get_variable_type(ts, current_token.attribute.s_val);
      } else {
        new_item.data_type = type_unit;
      }

      push(new_item);
      prev_token_type = current_token.type;

      if (get_next_token(&current_token) != 0) {
        fprintf(stderr, "Lexical error while getting the next token\n");
        return 1; // Lexical error
      }

      // match
    } else if (precedence_relation == '=') {
      Stack_Item new_item = {.type = SYM_TERMINAL,
                             .token_type = current_token.type,
                             .prec_idx = column_idx,
                             .token = current_token};

      push(new_item);
      prev_token_type = current_token.type;

      if (get_next_token(&current_token) != 0) {
        fprintf(stderr, "Lexical error while getting the next token\n");
        return 1;
      }

      // reduction
    } else if (precedence_relation == '>') {
      Stack_Item popped_items[4];
      int pop_count = 0;
      bool found_shift_marker = false;

      while (stack_top >= 0) {
        Stack_Item item = pop();
        if (item.type == SYM_SHIFT_MARKER) {
          found_shift_marker = true;
          break;
        }
        if (pop_count < 4) {
          popped_items[pop_count] = item;
          pop_count++;
        } else {
          fprintf(stderr, "Syntax error: too many items to reduce\n");
          return 2;
        }
      }

      if (!found_shift_marker) {
        fprintf(stderr,
                "Syntax error: no shift marker found during reduction\n");
        return 2;
      }

      // E -> id / value
      if (pop_count == 1 && popped_items[0].type == SYM_TERMINAL) {
        Stack_Item item = popped_items[0];

        // Semantic action: push the value onto the stack in IFJcode26
        if (item.token_type == TOKEN_INT_LITERAL) {
          printf("PUSHS int@%d\n", item.token.attribute.i_val);
        } else if (item.token_type == TOKEN_DOUBLE_LITERAL) {
          printf("PUSHS float@%a\n",
                 item.token.attribute.d_val); // %a for double
        } else if (item.token_type == TOKEN_STRING_LITERAL) {
          printf("PUSHS string@");
          print_ifj_string(item.token.attribute.s_val);
          printf("\n");
        } else if (item.token_type == TOKEN_IDENTIFIER) {
          printf("PUSHS LF@%s\n", item.token.attribute.s_val);
        }

        Stack_Item new_item = {.type = SYM_NON_TERMINAL,
                               .token_type = TOKEN_EOF,
                               .data_type = item.data_type};
        push(new_item);

        // 2: E -> E op E
      } else if (pop_count == 3 && popped_items[0].type == SYM_NON_TERMINAL &&
                 popped_items[1].type == SYM_TERMINAL &&
                 popped_items[2].type == SYM_NON_TERMINAL) {

        Stack_Item left_operand = popped_items[2];
        Stack_Item op_item = popped_items[1];
        Stack_Item right_operand = popped_items[0];
        TokenType op = op_item.token_type;

        Stack_Item new_item = {.type = SYM_NON_TERMINAL,
                               .token_type = TOKEN_EOF};

        new_item.data_type = sem_get_binary_op_result(
            op, left_operand.data_type, right_operand.data_type);

        // generate IFJcode26 operations
        if (op == TOKEN_PLUS) {
          if (new_item.data_type == type_string) {
            // right operand
            printf("POPS GF@%%tmp2\n");
            // left operand
            printf("POPS GF@%%tmp1\n");
            // konkatenace
            printf("CONCAT GF@%%tmp3 GF@%%tmp1 GF@%%tmp2\n");
            // return result back to stack
            printf("PUSHS GF@%%tmp3\n");
          } else {
            printf("ADDS\n");
          }
        } else if (op == TOKEN_MINUS) {
          printf("SUBS\n");
        } else if (op == TOKEN_MUL) {
          printf("MULS\n");
        } else if (op == TOKEN_DIV) {
          if (new_item.data_type == type_int)
            printf("IDIVS\n");
          else
            printf("DIVS\n");
        } else if (op == TOKEN_EQ) {
          printf("EQS\n");
        } else if (op == TOKEN_NEQ) {
          printf("EQS\nNOTS\n");
        } else if (op == TOKEN_LT) {
          printf("LTS\n");
        } else if (op == TOKEN_LEQ) {
          printf("GTS\nNOTS\n");
        } else if (op == TOKEN_GT) {
          printf("GTS\n");
        } else if (op == TOKEN_GEQ) {
          printf("LTS\nNOTS\n");
        }

        push(new_item);

        // 3: E -> ( E )
      } else if (pop_count == 3 && popped_items[0].type == SYM_TERMINAL &&
                 popped_items[0].token_type == TOKEN_RPAREN &&
                 popped_items[1].type == SYM_NON_TERMINAL &&
                 popped_items[2].type == SYM_TERMINAL &&
                 popped_items[2].token_type == TOKEN_LPAREN) {

        // Závorky negenerují kód, pouze posílají non-terminál dál
        Stack_Item new_item = {
            .type = SYM_NON_TERMINAL,
            .token_type = TOKEN_EOF,
            .data_type = popped_items[1].data_type // Preserve data type
        };
        push(new_item);

        // 4: E -> unary_op E
      } else if (pop_count == 2 && popped_items[0].type == SYM_NON_TERMINAL &&
                 popped_items[1].type == SYM_TERMINAL &&
                 (popped_items[1].token_type == TOKEN_PLUS ||
                  popped_items[1].token_type == TOKEN_MINUS)) {

        Stack_Item operand = popped_items[0];

        if (popped_items[1].token_type == TOKEN_MINUS) {
          if (operand.data_type == type_int) {
            printf("PUSHS int@-1\n");
          } else {
            printf("PUSHS float@-0x1.0p+0\n");
          }
          printf("MULS\n");
        }

        if (operand.data_type == type_int || operand.data_type == type_double) {
          Stack_Item new_item = {.type = SYM_NON_TERMINAL,
                                 .token_type = TOKEN_EOF,
                                 .data_type = operand.data_type};
          push(new_item);
        } else {
          fprintf(stderr, "Semantic error (6): unary operator applied to "
                          "non-numeric type\n");
          return 6;
        }

      } else {
        fprintf(stderr, "Syntax error: invalid reduction pattern\n");
        return 2;
      }

    } else if (precedence_relation == 'E') {
      fprintf(stderr, "Syntax error: unrecognized precedence relation\n");
      return 2;
    }

    // check for end parsing condition
    if (column_idx == PREC_IDX_EOF && top_terminal.token_type == TOKEN_EOF) {
      if (stack_top == 1 && stack[1].type == SYM_NON_TERMINAL) {
        expr_parsed = true;
      } else {
        fprintf(stderr, "Syntax error: unexpected end of input\n");
        return 2;
      }
    }
  } // END OF WHILE CYCLE (1)

  // Save remaining token, wich ended parsing
  if (next_token != NULL) {
    *next_token = current_token;
  }

  return 0; // Successful parsing
}
