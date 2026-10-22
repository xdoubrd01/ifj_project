#include "symtable.h"
#include "hashtable.h"

#include <stdint.h>
#include <stdlib.h>

#define DEFAULT_SYMTABLE_CAPACITY 32U

typedef struct Scope {
    hashtable     table;
    struct Scope *parent;
} Scope;

struct SymTable {
    hashtable functions;
    Scope    *current_scope;
    size_t    initial_capacity;
    size_t    scope_depth;
};

static size_t normalized_capacity(size_t capacity) {
    return capacity == 0 ? DEFAULT_SYMTABLE_CAPACITY : capacity;
}

static void free_symbol_table_data(hashtable *table) {
    if (table == NULL || table->items == NULL) {
        return;
    }

    for (size_t i = 0; i < table->capacity; i++) {
        if (table->items[i].state != full) {
            continue;
        }

        symbol_data *symbol = table->items[i].data;
        if (symbol == NULL) {
            continue;
        }

        if (symbol->type == sym_func) {
            free(symbol->data.func.params);
            symbol->data.func.params      = NULL;
            symbol->data.func.param_count = 0;
        }

        free(symbol);
        table->items[i].data = NULL;
    }
}

SymTable *symtable_create(size_t initial_capacity) {
    SymTable *table = malloc(sizeof(*table));
    if (table == NULL) {
        return NULL;
    }

    table->current_scope    = NULL;
    table->scope_depth      = 0;
    table->initial_capacity = normalized_capacity(initial_capacity);

    table->functions.items    = NULL;
    table->functions.size     = 0;
    table->functions.capacity = 0;
    table->functions.used     = 0;

    ht_init(&table->functions, table->initial_capacity);
    if (table->functions.items == NULL || table->functions.capacity == 0) {
        free(table);
        return NULL;
    }

    return table;
}

void symtable_destroy(SymTable *table) {
    if (table == NULL) {
        return;
    }

    while (table->current_scope != NULL) {
        symtable_leave_scope(table);
    }

    free_symbol_table_data(&table->functions);
    ht_free(&table->functions);
    free(table);
}

bool symtable_enter_scope(SymTable *table) {
    if (table == NULL) {
        return false;
    }

    Scope *scope = malloc(sizeof(*scope));
    if (scope == NULL) {
        return false;
    }

    scope->table.items    = NULL;
    scope->table.size     = 0;
    scope->table.capacity = 0;
    scope->table.used     = 0;

    ht_init(&scope->table, table->initial_capacity);
    if (scope->table.items == NULL || scope->table.capacity == 0) {
        free(scope);
        return false;
    }

    scope->parent        = table->current_scope;
    table->current_scope = scope;
    table->scope_depth++;

    return true;
}

bool symtable_leave_scope(SymTable *table) {
    if (table == NULL || table->current_scope == NULL) {
        return false;
    }

    Scope *scope         = table->current_scope;
    table->current_scope = scope->parent;

    free_symbol_table_data(&scope->table);
    ht_free(&scope->table);
    free(scope);

    if (table->scope_depth > 0) {
        table->scope_depth--;
    }

    return true;
}

size_t symtable_scope_depth(SymTable *table) { return table == NULL ? 0 : table->scope_depth; }

symbol_data *symtable_search_current_var(SymTable *table, const char *key) {
    if (table == NULL || table->current_scope == NULL || key == NULL) {
        return NULL;
    }

    symbol_data *symbol = ht_search(&table->current_scope->table, key);
    if (symbol == NULL || symbol->type != sym_var) {
        return NULL;
    }

    return symbol;
}

symbol_data *symtable_search_var(SymTable *table, const char *key) {
    if (table == NULL || key == NULL) {
        return NULL;
    }

    for (Scope *scope = table->current_scope; scope != NULL; scope = scope->parent) {
        symbol_data *symbol = ht_search(&scope->table, key);

        if (symbol != NULL && symbol->type == sym_var) {
            return symbol;
        }
    }

    return NULL;
}

symbol_data *symtable_search_func(SymTable *table, const char *key) {
    if (table == NULL || key == NULL) {
        return NULL;
    }

    symbol_data *symbol = ht_search(&table->functions, key);
    if (symbol == NULL || symbol->type != sym_func) {
        return NULL;
    }

    return symbol;
}

bool symtable_insert_var(SymTable *table, const char *key, data_type type, bool is_mutable) {
    if (table == NULL || table->current_scope == NULL || key == NULL) {
        return false;
    }

    if (symtable_search_current_var(table, key) != NULL) {
        return false;
    }

    symbol_data *symbol = malloc(sizeof(*symbol));
    if (symbol == NULL) {
        return false;
    }

    symbol->type                  = sym_var;
    symbol->data.var.var_type     = type;
    symbol->data.var.is_mutable   = is_mutable;
    symbol->data.var.is_parameter = false;

    if (!ht_insert(&table->current_scope->table, key, symbol)) {
        free(symbol);
        return false;
    }

    return true;
}

bool symtable_insert_param(SymTable *table, const char *key, data_type type) {
    if (table == NULL || table->current_scope == NULL || key == NULL) {
        return false;
    }

    if (symtable_search_current_var(table, key) != NULL) {
        return false;
    }

    symbol_data *symbol = malloc(sizeof(*symbol));
    if (symbol == NULL) {
        return false;
    }

    symbol->type                  = sym_var;
    symbol->data.var.var_type     = type;
    symbol->data.var.is_mutable   = false;
    symbol->data.var.is_parameter = true;

    if (!ht_insert(&table->current_scope->table, key, symbol)) {
        free(symbol);
        return false;
    }

    return true;
}

bool symtable_insert_func(SymTable *table, const char *key, data_type ret_type, size_t param_count,
                          const data_type *params) {
    if (table == NULL || key == NULL || (param_count > 0 && params == NULL)) {
        return false;
    }

    if (symtable_search_func(table, key) != NULL) {
        return false;
    }

    symbol_data *symbol = malloc(sizeof(*symbol));
    if (symbol == NULL) {
        return false;
    }

    symbol->type                  = sym_func;
    symbol->data.func.ret_type    = ret_type;
    symbol->data.func.param_count = param_count;
    symbol->data.func.params      = NULL;

    if (param_count > 0) {
        if (param_count > SIZE_MAX / sizeof(*symbol->data.func.params)) {
            free(symbol);
            return false;
        }

        symbol->data.func.params = malloc(param_count * sizeof(*symbol->data.func.params));
        if (symbol->data.func.params == NULL) {
            free(symbol);
            return false;
        }

        for (size_t i = 0; i < param_count; i++) {
            symbol->data.func.params[i] = params[i];
        }
    }

    if (!ht_insert(&table->functions, key, symbol)) {
        free(symbol->data.func.params);
        free(symbol);
        return false;
    }

    return true;
}

bool symtable_set_func_return_type(SymTable *table, const char *key, data_type ret_type) {
    symbol_data *symbol = symtable_search_func(table, key);
    if (symbol == NULL) {
        return false;
    }

    symbol->data.func.ret_type = ret_type;
    return true;
}

bool symtable_type_compatible(data_type expected, data_type actual) {
    if (expected == type_any) {
        return true;
    }

    if (expected == actual) {
        return true;
    }

    return expected == type_double && actual == type_int;
}

bool symtable_insert_builtins(SymTable *table) {
    if (table == NULL) {
        return false;
    }

    const data_type any_param[]             = {type_any};
    const data_type double_param[]          = {type_double};
    const data_type string_param[]          = {type_string};
    const data_type string_int_int_params[] = {type_string, type_int, type_int};
    const data_type string_int_params[]     = {type_string, type_int};
    const data_type int_param[]             = {type_int};

    if (!symtable_insert_func(table, "print", type_unit, 1, any_param)) {
        return false;
    }
    if (!symtable_insert_func(table, "readLine", type_string, 0, NULL)) {
        return false;
    }
    if (!symtable_insert_func(table, "readInt", type_int, 0, NULL)) {
        return false;
    }
    if (!symtable_insert_func(table, "readDouble", type_double, 0, NULL)) {
        return false;
    }
    if (!symtable_insert_func(table, "toStr", type_string, 1, any_param)) {
        return false;
    }
    if (!symtable_insert_func(table, "toInt", type_int, 1, double_param)) {
        return false;
    }
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