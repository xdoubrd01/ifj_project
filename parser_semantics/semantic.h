#include "scanner.h"
#include "symtable.h"
#include <stdbool.h>
#ifndef SEMANTIC_H
#define SEMANTIC_H

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
void sem_declare_variable(char *var_name, ExprDataType type, bool is_mutable);

// Function to check if a variable is declared in the symbol table
void sem_check_assignment(char *var_name, ExprDataType new_value_type);

void sem_declare_function(char *func_name, ExprDataType return_type);

// Finds the type of a variable in the symbol table
// Returns EXPR_TYPE_UNDEFINED if the variable is not found
ExprDataType sem_get_variable_type(char *var_name);

#endif // SEMANTIC_H