/**
 * @file symtable.c
 * @brief Scoped symbol table built on the public hash table API.
 *
 * Public contracts and ownership rules are documented in symtable.h.
 */

#include "symtable.h"
#include "hashtable.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/** @brief Default capacity for each underlying hash table. */
#define DEFAULT_SYMTABLE_CAPACITY 32U

/**
 * @brief Local variable scope linked to its enclosing scope.
 *
 * Each scope owns one hash table containing its variables and parameters.
 * The parent pointer links to the enclosing scope without owning it.
 */
typedef struct Scope {
    hashtable    *table;
    struct Scope *parent;
} Scope;

/**
 * @brief Private symbol table state.
 *
 * Functions are stored globally, independently of local scopes.
 * Local scopes form a stack through their parent pointers.
 */
struct SymTable {
    hashtable *functions;
    Scope     *current_scope;
    size_t     initial_capacity;
    size_t     scope_depth;
    unsigned   next_var_id; /**< Id assigned to the next variable or parameter. */
};

/* Internal helpers. */

/**
 * @brief Replaces a zero capacity request with the module default.
 */
static size_t normalized_capacity(size_t capacity) {
    /* A zero request selects the default rather than an empty allocation. */
    return capacity == 0 ? DEFAULT_SYMTABLE_CAPACITY : capacity;
}

/**
 * @brief Releases an owned parameter-name array and its initialized strings.
 * @param names Array to release; NULL is accepted.
 * @param count Number of initialized entries to release.
 */
static void free_param_names(char **names, size_t count) {
    if (names == NULL) {
        return;
    }

    for (size_t i = 0; i < count; ++i) {
        free(names[i]);
    }

    free(names);
}

/**
 * @brief Releases one symbol record, its parameter types, and name copies.
 *
 * @param data Symbol record; NULL is accepted.
 */
static void free_symbol(void *data) {
    /* Recover the concrete record type from the generic callback argument. */
    symbol_data *symbol = data;

    if (symbol == NULL) {
        return;
    }

    /* Function records own parameter types and any assigned name copies. */
    if (symbol->type == sym_func) {
        free(symbol->data.func.params);
        free_param_names(symbol->data.func.param_names, symbol->data.func.param_count);
    }

    /*
     * Variable records have no additional dynamically allocated payload.
     * Function parameter storage has already been released above.
     */
    free(symbol);
}

/**
 * @brief Releases symbol values without destroying their hash table.
 *
 * @param table Table whose values are to be released; NULL is accepted.
 *
 * @warning Use only during disposal and follow with ht_free().
 */
static void free_symbol_table_data(hashtable *table) {
    if (table == NULL) {
        return;
    }

    /*
     * Access values through the public hash table API.
     * The callback releases records, while keys remain owned by the hash table.
     *
     * Afterwards, the cells still contain pointers to released records.
     * The caller must destroy the hash table before using those values again.
     */
    ht_foreach(table, free_symbol);
}

/* Creation and destruction. */

SymTable *symtable_create(size_t initial_capacity) {
    /* Allocate the outer object before creating any owned hash tables. */
    SymTable *table = malloc(sizeof(*table));
    if (table == NULL) {
        return NULL;
    }

    /*
     * No local scope exists initially.
     * Store the normalized capacity for both functions and future local scopes.
     */
    table->current_scope    = NULL;
    table->scope_depth      = 0;
    table->next_var_id      = 0;
    table->initial_capacity = normalized_capacity(initial_capacity);

    /* Function symbols are stored separately from local variables. */
    table->functions = ht_create(table->initial_capacity);

    if (table->functions == NULL) {
        /* Release the outer object if its hash table could not be created. */
        free(table);
        return NULL;
    }

    /*
     * The object is ready for use.
     * Opening a local scope and registering builtins are explicit operations.
     */
    return table;
}

void symtable_destroy(SymTable *table) {
    if (table == NULL) {
        return;
    }

    /*
     * Pop all local scopes before destroying the global function table.
     * Each iteration releases one scope and restores its parent.
     */
    while (table->current_scope != NULL) {
        symtable_leave_scope(table);
    }

    /*
     * Free function records and their parameter arrays first.
     * Then free the hash table, which owns the copied keys and cell array.
     */
    free_symbol_table_data(table->functions);
    ht_free(table->functions);

    /* No owned scopes or function records remain at this point. */
    free(table);
}

/* Scope management. */

bool symtable_enter_scope(SymTable *table) {
    if (table == NULL) {
        return false;
    }

    /* Keep the new scope detached until all its allocations succeed. */
    Scope *scope = malloc(sizeof(*scope));
    if (scope == NULL) {
        return false;
    }

    /* Each scope receives its own empty hash table. */
    scope->table = ht_create(table->initial_capacity);
    if (scope->table == NULL) {
        /*
         * Only the new scope object needs cleanup.
         * The active scope chain has not been modified.
         */
        free(scope);
        return false;
    }

    /*
     * Link the fully initialized scope to its enclosing scope.
     * Make it current only after all required allocations have succeeded.
     */
    scope->parent        = table->current_scope;
    table->current_scope = scope;
    table->scope_depth++;

    return true;
}

bool symtable_leave_scope(SymTable *table) {
    if (table == NULL || table->current_scope == NULL) {
        return false;
    }

    /*
     * Save the scope being removed and restore its parent first.
     * The parent belongs to the remaining scope chain.
     */
    Scope *scope         = table->current_scope;
    table->current_scope = scope->parent;

    /*
     * Release records before destroying the hash table.
     * The hash table releases its copied keys, but not the symbol values.
     */
    free_symbol_table_data(scope->table);
    ht_free(scope->table);

    /* Release the scope object after all resources it owns are gone. */
    free(scope);

    /* Keep the depth consistent with the remaining scope chain. */
    if (table->scope_depth > 0) {
        table->scope_depth--;
    }

    return true;
}

size_t symtable_scope_depth(SymTable *table) {
    /* NULL and a table without an active local scope both report depth zero. */
    return table == NULL ? 0 : table->scope_depth;
}

/* Symbol lookup. */

symbol_data *symtable_search_current_var(SymTable *table, const char *key) {
    if (table == NULL || table->current_scope == NULL || key == NULL) {
        return NULL;
    }

    /*
     * Restrict lookup to the current scope.
     * An outer declaration does not count as a duplicate in this scope.
     */
    symbol_data *symbol = ht_search(table->current_scope->table, key);
    if (symbol == NULL || symbol->type != sym_var) {
        return NULL;
    }

    /* Return a borrowed record without allocating or copying it. */
    return symbol;
}

symbol_data *symtable_search_var(SymTable *table, const char *key) {
    if (table == NULL || key == NULL) {
        return NULL;
    }

    /* Search from the innermost scope outward to implement shadowing. */
    for (Scope *scope = table->current_scope; scope != NULL; scope = scope->parent) {
        symbol_data *symbol = ht_search(scope->table, key);

        /* The first matching variable hides declarations in outer scopes. */
        if (symbol != NULL && symbol->type == sym_var) {
            return symbol;
        }
    }

    /* No active scope contains a matching variable or parameter. */
    return NULL;
}

symbol_data *symtable_search_func(SymTable *table, const char *key) {
    if (table == NULL || key == NULL) {
        return NULL;
    }

    /*
     * Functions are resolved through the global function table.
     * Local variable scopes are not searched here.
     */
    symbol_data *symbol = ht_search(table->functions, key);
    if (symbol == NULL || symbol->type != sym_func) {
        return NULL;
    }

    /* The symbol table retains ownership of the returned record. */
    return symbol;
}

/* Symbol registration. */

bool symtable_insert_var(SymTable *table, const char *key, data_type type, bool is_mutable) {
    if (table == NULL || table->current_scope == NULL || key == NULL) {
        return false;
    }

    /* Reject duplicates here while allowing names from outer scopes to be hidden. */
    if (symtable_search_current_var(table, key) != NULL) {
        return false;
    }

    /*
     * Allocate a separate record.
     * Hash table resizing can then move entries without moving this record.
     */
    symbol_data *symbol = malloc(sizeof(*symbol));
    if (symbol == NULL) {
        return false;
    }

    /*
     * Store the supplied type and mutability metadata.
     * Contextual source-language checks are performed by the parser.
     */
    symbol->type                  = sym_var;
    symbol->data.var.var_type     = type;
    symbol->data.var.is_mutable   = is_mutable;
    symbol->data.var.is_parameter = false;
    symbol->data.var.id           = table->next_var_id;

    /* Release the new record if insertion cannot take ownership of it. */
    if (!ht_insert(table->current_scope->table, key, symbol)) {
        free(symbol);
        return false;
    }

    /*
     * Ids are never reused, so equal names from different scopes stay distinct.
     * The symbol table owns the record; the hash table owns the copied key.
     */
    table->next_var_id++;
    return true;
}

bool symtable_insert_param(SymTable *table, const char *key, data_type type) {
    if (table == NULL || table->current_scope == NULL || key == NULL) {
        return false;
    }

    /* Parameters and ordinary variables share the current scope namespace. */
    if (symtable_search_current_var(table, key) != NULL) {
        return false;
    }

    symbol_data *symbol = malloc(sizeof(*symbol));
    if (symbol == NULL) {
        return false;
    }

    /*
     * A parameter is represented as a variable with additional metadata.
     * Parameters are immutable regardless of how ordinary variables are handled.
     */
    symbol->type                  = sym_var;
    symbol->data.var.var_type     = type;
    symbol->data.var.is_mutable   = false;
    symbol->data.var.is_parameter = true;
    symbol->data.var.id           = table->next_var_id;

    /* Failed insertion leaves cleanup of the new record to this function. */
    if (!ht_insert(table->current_scope->table, key, symbol)) {
        free(symbol);
        return false;
    }

    /* Parameters share the id sequence with ordinary variables. */
    table->next_var_id++;
    return true;
}

bool symtable_insert_func(SymTable *table, const char *key, data_type ret_type, size_t param_count,
                          const data_type *params) {
    if (table == NULL || key == NULL || (param_count > 0 && params == NULL)) {
        return false;
    }

    /* Function names must be unique because overloading is not supported. */
    if (symtable_search_func(table, key) != NULL) {
        return false;
    }

    symbol_data *symbol = malloc(sizeof(*symbol));
    if (symbol == NULL) {
        return false;
    }

    /*
     * Start with no parameter array.
     * A zero-parameter function therefore requires no additional allocation.
     */
    symbol->type                  = sym_func;
    symbol->data.func.ret_type    = ret_type;
    symbol->data.func.param_count = param_count;
    symbol->data.func.params      = NULL;
    symbol->data.func.param_names = NULL;

    if (param_count > 0) {
        /* Guard the byte-count multiplication before allocating the array. */
        if (param_count > SIZE_MAX / sizeof(*symbol->data.func.params)) {
            free(symbol);
            return false;
        }

        symbol->data.func.params = malloc(param_count * sizeof(*symbol->data.func.params));
        if (symbol->data.func.params == NULL) {
            free(symbol);
            return false;
        }

        /*
         * Copy the signature instead of borrowing the input array.
         * The caller may release or modify its original array afterwards.
         */
        for (size_t i = 0; i < param_count; i++) {
            symbol->data.func.params[i] = params[i];
        }
    }

    /* Neither allocation has been transferred to the table if insertion fails. */
    if (!ht_insert(table->functions, key, symbol)) {
        free(symbol->data.func.params);
        free(symbol);
        return false;
    }

    /* The table owns both the record and its optional parameter-type array. */
    return true;
}

bool symtable_set_func_param_names(SymTable *table, const char *key, size_t param_count,
                                   const char *const *param_names) {
    symbol_data *symbol = symtable_search_func(table, key);

    if (symbol == NULL || symbol->data.func.param_count != param_count ||
        (param_count > 0 && param_names == NULL)) {
        return false;
    }

    // Zero-parameter functions need no name storage.
    if (param_count == 0) {
        return true;
    }

    if (param_count > SIZE_MAX / sizeof(char *)) {
        return false;
    }

    // Validate all supplied entries before allocating any replacement storage.
    for (size_t i = 0; i < param_count; ++i) {
        if (param_names[i] == NULL || param_names[i][0] == '\0') {
            return false;
        }
    }

    char **copies = malloc(param_count * sizeof(*copies));
    if (copies == NULL) {
        return false;
    }

    for (size_t i = 0; i < param_count; ++i) {
        size_t length = strlen(param_names[i]);

        if (length == SIZE_MAX) {
            free_param_names(copies, i);
            return false;
        }

        copies[i] = malloc(length + 1);
        if (copies[i] == NULL) {
            // Only entries before i have been initialized successfully.
            free_param_names(copies, i);
            return false;
        }

        memcpy(copies[i], param_names[i], length + 1);
    }

    // Publish only the complete replacement; inputs may alias the old names.
    free_param_names(symbol->data.func.param_names, param_count);
    symbol->data.func.param_names = copies;

    return true;
}

bool symtable_insert_builtins(SymTable *table) {
    if (table == NULL) {
        return false;
    }

    /*
     * Describe builtin parameter types using temporary arrays.
     * symtable_insert_func copies them before this function returns.
     */
    const data_type any_param[]             = {type_any};
    const data_type double_param[]          = {type_double};
    const data_type string_param[]          = {type_string};
    const data_type string_int_int_params[] = {type_string, type_int, type_int};
    const data_type string_int_params[]     = {type_string, type_int};
    const data_type int_param[]             = {type_int};

    /*
     * Stop at the first failure.
     * Functions inserted earlier in this call are not rolled back.
     */
    if (!symtable_insert_func(table, "print", type_unit, 1, any_param)) {
        return false;
    }

    /* Input functions take no arguments. */
    if (!symtable_insert_func(table, "readLine", type_string, 0, NULL)) {
        return false;
    }
    if (!symtable_insert_func(table, "readInt", type_int, 0, NULL)) {
        return false;
    }
    if (!symtable_insert_func(table, "readDouble", type_double, 0, NULL)) {
        return false;
    }

    /* Conversion functions use their declared argument types. */
    if (!symtable_insert_func(table, "toStr", type_string, 1, any_param)) {
        return false;
    }
    if (!symtable_insert_func(table, "toInt", type_int, 1, double_param)) {
        return false;
    }

    /* Register string operations and character-code conversions. */
    if (!symtable_insert_func(table, "strLength", type_int, 1, string_param)) {
        return false;
    }
    if (!symtable_insert_func(table, "subStr", type_string, 3, string_int_int_params)) {
        return false;
    }
    if (!symtable_insert_func(table, "ord", type_int, 2, string_int_params)) {
        return false;
    }
    if (!symtable_insert_func(table, "chr", type_string, 1, int_param)) {
        return false;
    }

    return true;
}

/* Type metadata and compatibility. */

bool symtable_set_func_return_type(SymTable *table, const char *key, data_type ret_type) {
    /* Update an existing declaration rather than creating another function. */
    symbol_data *symbol = symtable_search_func(table, key);
    if (symbol == NULL) {
        return false;
    }

    /* The parser decides when updating the stored return type is permitted. */
    symbol->data.func.ret_type = ret_type;
    return true;
}

bool symtable_type_compatible(data_type expected, data_type actual) {
    /* An unresolved type cannot establish compatibility, even with itself. */
    if (expected == type_unknown || actual == type_unknown) {
        return false;
    }

    /* After unresolved types are rejected, expected Any accepts the actual type. */
    if (expected == type_any) {
        return true;
    }

    /* Matching resolved types require no implicit conversion. */
    if (expected == actual) {
        return true;
    }

    /*
     * Int-to-Double widening is the only implicit conversion.
     * This function reports compatibility; code generation performs conversion.
     */
    return expected == type_double && actual == type_int;
}