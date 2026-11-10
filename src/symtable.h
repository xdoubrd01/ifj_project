/**
 * @file symtable.h
 * @brief Symbol records and the scoped symbol table interface.
 *
 * The table owns inserted records, copied names, and copied parameter arrays.
 * Search functions return borrowed records; callers must not free them or
 * replace their owned parameter arrays.
 *
 * Variable records expire when their scope is left.
 * Function records expire when the symbol table is destroyed.
 */

#ifndef SYMTABLE_H
#define SYMTABLE_H

#include <stdbool.h>
#include <stddef.h>

/** @brief Type metadata, including internal and unresolved types. */
typedef enum {
    type_int,
    type_double,
    type_string,
    type_unit,
    type_bool,   /**< Internal Boolean type; explicit source types require BOOLTHEN. */
    type_any,    /**< Wildcard used for expected builtin parameter types. */
    type_unknown /**< Unresolved type metadata. */
} data_type;

/** @brief Kind of symbol; parameters use sym_var. */
typedef enum { sym_var, sym_func } symbol_type;

/**
 * @brief Symbol record owned by the symbol table.
 *
 * The type member selects the active union member.
 * Function parameter arrays remain owned by the symbol table,
 * including when accessed through a returned record.
 */
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
        } func;
    } data;
} symbol_data;

/** @brief Opaque symbol table type. */
typedef struct SymTable SymTable;

/* Creation and destruction. */

/**
 * @brief Creates a symbol table with an empty global function table.
 *
 * @param initial_capacity Initial capacity per hash table; 0 selects the default.
 * @return A new symbol table, or NULL on failure.
 *
 * @note No local scope or builtin functions are created automatically.
 */
SymTable *symtable_create(size_t initial_capacity);

/**
 * @brief Destroys all scopes, functions, and owned symbol data.
 *
 * @param table Symbol table to destroy; NULL is accepted.
 *
 * @warning All borrowed symbol pointers become invalid.
 */
void symtable_destroy(SymTable *table);

/* Scope management. */

/**
 * @brief Creates an empty local scope nested inside the current one.
 *
 * @param table Symbol table to update.
 * @return true on success; false for NULL or allocation failure.
 *
 * @note On failure, the active scope and depth remain unchanged.
 */
bool symtable_enter_scope(SymTable *table);

/**
 * @brief Removes the current scope and restores its parent.
 *
 * @param table Symbol table to update.
 * @return true on success; false for NULL or if no local scope exists.
 *
 * @warning Pointers to symbols owned by the removed scope become invalid.
 */
bool symtable_leave_scope(SymTable *table);

/**
 * @brief Returns the number of active local scopes.
 *
 * @param table Symbol table to inspect.
 * @return Current depth, or 0 for NULL.
 */
size_t symtable_scope_depth(SymTable *table);

/* Symbol lookup. */

/**
 * @brief Finds a variable or parameter in the current scope only.
 *
 * @param table Symbol table to search.
 * @param key Null-terminated symbol name.
 * @return A borrowed record, or NULL if absent, invalid, or no scope exists.
 */
symbol_data *symtable_search_current_var(SymTable *table, const char *key);

/**
 * @brief Finds the nearest visible variable or parameter.
 *
 * @param table Symbol table to search.
 * @param key Null-terminated symbol name.
 * @return A borrowed record, or NULL if absent or an argument is NULL.
 *
 * @note Searches from the current scope outward, respecting shadowing.
 */
symbol_data *symtable_search_var(SymTable *table, const char *key);

/**
 * @brief Finds a function in the global function table.
 *
 * @param table Symbol table to search.
 * @param key Null-terminated function name.
 * @return A borrowed record, or NULL if absent or an argument is NULL.
 */
symbol_data *symtable_search_func(SymTable *table, const char *key);

/* Symbol registration. */

/**
 * @brief Registers a variable in the current local scope.
 *
 * @param table Target symbol table with an active scope.
 * @param key Null-terminated variable name; copied on success.
 * @param type Variable type metadata.
 * @param is_mutable Whether assignment to the variable is permitted.
 * @return true on success; false for invalid input, no active scope,
 * a duplicate in the current scope, or allocation failure.
 *
 * @note Outer declarations may be shadowed.
 * Contextual type checks belong to the parser or semantic analyzer.
 */
bool symtable_insert_var(SymTable *table, const char *key, data_type type, bool is_mutable);

/**
 * @brief Registers an immutable parameter in the current local scope.
 *
 * @param table Target symbol table with an active scope.
 * @param key Null-terminated parameter name; copied on success.
 * @param type Parameter type metadata.
 * @return true on success; false for invalid input, no active scope,
 * a duplicate in the current scope, or allocation failure.
 */
bool symtable_insert_param(SymTable *table, const char *key, data_type type);

/**
 * @brief Registers a global function and copies its parameter types.
 *
 * @param table Target symbol table.
 * @param key Null-terminated function name; copied on success.
 * @param ret_type Stored return type metadata.
 * @param param_count Number of parameter types.
 * @param params Array of param_count types; may be NULL when the count is zero.
 * @return true on success; false for invalid input, a duplicate name,
 * an unrepresentable array size, or allocation failure.
 *
 * @note The caller retains ownership of the input array.
 * Function names must be unique; overloading is not supported.
 */
bool symtable_insert_func(SymTable *table, const char *key, data_type ret_type, size_t param_count,
                          const data_type *params);

/**
 * @brief Registers the ten builtin functions of the base IFJ26 language.
 *
 * @param table Target symbol table.
 * @return true if all functions were inserted; false otherwise.
 *
 * @warning Insertion is not atomic. On failure, functions inserted earlier
 * in this call remain present. The table can still be safely destroyed.
 * @note Calling again after successful insertion fails due to duplicate names.
 */
bool symtable_insert_builtins(SymTable *table);

/* Type metadata and compatibility. */

/**
 * @brief Updates the stored return type of an existing function.
 *
 * @param table Symbol table containing the function.
 * @param key Null-terminated function name.
 * @param ret_type New return type metadata.
 * @return true if updated; false if the function cannot be found.
 *
 * @note The parser or semantic analyzer decides whether an update is permitted.
 */
bool symtable_set_func_return_type(SymTable *table, const char *key, data_type ret_type);

/**
 * @brief Checks compatibility of stored type metadata.
 *
 * @param expected Required type, possibly the Any wildcard.
 * @param actual Actual type metadata.
 * @return false if either type is unknown. Otherwise returns true for
 * expected Any, equal types, or implicit Int-to-Double widening.
 *
 * @note This helper does not validate whether a type is legal
 * in a particular source-language context.
 */
bool symtable_type_compatible(data_type expected, data_type actual);

#endif // SYMTABLE_H