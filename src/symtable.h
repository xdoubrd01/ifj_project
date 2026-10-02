#ifndef SYMTABLE_H
#define SYMTABLE_H

#include "hashtable.h"
#include <stdbool.h>

typedef enum { type_int, type_double, type_string, type_unit } data_type;

typedef enum { sym_var, sym_func } symbol_type;

// Optimized specific data for a symbol
typedef struct {
    symbol_type type; // Determines the symbol type (variable vs. function)

    union {
        // Variable attributes
        struct {
            data_type var_type;
            bool      is_mutable;
        } var;

        // Function attributes
        struct {
            data_type  ret_type;
            int        param_count;
            data_type *params;
        } func;
    } data;
} symbol_data;

// Symbol table interface for the rest of the compiler
void symtable_init(hashtable *ts, size_t initial_capacity);
bool symtable_insert_var(hashtable *ts, const char *key, data_type type, bool is_mutable);
bool symtable_insert_func(hashtable *ts, const char *key, data_type ret_type, int param_count,
                          data_type *params);
symbol_data *symtable_search(hashtable *ts, const char *key);
void         symtable_free(hashtable *ts);

#endif // SYMTABLE_H