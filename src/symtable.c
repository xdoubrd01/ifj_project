#include "symtable.h"
#include "hashtable.h"

#include <stdlib.h>

#define DEFAULT_SYMTABLE_CAPACITY 32U

typedef struct Scope {
    hashtable table;
    struct Scope *parent;
} Scope;

typedef struct FunctionMetaNode {
    symbol_data *symbol;
    struct FunctionMetaNode *next;
} FunctionMetaNode;

struct SymTable {
    hashtable functions;
    Scope *current_scope;
    FunctionMetaNode *function_meta;
    size_t initial_capacity;
    size_t scope_depth;
};

static size_t normalized_capacity(size_t capacity) {
    return capacity == 0 ? DEFAULT_SYMTABLE_CAPACITY : capacity;
}

static void free_function_parameter_arrays(FunctionMetaNode *node) {
    while (node != NULL) {
        FunctionMetaNode *next = node->next;
        if (node->symbol != NULL && node->symbol->type == sym_func) {
            free(node->symbol->data.func.params);
            node->symbol->data.func.params = NULL;
            node->symbol->data.func.param_count = 0;
        }
        free(node);
        node = next;
    }
}

static FunctionMetaNode *create_function_meta_node(symbol_data *symbol) {
    FunctionMetaNode *node = malloc(sizeof(*node));
    if (node == NULL) {
        return NULL;
    }

    node->symbol = symbol;
    node->next = NULL;
    return node;
}

SymTable *symtable_create(size_t initial_capacity) {
    SymTable *table = malloc(sizeof(*table));
    if (table == NULL) {
        return NULL;
    }

    table->current_scope = NULL;
    table->function_meta = NULL;
    table->initial_capacity = normalized_capacity(initial_capacity);
    table->scope_depth = 0;

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
        Scope *scope = table->current_scope;
        table->current_scope = scope->parent;
        ht_free(&scope->table);
        free(scope);
    }

    free_function_parameter_arrays(table->function_meta);
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

    ht_init(&scope->table, table->initial_capacity);
    if (scope->table.items == NULL || scope->table.capacity == 0) {
        free(scope);
        return false;
    }

    scope->parent = table->current_scope;
    table->current_scope = scope;
    ++table->scope_depth;
    return true;
}

bool symtable_leave_scope(SymTable *table) {
    if (table == NULL || table->current_scope == NULL) {
        return false;
    }

    Scope *scope = table->current_scope;
    table->current_scope = scope->parent;
    ht_free(&scope->table);
    free(scope);
    --table->scope_depth;
    return true;
}

size_t symtable_scope_depth(SymTable *table) {
    return table == NULL ? 0 : table->scope_depth;
}

symbol_data *symtable_search_current_var(SymTable *table, const char *key) {
    if (table == NULL || key == NULL || table->current_scope == NULL) {
        return NULL;
    }

    return (symbol_data *)ht_search(&table->current_scope->table, key);
}

symbol_data *symtable_search_var(SymTable *table, const char *key) {
    if (table == NULL || key == NULL) {
        return NULL;
    }

    for (Scope *scope = table->current_scope; scope != NULL; scope = scope->parent) {
        symbol_data *symbol = (symbol_data *)ht_search(&scope->table, key);
        if (symbol != NULL) {
            return symbol;
        }
    }

    return NULL;
}

symbol_data *symtable_search_func(SymTable *table, const char *key) {
    if (table == NULL || key == NULL) {
        return NULL;
    }

    return (symbol_data *)ht_search(&table->functions, key);
}

bool symtable_insert_var(SymTable *table, const char *key, data_type type, bool is_mutable) {
    if (table == NULL || key == NULL || table->current_scope == NULL) {
        return false;
    }

    if (symtable_search_current_var(table, key) != NULL) {
        return false;
    }

    symbol_data *symbol = malloc(sizeof(*symbol));
    if (symbol == NULL) {
        return false;
    }

    symbol->type = sym_var;
    symbol->data.var.var_type = type;
    symbol->data.var.is_mutable = is_mutable;
    symbol->data.var.is_parameter = false;

    if (!ht_insert(&table->current_scope->table, key, symbol)) {
        free(symbol);
        return false;
    }

    return true;
}

bool symtable_insert_param(SymTable *table, const char *key, data_type type) {
    if (!symtable_insert_var(table, key, type, false)) {
        return false;
    }

    symbol_data *symbol = symtable_search_current_var(table, key);
    if (symbol == NULL) {
        return false;
    }

    symbol->data.var.is_parameter = true;
    return true;
}

bool symtable_insert_func(SymTable *table, const char *key, data_type ret_type,
                          size_t param_count, const data_type *params) {
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

    symbol->type = sym_func;
    symbol->data.func.ret_type = ret_type;
    symbol->data.func.param_count = param_count;
    symbol->data.func.params = NULL;

    if (param_count > 0) {
        symbol->data.func.params = malloc(param_count * sizeof(*symbol->data.func.params));
        if (symbol->data.func.params == NULL) {
            free(symbol);
            return false;
        }

        for (size_t i = 0; i < param_count; ++i) {
            symbol->data.func.params[i] = params[i];
        }
    }

    FunctionMetaNode *meta = create_function_meta_node(symbol);
    if (meta == NULL) {
        free(symbol->data.func.params);
        free(symbol);
        return false;
    }

    if (!ht_insert(&table->functions, key, symbol)) {
        free(meta);
        free(symbol->data.func.params);
        free(symbol);
        return false;
    }

    meta->next = table->function_meta;
    table->function_meta = meta;
    return true;
}

bool symtable_set_func_return_type(SymTable *table, const char *key, data_type ret_type) {
    symbol_data *symbol = symtable_search_func(table, key);
    if (symbol == NULL || symbol->type != sym_func) {
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

    const data_type any_param[] = { type_any };
    const data_type double_param[] = { type_double };
    const data_type string_param[] = { type_string };
    const data_type string_int_int_params[] = { type_string, type_int, type_int };
    const data_type string_int_params[] = { type_string, type_int };
    const data_type int_param[] = { type_int };

    return symtable_insert_func(table, "print", type_unit, 1, any_param) &&
           symtable_insert_func(table, "readLine", type_string, 0, NULL) &&
           symtable_insert_func(table, "readInt", type_int, 0, NULL) &&
           symtable_insert_func(table, "readDouble", type_double, 0, NULL) &&
           symtable_insert_func(table, "toStr", type_string, 1, any_param) &&
           symtable_insert_func(table, "toInt", type_int, 1, double_param) &&
           symtable_insert_func(table, "strLength", type_int, 1, string_param) &&
           symtable_insert_func(table, "subStr", type_string, 3, string_int_int_params) &&
           symtable_insert_func(table, "ord", type_int, 2, string_int_params) &&
           symtable_insert_func(table, "chr", type_string, 1, int_param);
}
