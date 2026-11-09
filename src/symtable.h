#ifndef SYMTABLE_H
#define SYMTABLE_H

#include <stdbool.h>
#include <stddef.h>

typedef enum {
    type_int,
    type_double,
    type_string,
    type_unit,

    /* Internal condition type, not a base-language value type. */
    type_bool,

    /* Builtin parameter wildcard only. */
    type_any,

    /* Unresolved inferred function result only. */
    type_unknown
} data_type;

typedef enum { sym_var, sym_func } symbol_type;

typedef enum {
    SYMTABLE_OK             = 0,
    SYMTABLE_UNDEFINED      = 3,
    SYMTABLE_REDEFINITION   = 4,
    SYMTABLE_ARGUMENT_ERROR = 5,
    SYMTABLE_TYPE_ERROR     = 6,
    SYMTABLE_IMMUTABLE      = 7,
    SYMTABLE_INTERNAL_ERROR = 99
} symtable_status;

typedef struct {
    symbol_type type;

    union {
        struct {
            data_type var_type;
            bool      is_mutable;
            bool      is_parameter;
        } var;

        struct {
            data_type  ret_type;
            size_t     param_count;
            data_type *params;
            bool       return_type_explicit;
            bool       is_builtin;
            bool       is_main;
        } func;
    } data;
} symbol_data;

typedef struct SymTable SymTable;

/*
 * No local scope is created automatically.
 * Builtins are inserted explicitly.
 * Zero capacity selects a default.
 * Returns NULL on allocation failure.
 */
SymTable *symtable_create(size_t initial_capacity);
void      symtable_destroy(SymTable *table);

/*
 * Status of the last mutation/check.
 * Pure lookups do not change the status.
 * NULL returns SYMTABLE_INTERNAL_ERROR.
 */
symtable_status symtable_last_error(const SymTable *table);

bool   symtable_enter_scope(SymTable *table);
bool   symtable_leave_scope(SymTable *table);
size_t symtable_scope_depth(SymTable *table);

/*
 * Insert a variable after checking its initializer.
 * This lets a shadowing initializer refer to the outer variable.
 * Variable and parameter types must be Int, Double, String, or Unit.
 */
bool symtable_insert_var(SymTable *table, const char *key, data_type type, bool is_mutable);

bool symtable_insert_param(SymTable *table, const char *key, data_type type);

/*
 * A concrete return type represents an explicit annotation.
 * type_unknown represents an omitted annotation and pending inference.
 *
 * Pre-register function headers to support forward calls.
 * The parser must reject direct/indirect recursion without explicit
 * return annotations.
 */
bool symtable_insert_func(SymTable *table, const char *key, data_type ret_type, size_t param_count,
                          const data_type *params);

/* Resolves unknown; cannot change an already resolved return type. */
bool symtable_set_func_return_type(SymTable *table, const char *key, data_type ret_type);

/*
 * Borrowed records: do not free them or modify ownership/type metadata.
 * Variable pointers expire when their scope is left.
 * Function pointers expire when the symbol table is destroyed.
 * Insertion/rehashing does not invalidate record pointers.
 */
symbol_data *symtable_search_var(SymTable *table, const char *key);
symbol_data *symtable_search_current_var(SymTable *table, const char *key);
symbol_data *symtable_search_func(SymTable *table, const char *key);

/*
 * Unknown is never compatible, including with itself or Any.
 * The only implicit conversion is Int -> Double.
 * Boolean remains an internal expression type.
 */
bool symtable_type_compatible(data_type expected, data_type actual);

/*
 * Inserts the ten base-language builtins.
 * All or none are inserted on failure.
 * A second insertion is a redefinition error.
 */
bool symtable_insert_builtins(SymTable *table);

/* Semantic helpers set last_error to the corresponding IFJ26 code. */
bool symtable_check_call(SymTable *table, const char *key, size_t arg_count,
                         const data_type *arg_types);

bool symtable_check_assignment(SymTable *table, const char *key, data_type actual);

/*
 * Checks an explicit return statement, not the last body expression.
 * has_value == false means bare return; actual is then ignored.
 * An explicit Unit return type discards a checked base-language value.
 */
bool symtable_check_return(SymTable *table, const char *key, data_type actual, bool has_value);

/*
 * @main may have any identifier.
 * Mark while reading headers, validate after return-type inference.
 * mark temporarily permits an unresolved result type.
 */
bool symtable_mark_main(SymTable *table, const char *key);
bool symtable_validate_main(SymTable *table);

#endif