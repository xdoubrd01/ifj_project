#ifndef SEMANTIC_H
#define SEMANTIC_H

#include "../src/scanner.h"
#include "../src/symtable.h"
#include "expr_tree.h" // AI - UPDATED: nodes are needed for implicit conversions
#include <stdbool.h>

// AI - UPDATED START
/*
 * All functions return 0 on success or an error code from the specification:
 *   3 = undefined variable, 4 = redefinition, 6 = type error,
 *   7 = write to immutable variable / parameter, 99 = internal error.
 * An error message is printed to stderr, the caller only propagates the code.
 * out_id = unique id of the variable (symtable data.var.id) for the code
 *          generator, may be NULL when not needed.
 */

// Functions for parser
// Declares a variable in the current scope.
// declared_type = type written after ':' or type_unknown when omitted
// value_type    = type of the initializer (expression or call)
int sem_declare_variable(SymTable *st, const char *var_name,
                         data_type declared_type, data_type value_type,
                         bool is_mutable, unsigned *out_id);
// Checks assignment 'var_name = value' (variable exists, is mutable, type)
// If the value is an expression tree, call sem_coerce_expr() afterwards
// so Int -> Double conversion appears in the tree.
int sem_check_assignment(SymTable *st, const char *var_name,
                         data_type value_type, unsigned *out_id);
// Converts expression to the target type (inserts EXPR_INT2FLOAT if needed)
int sem_coerce_expr(ExprNode **expr, data_type target);
// Condition of if / while must be of type Boolean (relational expression)
int sem_check_condition(const ExprNode *cond);

// Functions for expression parser
// Finds a visible variable and stores its type to *out_type
int sem_get_variable_type(SymTable *st, const char *var_name,
                          data_type *out_type, unsigned *out_id);
// Checks a binary operation, may wrap operands in EXPR_INT2FLOAT,
// returns resolved operator and result type
int sem_binary_op(TokenType op, ExprNode **left, ExprNode **right,
                  ExprOp *out_op, data_type *out_type);
// Checks operand of unary + / -
int sem_unary_op(const ExprNode *operand);
// AI - UPDATED END

#endif // SEMANTIC_H
