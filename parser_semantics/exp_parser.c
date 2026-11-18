/*
 * exp_parser.c - precedence parser of expressions, builds a typed expression
 *                tree (expr_tree.h) for the code generator
 *
 * Precedence table, stack and the parsing algorithm are my own work.
 * AI (Claude Code, Claude Opus 5.5) was used to rewrite the semantic actions
 * from printf of IFJcode to building the tree and for code review; functions
 * written by AI have "prompt:" above them, my code changed by AI is marked
 * "AI - UPDATED". The precedence table itself was NOT changed by AI.
 * prompt: "Look at origin/development (src/parser.c, src/scanner.h,
 *          src/symtable.h). Rewrite exp_parser.c so that instead of printf it
 *          builds an expression tree with types for my colleague doing the
 *          code generator. Don't change the precedence table, if it needs
 *          a change warn me. Propose the interface first."
 * prompt: "Keep the code as basic and simple as possible, but functional.
 *          Don't change the overall logic of the program, comment the lines
 *          briefly in the same style as the file."
 * prompt: "Check if this part is OK to hand over to the generator colleague,
 *          offer updates for parts that might contain problematic values."
 * prompt: "Add a unique id of the variable (for shadowing) and update my part."
 * prompt: "Add more accurate comments/description of functions, leave
 *          already existing comments."
 * prompt: "Check the comments after my changes, fix the markers"
 * AI - UPDATED: removed top() (unused) and print_ifj_string() (IFJcode string
 *               output belongs to the code generator now) 
 */
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
// AI - UPDATED: static
static prec_idx_t get_precedence_index(TokenType current_type,
                                       TokenType prev_type) {
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
  case TOKEN_EOF: // AI - UPDATED: duplicate 'case TOKEN_EOF' removed
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
// prompt: "Don't change the precedence table, if it needs a change, warn me."
// AI: review notes, table values were not changed
static char prec_table[9][9] = {
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
  ExprNode *node;       // AI - UPDATED: subtree of a non-terminal (owned)
  prec_idx_t prec_idx;  // The precedence index of the symbol
  Token token;          // Store the actual token for semantic analysis
} Stack_Item;

// AI - UPDATED: 100 -> 1000, 100 overflowed already at ~49 nested '('
#define STACK_MAX_SIZE 1000              // Maximum size of the stack
static Stack_Item stack[STACK_MAX_SIZE]; // AI - UPDATED: static
static int stack_top = -1;               // AI - UPDATED: static

static void push(Stack_Item item) { // AI - UPDATED: static
  if (stack_top < STACK_MAX_SIZE - 1) {
    stack[++stack_top] = item;
  } else {
    fprintf(stderr, "Stack overflow\n");
    exit(99);
  }
}

static Stack_Item pop(void) { // AI - UPDATED: static
  if (stack_top >= 0) {
    return stack[stack_top--];
  } else {
    fprintf(stderr, "Stack underflow\n");
    exit(99);
  }
}

static Stack_Item get_top_terminal(void) { // AI - UPDATED: static
  for (int i = stack_top; i >= 0; i--) {
    if (stack[i].type == SYM_TERMINAL || stack[i].type == SYM_STOP) {
      return stack[i];
    }
  }
  fprintf(stderr, "No terminal symbol found in stack\n");
  exit(99);
}

// prompt: (rewrite of exp_parser.c, see header) - bug found by AI while
//          testing the rewrite on "a + b"
// AI - UPDATED: marker was inserted after the topmost non-terminal (or below
//               '$'), now it is inserted right after the topmost terminal
// Inserts '<' marker right AFTER the topmost terminal (or stop symbol),
// e.g. "$ E" + shift '+'  ->  "$ < E +"
static void insert_shift_marker(void) {
  int top_index = stack_top;
  while (top_index >= 0 && stack[top_index].type != SYM_TERMINAL &&
         stack[top_index].type != SYM_STOP) {
    top_index--; // skip non-terminals above the top terminal
  }

  if (stack_top >= STACK_MAX_SIZE - 1) {
    fprintf(stderr, "Stack overflow while inserting shift marker\n");
    exit(99);
  }

  for (int i = stack_top; i > top_index; i--) {
    stack[i + 1] = stack[i]; // move non-terminals one position up
  }

  Stack_Item marker = {.type = SYM_SHIFT_MARKER, .token_type = TOKEN_EOF};
  stack[top_index + 1] = marker;
  stack_top++;
}

// prompt: (rewrite of exp_parser.c, see header) - stack has to be freed on
//          error, tokens and subtrees left on the stack leaked
// Frees heap data owned by one stack item
static void free_item(Stack_Item *item) {
  if (item->type == SYM_NON_TERMINAL) {
    expr_free(item->node); // subtree
    item->node = NULL;
  } else if (item->type == SYM_TERMINAL &&
             (item->token_type == TOKEN_IDENTIFIER ||
              item->token_type == TOKEN_STRING_LITERAL)) {
    free(item->token.attribute.s_val); // lexeme from scanner
    item->token.attribute.s_val = NULL;
  }
}

// Empties the stack and frees everything in it (used on error)
static void stack_clear(void) {
  while (stack_top >= 0) {
    free_item(&stack[stack_top]);
    stack_top--;
  }
}

// prompt: "parse_expression has to fit the parser: in CALL_OR_EXP the
//          identifier is already read before it is known to be an expression,
//          so take that token and the lookahead after it."
// Reads the next token; returns the caller's lookahead first if it is saved
static int read_token(Token *current, Token *saved, bool *use_saved) {
  if (*use_saved) {
    *current = *saved; // token already read by parser
    *use_saved = false;
    return 0;
  }
  return get_next_token(current); // 0, 1 lexical, 99 internal
}

// prompt: "Rule E -> i: instead of PUSHS create a leaf of the tree, type of
//          identifier comes from symtable through semantic.c."
// Rule E -> term: creates a leaf node, takes over s_val of the token
static int make_term_node(SymTable *st, Stack_Item *item, ExprNode **out) {
  ExprNode *node = NULL;
  data_type type;
  unsigned id; // unique variable id from symtable
  int rc;

  switch (item->token_type) {
  case TOKEN_INT_LITERAL:
    node = expr_new(EXPR_INT_LIT, type_int);
    if (node != NULL) {
      node->u.i_val = item->token.attribute.i_val;
    }
    break;
  case TOKEN_DOUBLE_LITERAL:
    node = expr_new(EXPR_DOUBLE_LIT, type_double);
    if (node != NULL) {
      node->u.d_val = item->token.attribute.d_val;
    }
    break;
  case TOKEN_STRING_LITERAL:
    node = expr_new(EXPR_STRING_LIT, type_string);
    if (node != NULL) {
      node->u.s_val = item->token.attribute.s_val; // ownership -> node
      item->token.attribute.s_val = NULL;
    }
    break;
  case TOKEN_IDENTIFIER:
    // variable must be visible and have a known type (3 / 6)
    if ((rc = sem_get_variable_type(st, item->token.attribute.s_val, &type,
                                    &id)) != 0) {
      return rc;
    }
    node = expr_new(EXPR_VAR, type);
    if (node != NULL) {
      node->u.var.name = item->token.attribute.s_val; // ownership -> node
      node->u.var.id = id;                            // shadowing resolved
      item->token.attribute.s_val = NULL;
    }
    break;
  default:
    // operator alone, e.g. "a *" at the end of expression
    fprintf(stderr, "Syntax error: missing operand\n");
    return 2;
  }

  if (node == NULL) {
    fprintf(stderr, "Internal error (99): out of memory\n");
    return 99;
  }
  *out = node;
  return 0;
}

// prompt: "Rewrite parse_expression so that instead of printf it builds the
//          tree and the types are checked in semantic.c." + (answer to AI)
//          "Returning codes instead of exit() is fine."
// AI - UPDATED: parameters (was parse_expression(hashtable *ts) using undefined
//               first_token), semantic actions build ExprNode, every error
//               goes through the error label
// Parses one expression, see exp_parser.h for the parameters
int parse_expression(SymTable *st, Token *pending, Token *tok, // AI - UPDATED
                     ExprNode **out) {
  // AI - UPDATED: variables for the error label, stack reset
  *out = NULL;                // nothing on error
  stack_top = -1;             // reset stack from previous call
  int rc = 0;                 // error code for the error label
  bool use_saved = false;     // *tok still waits to be read
  Stack_Item popped_items[4]; // handle of the current reduction
  int pop_count = 0;          // number of valid popped_items

  // Initialize the stack with a stop symbol
  Stack_Item stop_item = {
      .type = SYM_STOP, .token_type = TOKEN_EOF, .prec_idx = PREC_IDX_EOF};
  push(stop_item);

  TokenType prev_token_type = TOKEN_EOF; // OPRAVENO
  Token current_token;

  // AI - UPDATED: pending token instead of first_token
  if (pending != NULL) {
    current_token = *pending; // identifier already read by parser
    use_saved = true;         // *tok comes next
  } else {
    current_token = *tok; // first token of the expression
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

    // STOP CONDITION
    bool is_stop_token =
        (column_idx == PREC_IDX_EOF || current_token.type == TOKEN_RPAREN);

    if (is_stop_token && top_terminal.prec_idx == PREC_IDX_EOF) {
      if (stack_top == 1 && stack[1].type == SYM_NON_TERMINAL) {
        break; // END OF WHILE CYCLE
      } else {
        fprintf(stderr, "Syntax error: invalid or empty expression\n");
        rc = 2;     // AI - UPDATED
        goto error; // AI - UPDATED
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
          .node = NULL, // AI - UPDATED: type is resolved at reduction
          .prec_idx = column_idx,
          .token = current_token // saves lexem
      };

      push(new_item);
      prev_token_type = current_token.type;

      // AI - UPDATED: read_token instead of get_next_token (pending token
      // first)
      if ((rc = read_token(&current_token, tok, &use_saved)) != 0) {
        fprintf(stderr, "Lexical error while getting the next token\n");
        current_token.type = TOKEN_EOF; // attributes are invalid
        goto error;
      }

      // match
    } else if (precedence_relation == '=') {
      Stack_Item new_item = {.type = SYM_TERMINAL,
                             .token_type = current_token.type,
                             .node = NULL, // AI - UPDATED
                             .prec_idx = column_idx,
                             .token = current_token};

      push(new_item);
      prev_token_type = current_token.type;

      // AI - UPDATED: read_token instead of get_next_token (pending token
      // first)
      if ((rc = read_token(&current_token, tok, &use_saved)) != 0) {
        fprintf(stderr, "Lexical error while getting the next token\n");
        current_token.type = TOKEN_EOF; // attributes are invalid
        goto error;
      }

      // reduction
    } else if (precedence_relation == '>') {
      pop_count = 0; // AI - UPDATED: popped_items declared at the top
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
          free_item(&item); // AI - UPDATED
          rc = 2;           // AI - UPDATED
          goto error;       // AI - UPDATED
        }
      }

      if (!found_shift_marker) {
        fprintf(stderr,
                "Syntax error: no shift marker found during reduction\n");
        rc = 2;     // AI - UPDATED
        goto error; // AI - UPDATED
      }

      // AI - UPDATED: result of every rule is one non-terminal with a node
      Stack_Item new_item = {
          .type = SYM_NON_TERMINAL, .token_type = TOKEN_EOF, .node = NULL};

      // E -> id / value
      if (pop_count == 1 && popped_items[0].type == SYM_TERMINAL) {
        // AI - UPDATED: leaf node instead of printf PUSHS
        rc = make_term_node(st, &popped_items[0], &new_item.node);
        if (rc != 0) {
          goto error;
        }
        push(new_item);

        // 2: E -> E op E
      } else if (pop_count == 3 && popped_items[0].type == SYM_NON_TERMINAL &&
                 popped_items[1].type == SYM_TERMINAL &&
                 popped_items[2].type == SYM_NON_TERMINAL) {

        // AI - UPDATED: binary node instead of printf, types in
        // sem_binary_op
        Stack_Item *op_item = &popped_items[1];
        if (op_item->prec_idx != PREC_IDX_ADD_SUB &&
            op_item->prec_idx != PREC_IDX_MUL_DIV &&
            op_item->prec_idx != PREC_IDX_RELATION &&
            op_item->prec_idx != PREC_IDX_EQ) {
          fprintf(stderr, "Syntax error: expected binary operator\n");
          rc = 2;
          goto error;
        }

        ExprOp op;      // resolved operator (DIV/IDIV, ADD/CONCAT ...)
        data_type type; // result type
        // left = popped_items[2], right = popped_items[0]; may get INT2FLOAT
        rc = sem_binary_op(op_item->token_type, &popped_items[2].node,
                           &popped_items[0].node, &op, &type);
        if (rc != 0) {
          goto error;
        }

        new_item.node = expr_new(EXPR_BINARY, type);
        if (new_item.node == NULL) {
          fprintf(stderr, "Internal error (99): out of memory\n");
          rc = 99;
          goto error;
        }
        new_item.node->u.bin.op = op;
        new_item.node->u.bin.left = popped_items[2].node; // ownership -> node
        new_item.node->u.bin.right = popped_items[0].node;
        popped_items[2].node = NULL;
        popped_items[0].node = NULL;
        push(new_item);

        // 3: E -> ( E )
      } else if (pop_count == 3 && popped_items[0].type == SYM_TERMINAL &&
                 popped_items[0].token_type == TOKEN_RPAREN &&
                 popped_items[1].type == SYM_NON_TERMINAL &&
                 popped_items[2].type == SYM_TERMINAL &&
                 popped_items[2].token_type == TOKEN_LPAREN) {

        // Závorky negenerují kód, pouze posílají non-terminál dál
        new_item.node = popped_items[1].node; // AI - UPDATED: pass subtree
        popped_items[1].node = NULL;          // AI - UPDATED
        push(new_item);

        // AI - UPDATED: new rule E -> ( ), Unit literal
        // E -> ( )   Unit literal
      } else if (pop_count == 2 && popped_items[0].type == SYM_TERMINAL &&
                 popped_items[0].token_type == TOKEN_RPAREN &&
                 popped_items[1].type == SYM_TERMINAL &&
                 popped_items[1].token_type == TOKEN_LPAREN) {

        new_item.node = expr_new(EXPR_UNIT_LIT, type_unit);
        if (new_item.node == NULL) {
          fprintf(stderr, "Internal error (99): out of memory\n");
          rc = 99;
          goto error;
        }
        push(new_item);

        // 4: E -> unary_op E
      } else if (pop_count == 2 && popped_items[0].type == SYM_NON_TERMINAL &&
                 popped_items[1].type == SYM_TERMINAL &&
                 popped_items[1].prec_idx == PREC_IDX_UNARY) { // AI - UPDATED

        // AI - UPDATED: type check, EXPR_NEG node instead of printf
        // only numbers (6)
        if ((rc = sem_unary_op(popped_items[0].node)) != 0) {
          goto error;
        }

        if (popped_items[1].token_type == TOKEN_MINUS) {
          new_item.node = expr_new(EXPR_NEG, popped_items[0].node->type);
          if (new_item.node == NULL) {
            fprintf(stderr, "Internal error (99): out of memory\n");
            rc = 99;
            goto error;
          }
          new_item.node->u.operand = popped_items[0].node; // ownership
        } else {
          new_item.node = popped_items[0].node; // unary plus: no node
        }
        popped_items[0].node = NULL;
        push(new_item);

      } else {
        fprintf(stderr, "Syntax error: invalid reduction pattern\n");
        rc = 2;     // AI - UPDATED
        goto error; // AI - UPDATED
      }
      pop_count = 0; // AI: popped items are consumed

    } else if (precedence_relation == 'E') {
      fprintf(stderr, "Syntax error: unrecognized precedence relation\n");
      rc = 2;     // AI - UPDATED
      goto error; // AI - UPDATED
    }
  } // END OF WHILE CYCLE (1)

  // AI - UPDATED: return the tree, error label frees everything
  *out = stack[1].node; // finished tree ($ E on the stack)
  stack[1].node = NULL;
  stack_top = -1;
  *tok = current_token; // Save remaining token, wich ended parsing
  return 0;             // Successful parsing

error:
  for (int i = 0; i < pop_count; i++) {
    free_item(&popped_items[i]); // items of unfinished reduction
  }
  stack_clear();        // everything still on the stack
  *tok = current_token; // caller owns the token where we stopped
  return rc;
}
