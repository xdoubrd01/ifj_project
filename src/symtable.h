#ifndef SYMTABLE_H
#define SYMTABLE_H

#include <stdbool.h>
#include <stddef.h>

typedef enum {
    type_int,
    type_double,
    type_string,
    type_unit,
    type_bool,
    type_any,
    type_unknown
} data_type;

typedef enum {
    sym_var,
    sym_func
} symbol_type;

typedef struct {
    symbol_type type;

    union {
        struct {
            data_type var_type;
            bool is_mutable;
            bool is_parameter;
        } var;

        struct {
            data_type ret_type;
            size_t param_count;
            data_type *params;
        } func;
    } data;
} symbol_data;

/* Opaque symbol-table ADT. Its hashtable representation is hidden from users. */
typedef struct SymTable SymTable;

/* Creates the symbol table. A value of 0 lets the module choose a default capacity. */
SymTable *symtable_create(size_t initial_capacity);
void symtable_destroy(SymTable *table);

/* Block-scope management for local variables and function parameters. */
bool symtable_enter_scope(SymTable *table);
bool symtable_leave_scope(SymTable *table);
size_t symtable_scope_depth(SymTable *table);

/* Variables are inserted into the current local scope. */
bool symtable_insert_var(SymTable *table, const char *key, data_type type, bool is_mutable);
bool symtable_insert_param(SymTable *table, const char *key, data_type type);

/* Functions are stored globally and are independent of local variable scopes. */
bool symtable_insert_func(SymTable *table, const char *key, data_type ret_type,
                          size_t param_count, const data_type *params);
bool symtable_set_func_return_type(SymTable *table, const char *key, data_type ret_type);

/* Registers the built-in functions required by the basic IFJ26 specification. */
bool symtable_insert_builtins(SymTable *table);

/* Basic type compatibility used by semantic analysis (including Int -> Double). */
bool symtable_type_compatible(data_type expected, data_type actual);

/* Lookup operations. */
symbol_data *symtable_search_var(SymTable *table, const char *key);
symbol_data *symtable_search_current_var(SymTable *table, const char *key);
symbol_data *symtable_search_func(SymTable *table, const char *key);

#endif // SYMTABLE_H
