#include "semantic.h"
#include <stdio.h>
#include <stdlib.h>

// AI START
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
// AI END

// AI - UPDATED START
int sem_declare_variable(SymTable *st, const char *var_name,
                         data_type declared_type, data_type value_type,
                         bool is_mutable) {
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
  return 0;
}

int sem_get_variable_type(SymTable *st, const char *var_name,
                          data_type *out_type) {
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
  return 0;
}
// AI - UPDATED END

// AI START
int sem_check_assignment(SymTable *st, const char *var_name,
                         data_type value_type) {
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
  return 0;
}

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

int sem_check_condition(const ExprNode *cond) {
  if (cond->type != type_bool) {
    // if / while needs a relational expression
    fprintf(stderr, "Semantic error(6): Condition is not a relation\n");
    return 6;
  }
  return 0;
}

int sem_unary_op(const ExprNode *operand) {
  if (!is_numeric(operand->type)) {
    // unary + / - only for numbers
    fprintf(stderr, "Semantic error(6): Unary operator on non-numeric type\n");
    return 6;
  }
  return 0;
}
// AI END

// AI - UPDATED START
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
      fprintf(stderr, "Semantic Error (6): Incompatible types for arithmetic "
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
      fprintf(stderr, "Semantic Error (6): Incompatible types for relational "
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
// AI - UPDATED END
