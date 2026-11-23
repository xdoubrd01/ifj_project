// Project:    IFJ26
// Filename:   expr_tree.c
// Author:     Jakub Grásgruber <xgrasgj00>

/*
 * expr_tree.c - helper functions for the expression tree (see expr_tree.h)
 *
 * AI: file written with AI (Claude Code, Claude Opus 5.5), after
 *     discussing the interface, checked and adjusted by me, function
 *     descriptions added by me
 * prompt: "You can create the expr_tree files, keep it as basic and simple as
 *          possible, but functional."
 * prompt: "Add more accurate comments/description of functions, leave
 *          already existing comments."
 * prompt: "My colleague needs function calls inside expressions, e.g.
 *          val x = getValue() + 1, print(2 * getValue()) or
 *          return getValue() + 1. Suggest how to add it to my part without
 *          changing the precedence table."
 */
#include "expr_tree.h"
#include <stdlib.h>

// prompt: "Function that allocates one node of the tree, on allocation failure
//          the caller should report error 99."
// Creates a new expression node of the given kind and type, with all fields
// zeroed
ExprNode *expr_new(ExprKind kind, data_type type) {
  ExprNode *node = calloc(1, sizeof(ExprNode)); // zeroed -> pointers are NULL
  if (node == NULL) {
    return NULL; // caller reports error 99
  }
  node->kind = kind;
  node->type = type;
  return node;
}

// prompt: "Function that frees the whole tree, the nodes own the strings
//          taken over from tokens, so free them too."
// Frees the whole tree recursively, including owned strings and identifiers
void expr_free(ExprNode *node) {
  if (node == NULL) {
    return;
  }
  switch (node->kind) {
  case EXPR_STRING_LIT:
    free(node->u.s_val); // owned string value
    break;
  case EXPR_VAR:
    free(node->u.var.name); // owned identifier
    break;
  case EXPR_BINARY:
    expr_free(node->u.bin.left); // recursively free both operands
    expr_free(node->u.bin.right);
    break;
  case EXPR_NEG:
  case EXPR_INT2FLOAT:
    expr_free(node->u.operand); // single operand
    break;
  // AI - UPDATED: arguments and name of the called function
  case EXPR_CALL:
    for (size_t i = 0; i < node->u.call.argc; i++) {
      expr_free(node->u.call.args[i]);
    }
    free(node->u.call.args);
    free(node->u.call.name);
    break;
  default:
    break; // literals without heap data
  }
  free(node);
}

// prompt: "Debug print of the tree, so I can check the result of the parser
//          on test inputs without the generator."
// AI: type_name / op_name are helpers of expr_dump only
// type name for debug output
static const char *type_name(data_type type) {
  switch (type) {
  case type_int:
    return "int";
  case type_double:
    return "double";
  case type_string:
    return "string";
  case type_unit:
    return "unit";
  case type_bool:
    return "bool";
  case type_any:
    return "any";
  default:
    return "unknown";
  }
}

// operator name for debug output
static const char *op_name(ExprOp op) {
  static const char *names[] = {"+",  "-",  "*", "/",  "idiv", "concat",
                                "==", "!=", "<", "<=", ">",    ">="};
  return names[op];
}

// Dumps the tree in Prefix
// AI: e.g. "i + d" -> (+ double (int2float (var i$0 int)) (var d$1 double))
void expr_dump(const ExprNode *node, FILE *out) {
  if (node == NULL) {
    fprintf(out, "(null)");
    return;
  }
  switch (node->kind) {
  case EXPR_INT_LIT:
    fprintf(out, "%d", node->u.i_val);
    break;
  case EXPR_DOUBLE_LIT:
    fprintf(out, "%g", node->u.d_val);
    break;
  case EXPR_STRING_LIT:
    fprintf(out, "\"%s\"", node->u.s_val);
    break;
  case EXPR_UNIT_LIT:
    fprintf(out, "()");
    break;
  case EXPR_VAR:
    fprintf(out, "(var %s$%u %s)", node->u.var.name, node->u.var.id,
            type_name(node->type));
    break;
  case EXPR_BINARY:
    fprintf(out, "(%s %s ", op_name(node->u.bin.op), type_name(node->type));
    expr_dump(node->u.bin.left, out);
    fprintf(out, " ");
    expr_dump(node->u.bin.right, out);
    fprintf(out, ")");
    break;
  case EXPR_NEG:
    fprintf(out, "(neg %s ", type_name(node->type));
    expr_dump(node->u.operand, out);
    fprintf(out, ")");
    break;
  case EXPR_INT2FLOAT:
    fprintf(out, "(int2float ");
    expr_dump(node->u.operand, out);
    fprintf(out, ")");
    break;
  // AI - UPDATED: function call
  case EXPR_CALL:
    fprintf(out, "(call %s %s", node->u.call.name, type_name(node->type));
    for (size_t i = 0; i < node->u.call.argc; i++) {
      fprintf(out, " ");
      expr_dump(node->u.call.args[i], out);
    }
    fprintf(out, ")");
    break;
  }
}
