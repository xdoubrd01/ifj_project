// Project:    IFJ26
// Filename:   semantic.c
// Author:     Jakub Grásgruber <xgrasgj00>

#include "semantic.h"
#include <stdio.h>
#include <stdlib.h>

// prompt: "Int -> Double is the only implicit conversion, the generator needs
//          it explicitly in the tree (INT2FLOATS), add it where both
//          operands must be numbers."
// AI: helpers for sem_binary_op / sem_unary_op / sem_coerce_expr
// true for Int or Double
static bool is_numeric(data_type type) {
  return type == type_int || type == type_double;
}

// wraps an Int node into EXPR_INT2FLOAT, Double node is left as it is
static int to_double(ExprNode **node) {
  if ((*node)->type != type_int) {
    return 0; // already double
  }
  ExprNode *conv = expr_new(EXPR_INT2FLOAT, type_double);
  if (conv == NULL) {
    fprintf(stderr, "Internal error (99): out of memory\n");
    return 99;
  }
  conv->u.operand = *node; // old node becomes operand
  *node = conv;            // caller now holds the conversion node
  return 0;
}

// prompt: "Adapt sem_declare_variable to SymTable: redefinition only in the
//          same block (error 4), type can be omitted and is then taken from
//          the initializer, explicit type must accept it (Int -> Double)."
// AI - UPDATED: hashtable* -> SymTable*, exit() -> return code, type inference
//               and compatibility check added, out_id for the generator
// Declares a new variable in the current scope (val / var)
int sem_declare_variable(SymTable *st, const char *var_name,
                         data_type declared_type, data_type value_type,
                         bool is_mutable, unsigned *out_id) {
  // redefinition only in the same block, shadowing outer one is allowed
  if (symtable_search_current_var(st, var_name) != NULL) {
    fprintf(stderr, "Semantic error(4): Variable '%s' is already defined\n",
            var_name);
    return 4;
  }

  data_type final_type = declared_type;
  if (declared_type == type_unknown) {
    // type inferred from initializer; Boolean cannot be stored (no BOOLTHEN)
    if (value_type != type_int && value_type != type_double &&
        value_type != type_string && value_type != type_unit) {
      fprintf(stderr, "Semantic error(6): Cannot infer type of '%s'\n",
              var_name);
      return 6;
    }
    final_type = value_type;
  } else if (!symtable_type_compatible(declared_type, value_type)) {
    // explicit type must accept the initializer (Int -> Double allowed)
    fprintf(stderr, "Semantic error(6): Incompatible initializer of '%s'\n",
            var_name);
    return 6;
  }

  if (!symtable_insert_var(st, var_name, final_type, is_mutable)) {
    fprintf(stderr, "Internal error (99): cannot insert '%s'\n", var_name);
    return 99; // duplicate was checked above -> allocation failure
  }
  if (out_id != NULL) {
    // id of the new record, generator uses it in DEFVAR
    *out_id = symtable_search_current_var(st, var_name)->data.var.id;
  }
  return 0;
}

// prompt: "Adapt sem_get_variable_type to SymTable, the variable must be
//          visible (3), unknown type is an error (6)."
// AI - UPDATED: symtable_search -> symtable_search_var (shadowing),
//               type_unknown -> error 6, returns code, type via out_type
// Returns the type of a visible variable used in an expression
int sem_get_variable_type(SymTable *st, const char *var_name,
                          data_type *out_type, unsigned *out_id) {
  // nearest visible variable or parameter (respects shadowing)
  symbol_data *found = symtable_search_var(st, var_name);
  if (found == NULL) {
    fprintf(stderr, "Semantic error(3): Variable '%s' is not defined\n",
            var_name);
    return 3;
  }

  if (found->data.var.var_type == type_unknown) {
    // type could not be inferred -> error 6
    fprintf(stderr, "Semantic error(6): Type of '%s' is unknown\n", var_name);
    return 6;
  }

  *out_type = found->data.var.var_type;
  if (out_id != NULL) {
    *out_id = found->data.var.id; // which of the shadowed variables
  }
  return 0;
}

// prompt: "sem_check_assignment is declared in semantic.h but not implemented,
//          implement it: variable must exist (3), val and parameters cannot
//          be assigned (7), type of the variable must accept the value (6)."
// Checks assignment 'name = value' to an existing variable
int sem_check_assignment(SymTable *st, const char *var_name,
                         data_type value_type, unsigned *out_id) {
  symbol_data *found = symtable_search_var(st, var_name);
  if (found == NULL) {
    fprintf(stderr, "Semantic error(3): Variable '%s' is not defined\n",
            var_name);
    return 3;
  }
  if (!found->data.var.is_mutable || found->data.var.is_parameter) {
    // val or function parameter
    fprintf(stderr, "Semantic error(7): '%s' is not mutable\n", var_name);
    return 7;
  }
  if (!symtable_type_compatible(found->data.var.var_type, value_type)) {
    // type of variable never changes
    fprintf(stderr, "Semantic error(6): Incompatible assignment to '%s'\n",
            var_name);
    return 6;
  }
  if (out_id != NULL) {
    *out_id = found->data.var.id; // target variable for MOVE
  }
  return 0;
}

// prompt: "When assigning Int expression to a Double variable the conversion
//          must be in the tree too, add a function for the parser."
// Converts the whole expression to the target type (only Int -> Double)
int sem_coerce_expr(ExprNode **expr, data_type target) {
  if ((*expr)->type == target) {
    return 0; // nothing to do
  }
  if (target == type_double && (*expr)->type == type_int) {
    return to_double(expr); // the only implicit conversion
  }
  fprintf(stderr, "Semantic error(6): Incompatible expression type\n");
  return 6;
}

// prompt: "Condition of if / while has to be a relational expression, add
//          a check the parser can call (Boolean exists only in conditions)."
// Checks that the condition of if / while is a relation (type_bool)
int sem_check_condition(const ExprNode *cond) {
  if (cond->type != type_bool) {
    // if / while needs a relational expression
    fprintf(stderr, "Semantic error(6): Condition is not a relation\n");
    return 6;
  }
  return 0;
}

// prompt: "Add a semantic check of function calls inside expressions: undefined
//          function (3), wrong argument count or type (5), Boolean cannot be
//          passed (6), Int -> Double conversion of arguments in the tree."
// Checks a function call and converts its arguments to the parameter types
int sem_check_call(SymTable *st, const char *name, ExprNode **args, size_t argc,
                   data_type *out_type) {
  symbol_data *func = symtable_search_func(st, name);
  if (func == NULL) {
    fprintf(stderr, "Semantic error(3): Function '%s' is not defined\n", name);
    return 3;
  }
  if (argc != func->data.func.param_count) {
    fprintf(stderr, "Semantic error(5): Wrong argument count of '%s'\n", name);
    return 5;
  }
  for (size_t i = 0; i < argc; i++) {
    data_type expected = func->data.func.params[i];
    // AI - UPDATED: wrong type of a typed parameter is 5, checked first
    if (!symtable_type_compatible(expected, args[i]->type)) {
      fprintf(stderr, "Semantic error(5): Wrong argument type of '%s'\n", name);
      return 5;
    }
    if (args[i]->type == type_bool) {
      // relation can be used only directly in if / while
      fprintf(stderr, "Semantic error(6): Boolean argument of '%s'\n", name);
      return 6;
    }
    if (expected != type_any) {
      int rc = sem_coerce_expr(&args[i], expected); // Int -> Double
      if (rc != 0) {
        return rc;
      }
    }
  }
  *out_type = func->data.func.ret_type;
  return 0;
}

// prompt: "Check of unary + / - operand, only numbers are allowed (6)."
// Checks the operand of unary + / -
int sem_unary_op(const ExprNode *operand) {
  if (!is_numeric(operand->type)) {
    // unary + / - only for numbers
    fprintf(stderr, "Semantic error(6): Unary operator on non-numeric type\n");
    return 6;
  }
  return 0;
}

// prompt: "Adapt sem_get_binary_op_result to the tree: insert the Int -> Double
//          conversion into operands and decide DIV / IDIV and ADD / CONCAT
//          here, so the generator gets the final operator."
// AI - UPDATED: was sem_get_binary_op_result (returned only the type), now it
//               may wrap operands into EXPR_INT2FLOAT and returns ExprOp;
//               == / != on Boolean, unknown and any is an error 6
// Checks a binary operation and resolves its operator and result type
int sem_binary_op(TokenType op, ExprNode **left, ExprNode **right,
                  ExprOp *out_op, data_type *out_type) {
  data_type left_type = (*left)->type;
  data_type right_type = (*right)->type;
  bool are_numeric = is_numeric(left_type) && is_numeric(right_type);
  bool has_double = (left_type == type_double || right_type == type_double);
  int rc = 0;

  if (op == TOKEN_PLUS || op == TOKEN_MINUS || op == TOKEN_MUL ||
      op == TOKEN_DIV) {
    // String + String -> concatenation
    if (op == TOKEN_PLUS && left_type == type_string &&
        right_type == type_string) {
      *out_op = OP_CONCAT;
      *out_type = type_string;
      return 0;
    }
    if (!are_numeric) {
      fprintf(stderr, "Semantic error(6): Incompatible types for arithmetic "
                      "operation.\n");
      return 6;
    }
    if (has_double) {
      // one Double -> convert the other one, result Double
      if ((rc = to_double(left)) != 0 || (rc = to_double(right)) != 0) {
        return rc;
      }
      *out_type = type_double;
    } else {
      *out_type = type_int; // Int op Int -> Int
    }

    if (op == TOKEN_PLUS) {
      *out_op = OP_ADD;
    } else if (op == TOKEN_MINUS) {
      *out_op = OP_SUB;
    } else if (op == TOKEN_MUL) {
      *out_op = OP_MUL;
    } else {
      *out_op = has_double ? OP_DIV : OP_IDIV; // Int / Int is integer division
    }
    return 0;
  }

  if (op == TOKEN_LT || op == TOKEN_LEQ || op == TOKEN_GT || op == TOKEN_GEQ ||
      op == TOKEN_EQ || op == TOKEN_NEQ) {
    bool is_equality = (op == TOKEN_EQ || op == TOKEN_NEQ);
    bool are_strings = (left_type == type_string && right_type == type_string);
    // == / != also for same types (Unit); Boolean is not a value without
    // BOOLTHEN, unknown is never valid
    bool are_same = (left_type == right_type && left_type != type_bool &&
                     left_type != type_unknown && left_type != type_any);

    if (are_numeric) {
      // numbers are compared with Int -> Double conversion
      if (has_double &&
          ((rc = to_double(left)) != 0 || (rc = to_double(right)) != 0)) {
        return rc;
      }
    } else if (!are_strings && !(is_equality && are_same)) {
      fprintf(stderr, "Semantic error(6): Incompatible types for relational "
                      "operation.\n");
      return 6;
    }

    *out_type = type_bool; // return value
    switch (op) {
    case TOKEN_LT:
      *out_op = OP_LT;
      break;
    case TOKEN_LEQ:
      *out_op = OP_LEQ;
      break;
    case TOKEN_GT:
      *out_op = OP_GT;
      break;
    case TOKEN_GEQ:
      *out_op = OP_GEQ;
      break;
    case TOKEN_EQ:
      *out_op = OP_EQ;
      break;
    default:
      *out_op = OP_NEQ;
      break;
    }
    return 0;
  }

  fprintf(stderr,
          "Internal Error (99): Unknown operator in semantic analysis.\n");
  return 99;
}
