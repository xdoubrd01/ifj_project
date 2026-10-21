#include "semantic.h"
#include <stdio.h>
#include <stdlib.h>

void sem_declare_variable(hashtable *ts, const char *var_name, data_type type, bool is_mutable) {
  // OPRAVA: Kolegova funkce se jmenuje symtable_search
  symbol_data *found = symtable_search(ts, var_name);
  if (found != NULL) {
    fprintf(stderr, "Semantic error(4): Variable '%s' is already defined\n", var_name);
    exit(4);
  }
  symtable_insert_var(ts, var_name, type, is_mutable);
}

data_type sem_get_variable_type(hashtable *ts, const char *var_name) {
  // OPRAVA: Kolegova funkce se jmenuje symtable_search
  symbol_data *found = symtable_search(ts, var_name);
  if (found == NULL) {
    fprintf(stderr, "Semantic error(3): Variable '%s' is not defined\n", var_name);
    exit(3);
  }
  
  if(found->type == sym_var) {
    return found->data.var.var_type;
  } else {
    fprintf(stderr, "Semantic error(3): '%s' is not a variable\n", var_name);
    exit(3);
  }
}

// DOPLNĚNÍ CHYBĚJÍCÍ FUNKCE (nutné pro exp_parser.c)
data_type sem_get_binary_op_result(TokenType op, data_type left_type, data_type right_type) {
    if (op == TOKEN_PLUS || op == TOKEN_MINUS || op == TOKEN_MUL || op == TOKEN_DIV) {
        if (left_type == type_int && right_type == type_int) return type_int;
        if (op == TOKEN_PLUS && left_type == type_string && right_type == type_string) return type_string;
        if ((left_type == type_int && right_type == type_double) ||
            (left_type == type_double && right_type == type_int) ||
            (left_type == type_double && right_type == type_double)) {
            return type_double;
        }
        fprintf(stderr, "Semantic Error (6): Incompatible types for arithmetic operation.\n");
        exit(6);
    }
    
    if (op == TOKEN_LT || op == TOKEN_LEQ || op == TOKEN_GT || op == TOKEN_GEQ || 
        op == TOKEN_EQ || op == TOKEN_NEQ) {
        
        bool are_numeric = (left_type == type_int || left_type == type_double) &&
                           (right_type == type_int || right_type == type_double);
        bool are_strings = (left_type == type_string && right_type == type_string);
        bool are_same = (left_type == right_type);

        if (are_numeric || are_strings || (are_same && (op == TOKEN_EQ || op == TOKEN_NEQ))) {
            return type_bool; // Návratová hodnota relace (typ Boolean z tabulky symbolů)
        }
        fprintf(stderr, "Semantic Error (6): Incompatible types for relational operation.\n");
        exit(6);
    }

    fprintf(stderr, "Internal Error (99): Unknown operator in semantic analysis.\n");
    exit(99);
}