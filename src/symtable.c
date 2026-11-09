#include "symtable.h"
#include "hashtable.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_SYMTABLE_CAPACITY 32U

typedef struct Scope {
    hashtable    *table;
    struct Scope *parent;
} Scope;

struct SymTable {
    hashtable      *functions;
    Scope          *current_scope;
    symbol_data    *main_function;
    size_t          initial_capacity;
    size_t          scope_depth;
    symtable_status last_error;
};

static bool result(SymTable *table, symtable_status status) {
    if (table != NULL) {
        table->last_error = status;
    }

    return status == SYMTABLE_OK;
}

static bool value_type(data_type type) {
    return type == type_int || type == type_double || type == type_string || type == type_unit;
}

static void destroy_symbol(void *value) {
    symbol_data *symbol = value;

    if (symbol != NULL) {
        if (symbol->type == sym_func) {
            free(symbol->data.func.params);
        }

        free(symbol);
    }
}

SymTable *symtable_create(size_t initial_capacity) {
    SymTable *table = calloc(1, sizeof(*table));
    if (table == NULL) {
        return NULL;
    }

    table->initial_capacity = initial_capacity == 0 ? DEFAULT_SYMTABLE_CAPACITY : initial_capacity;

    table->functions = ht_create(table->initial_capacity);
    if (table->functions == NULL) {
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

    ht_destroy(table->functions, destroy_symbol);
    free(table);
}

symtable_status symtable_last_error(const SymTable *table) {
    return table == NULL ? SYMTABLE_INTERNAL_ERROR : table->last_error;
}

bool symtable_enter_scope(SymTable *table) {
    if (table == NULL || table->scope_depth == SIZE_MAX) {
        return result(table, SYMTABLE_INTERNAL_ERROR);
    }

    Scope *scope = malloc(sizeof(*scope));
    if (scope == NULL) {
        return result(table, SYMTABLE_INTERNAL_ERROR);
    }

    scope->table = ht_create(table->initial_capacity);
    if (scope->table == NULL) {
        free(scope);
        return result(table, SYMTABLE_INTERNAL_ERROR);
    }

    scope->parent        = table->current_scope;
    table->current_scope = scope;
    ++table->scope_depth;

    return result(table, SYMTABLE_OK);
}

bool symtable_leave_scope(SymTable *table) {
    if (table == NULL || table->current_scope == NULL) {
        return result(table, SYMTABLE_INTERNAL_ERROR);
    }

    Scope *scope         = table->current_scope;
    table->current_scope = scope->parent;

    ht_destroy(scope->table, destroy_symbol);
    free(scope);

    --table->scope_depth;

    return result(table, SYMTABLE_OK);
}

size_t symtable_scope_depth(SymTable *table) { return table == NULL ? 0 : table->scope_depth; }

symbol_data *symtable_search_current_var(SymTable *table, const char *key) {
    return table == NULL || table->current_scope == NULL
               ? NULL
               : ht_search(table->current_scope->table, key);
}

symbol_data *symtable_search_var(SymTable *table, const char *key) {
    if (table == NULL || key == NULL) {
        return NULL;
    }

    for (Scope *scope = table->current_scope; scope != NULL; scope = scope->parent) {
        symbol_data *symbol = ht_search(scope->table, key);

        if (symbol != NULL) {
            return symbol;
        }
    }

    return NULL;
}

symbol_data *symtable_search_func(SymTable *table, const char *key) {
    return table == NULL ? NULL : ht_search(table->functions, key);
}

static bool insert_variable(SymTable *table, const char *key, data_type type, bool is_mutable,
                            bool is_parameter) {
    if (table == NULL || table->current_scope == NULL || key == NULL) {
        return result(table, SYMTABLE_INTERNAL_ERROR);
    }

    if (symtable_search_current_var(table, key) != NULL) {
        return result(table, SYMTABLE_REDEFINITION);
    }

    if (!value_type(type)) {
        return result(table, SYMTABLE_TYPE_ERROR);
    }

    symbol_data *symbol = calloc(1, sizeof(*symbol));
    if (symbol == NULL) {
        return result(table, SYMTABLE_INTERNAL_ERROR);
    }

    symbol->type                  = sym_var;
    symbol->data.var.var_type     = type;
    symbol->data.var.is_mutable   = is_mutable;
    symbol->data.var.is_parameter = is_parameter;

    if (!ht_insert(table->current_scope->table, key, symbol)) {
        free(symbol);
        return result(table, SYMTABLE_INTERNAL_ERROR);
    }

    return result(table, SYMTABLE_OK);
}

bool symtable_insert_var(SymTable *table, const char *key, data_type type, bool is_mutable) {
    return insert_variable(table, key, type, is_mutable, false);
}

bool symtable_insert_param(SymTable *table, const char *key, data_type type) {
    return insert_variable(table, key, type, false, true);
}

static symbol_data *make_function(data_type ret_type, size_t count, const data_type *params,
                                  bool builtin) {
    if (count > SIZE_MAX / sizeof(data_type)) {
        return NULL;
    }

    symbol_data *symbol = calloc(1, sizeof(*symbol));
    if (symbol == NULL) {
        return NULL;
    }

    symbol->type                           = sym_func;
    symbol->data.func.ret_type             = ret_type;
    symbol->data.func.param_count          = count;
    symbol->data.func.return_type_explicit = ret_type != type_unknown;
    symbol->data.func.is_builtin           = builtin;

    if (count != 0) {
        symbol->data.func.params = malloc(count * sizeof(data_type));

        if (symbol->data.func.params == NULL) {
            free(symbol);
            return NULL;
        }

        memcpy(symbol->data.func.params, params, count * sizeof(data_type));
    }

    return symbol;
}

bool symtable_insert_func(SymTable *table, const char *key, data_type ret_type, size_t param_count,
                          const data_type *params) {
    if (table == NULL || key == NULL || (param_count != 0 && params == NULL) ||
        param_count > SIZE_MAX / sizeof(data_type)) {
        return result(table, SYMTABLE_INTERNAL_ERROR);
    }

    if (symtable_search_func(table, key) != NULL) {
        return result(table, SYMTABLE_REDEFINITION);
    }

    if (ret_type != type_unknown && !value_type(ret_type)) {
        return result(table, SYMTABLE_TYPE_ERROR);
    }

    for (size_t i = 0; i < param_count; ++i) {
        if (!value_type(params[i])) {
            return result(table, SYMTABLE_TYPE_ERROR);
        }
    }

    symbol_data *symbol = make_function(ret_type, param_count, params, false);

    if (symbol == NULL) {
        return result(table, SYMTABLE_INTERNAL_ERROR);
    }

    if (!ht_insert(table->functions, key, symbol)) {
        destroy_symbol(symbol);
        return result(table, SYMTABLE_INTERNAL_ERROR);
    }

    return result(table, SYMTABLE_OK);
}

bool symtable_set_func_return_type(SymTable *table, const char *key, data_type ret_type) {
    if (table == NULL || key == NULL) {
        return result(table, SYMTABLE_INTERNAL_ERROR);
    }

    symbol_data *symbol = symtable_search_func(table, key);
    if (symbol == NULL) {
        return result(table, SYMTABLE_UNDEFINED);
    }

    if (!value_type(ret_type) ||
        (symbol->data.func.ret_type != type_unknown && symbol->data.func.ret_type != ret_type)) {
        return result(table, SYMTABLE_TYPE_ERROR);
    }

    symbol->data.func.ret_type = ret_type;

    return result(table, SYMTABLE_OK);
}

bool symtable_type_compatible(data_type expected, data_type actual) {
    if (expected == type_any) {
        return value_type(actual);
    }

    if ((!value_type(expected) && expected != type_bool) ||
        (!value_type(actual) && actual != type_bool)) {
        return false;
    }

    return expected == actual || (expected == type_double && actual == type_int);
}

bool symtable_insert_builtins(SymTable *table) {
    static const struct {
        const char *name;
        data_type   ret_type;
        size_t      count;
        data_type   params[3];
    } builtins[] = {{"print", type_unit, 1, {type_any}},
                    {"readLine", type_string, 0, {type_int}},
                    {"readInt", type_int, 0, {type_int}},
                    {"readDouble", type_double, 0, {type_int}},
                    {"toStr", type_string, 1, {type_any}},
                    {"toInt", type_int, 1, {type_double}},
                    {"strLength", type_int, 1, {type_string}},
                    {"subStr", type_string, 3, {type_string, type_int, type_int}},
                    {"ord", type_int, 2, {type_string, type_int}},
                    {"chr", type_string, 1, {type_int}}};

    if (table == NULL) {
        return false;
    }

    size_t count = sizeof(builtins) / sizeof(builtins[0]);

    /* Detect collisions before changing the table. */
    for (size_t i = 0; i < count; ++i) {
        if (symtable_search_func(table, builtins[i].name) != NULL) {
            return result(table, SYMTABLE_REDEFINITION);
        }
    }

    size_t inserted = 0;

    for (; inserted < count; ++inserted) {
        symbol_data *symbol = make_function(builtins[inserted].ret_type, builtins[inserted].count,
                                            builtins[inserted].params, true);

        if (symbol == NULL) {
            break;
        }

        if (!ht_insert(table->functions, builtins[inserted].name, symbol)) {
            destroy_symbol(symbol);
            break;
        }
    }

    if (inserted == count) {
        return result(table, SYMTABLE_OK);
    }

    /* Roll back successfully inserted builtins after allocation failure. */
    for (size_t i = 0; i < inserted; ++i) {
        symbol_data *symbol = ht_search(table->functions, builtins[i].name);

        ht_delete(table->functions, builtins[i].name);
        destroy_symbol(symbol);
    }

    return result(table, SYMTABLE_INTERNAL_ERROR);
}

bool symtable_check_call(SymTable *table, const char *key, size_t arg_count,
                         const data_type *arg_types) {
    if (table == NULL || key == NULL || (arg_count != 0 && arg_types == NULL)) {
        return result(table, SYMTABLE_INTERNAL_ERROR);
    }

    symbol_data *symbol = symtable_search_func(table, key);
    if (symbol == NULL) {
        return result(table, SYMTABLE_UNDEFINED);
    }

    if (arg_count != symbol->data.func.param_count) {
        return result(table, SYMTABLE_ARGUMENT_ERROR);
    }

    for (size_t i = 0; i < arg_count; ++i) {
        if (arg_types[i] == type_unknown) {
            return result(table, SYMTABLE_TYPE_ERROR);
        }

        if (!value_type(arg_types[i]) ||
            !symtable_type_compatible(symbol->data.func.params[i], arg_types[i])) {
            return result(table, SYMTABLE_ARGUMENT_ERROR);
        }
    }

    return result(table, SYMTABLE_OK);
}

bool symtable_check_assignment(SymTable *table, const char *key, data_type actual) {
    if (table == NULL || key == NULL) {
        return result(table, SYMTABLE_INTERNAL_ERROR);
    }

    symbol_data *symbol = symtable_search_var(table, key);
    if (symbol == NULL) {
        return result(table, SYMTABLE_UNDEFINED);
    }

    if (!symbol->data.var.is_mutable) {
        return result(table, SYMTABLE_IMMUTABLE);
    }

    return result(table, symtable_type_compatible(symbol->data.var.var_type, actual)
                             ? SYMTABLE_OK
                             : SYMTABLE_TYPE_ERROR);
}

bool symtable_check_return(SymTable *table, const char *key, data_type actual, bool has_value) {
    if (table == NULL || key == NULL) {
        return result(table, SYMTABLE_INTERNAL_ERROR);
    }

    symbol_data *symbol = symtable_search_func(table, key);
    if (symbol == NULL) {
        return result(table, SYMTABLE_UNDEFINED);
    }

    if (symbol->data.func.is_builtin || !symbol->data.func.return_type_explicit) {
        return result(table, SYMTABLE_TYPE_ERROR);
    }

    data_type expected = symbol->data.func.ret_type;

    bool valid = has_value ? value_type(actual) && (expected == type_unit ||
                                                    symtable_type_compatible(expected, actual))
                           : expected == type_unit;

    return result(table, valid ? SYMTABLE_OK : SYMTABLE_TYPE_ERROR);
}

bool symtable_mark_main(SymTable *table, const char *key) {
    if (table == NULL || key == NULL) {
        return result(table, SYMTABLE_INTERNAL_ERROR);
    }

    symbol_data *symbol = symtable_search_func(table, key);
    if (symbol == NULL) {
        return result(table, SYMTABLE_UNDEFINED);
    }

    if (table->main_function != NULL || symbol->data.func.is_builtin) {
        return result(table, SYMTABLE_REDEFINITION);
    }

    if (symbol->data.func.param_count != 0) {
        return result(table, SYMTABLE_ARGUMENT_ERROR);
    }

    if (symbol->data.func.ret_type != type_unknown && symbol->data.func.ret_type != type_unit) {
        return result(table, SYMTABLE_TYPE_ERROR);
    }

    symbol->data.func.is_main = true;
    table->main_function      = symbol;

    return result(table, SYMTABLE_OK);
}

bool symtable_validate_main(SymTable *table) {
    if (table == NULL) {
        return false;
    }

    if (table->main_function == NULL) {
        return result(table, SYMTABLE_UNDEFINED);
    }

    return result(table, table->main_function->data.func.ret_type == type_unit
                             ? SYMTABLE_OK
                             : SYMTABLE_TYPE_ERROR);
}