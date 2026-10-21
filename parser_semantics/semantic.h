#ifndef SEMANTIC_H
#define SEMANTIC_H

#include "/../src/scanner.h"
#include "/../src/symtable.h"
#include <stdbool.h>

typedef enum {
  EXPR_TYPE_INT,
  EXPR_TYPE_DOUBLE,
  EXPR_TYPE_STRING,
  EXPR_TYPE_BOOLEAN,
  EXPR_TYPE_UNIT,
  EXPR_TYPE_UNDEFINED, // Undefined or unrecognized expression type
} ExprDataType;

// Functions for parser
// Function to declare a variable in the symbol table
void sem_declare_variable(hashtable *ts, const char *var_name, data_type type, bool is_mutable);
// Function to check if a variable is declared in the symbol table
void sem_check_assignment(hashtable *ts, const char *var_name, data_type new_value_type);
data_type sem_get_variable_type(hashtable *ts, const char *var_name);
data_type sem_get_binary_op_result(TokenType op, data_type left_type, data_type right_type);

#endif // SEMANTIC_H