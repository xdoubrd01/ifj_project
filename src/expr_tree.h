// AI START
#ifndef EXPR_TREE_H
#define EXPR_TREE_H

#include "symtable.h"
#include <stdio.h>

/*
 * Expression tree built by the precedence parser (exp_parser.c).
 * Every node already has its result type checked by semantic analysis.
 *
 * Guarantees for the code generator:
 *  - both operands of EXPR_BINARY always have the same type
 *    (int -> double conversion is an explicit EXPR_INT2FLOAT node)
 *  - DIV / IDIV and ADD / CONCAT are already resolved
 *  - parentheses and unary plus produce no node
 *  - result of a relational operator has type type_bool
 *  - EXPR_VAR carries u.var.id from the symbol table: same name in different
 *    scopes (shadowing) has a different id, e.g. use LF@name$id
 */

// kind of node
typedef enum {
  EXPR_INT_LIT,    // Int literal            -> u.i_val
  EXPR_DOUBLE_LIT, // Double literal         -> u.d_val
  EXPR_STRING_LIT, // String literal         -> u.s_val (raw value, not escaped)
  EXPR_UNIT_LIT,   // Unit literal ()        -> no data
  EXPR_VAR,        // variable / parameter   -> u.var (name + id)
  EXPR_BINARY,     // binary operation       -> u.bin
  EXPR_NEG,        // unary minus            -> u.operand
  EXPR_INT2FLOAT   // implicit Int -> Double -> u.operand
} ExprKind;

// resolved binary operator
typedef enum {
  OP_ADD,    // int + int, double + double         -> ADDS
  OP_SUB,    // -                                  -> SUBS
  OP_MUL,    // *                                  -> MULS
  OP_DIV,    // double / double                    -> DIVS
  OP_IDIV,   // int / int                          -> IDIVS
  OP_CONCAT, // string + string                    -> CONCAT
  OP_EQ,     // ==                                 -> EQS
  OP_NEQ,    // !=                                 -> EQS + NOTS
  OP_LT,     // <                                  -> LTS
  OP_LEQ,    // <=                                 -> GTS + NOTS
  OP_GT,     // >                                  -> GTS
  OP_GEQ     // >=                                 -> LTS + NOTS
} ExprOp;

// one node of the expression tree
typedef struct ExprNode {
  ExprKind kind;  // what the node is
  data_type type; // result type of this node
  union {
    int i_val;    // EXPR_INT_LIT
    double d_val; // EXPR_DOUBLE_LIT
    char *s_val;  // EXPR_STRING_LIT (owned by node)
    struct {
      char *name;  // variable name (owned by node)
      unsigned id; // unique id from symtable (data.var.id)
    } var;         // EXPR_VAR
    struct {
      ExprOp op;              // operator
      struct ExprNode *left;  // left operand (evaluated first)
      struct ExprNode *right; // right operand
    } bin;                    // EXPR_BINARY
    struct ExprNode *operand; // EXPR_NEG, EXPR_INT2FLOAT
  } u;
} ExprNode;

// Allocates a zeroed node of given kind and type, NULL on allocation failure
ExprNode *expr_new(ExprKind kind, data_type type);

// Frees the whole subtree including owned strings; NULL is accepted
void expr_free(ExprNode *node);

// Debug print of the tree in prefix form, e.g. (+ int (var x$0 int) 1)
void expr_dump(const ExprNode *node, FILE *out);

#endif // EXPR_TREE_H
// AI END
