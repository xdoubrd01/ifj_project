#include "semantic.h"
#include "/../src/symtable.h" // Include the symtable header for symbol table functions
#include <stdio.h>
#include <stdlib.h>

// Function to declare a variable in the symbol table
void sem_declare_variable(hastable *ts, const char *var_name, data_type type,
                          bool is_mutable) {
// Check if the variable is already declared in the symbol table
  symbol_data *found = symtable_find(ts, var_name);
  if (found != NULL) {
    fprintf(stderr, "Semantic error(4): Variable '%s' is already defined\n",
            var_name);
    exit(4);
  }
  symtable_insert_var(ts, var_name, type, is_mutable);
}

// Function to declare type of a variable in the symbol table
data_type sem_get_variable_type(hashtable *ts, const char *var_name) {
  symbol_data *found = symtable_find(ts, var_name);
  if (found == NULL) {
    fprintf(stderr, "Semantic error(3): Variable '%s' is not defined\n",
            var_name);
    exit(3);
  }
  
  if(found->type == sym_var) {
    return found->data.var.var_type;
  } else {
    fprintf(stderr, "Semantic error(3): '%s' is not a variable\n", var_name);
    exit(3);
  }
}