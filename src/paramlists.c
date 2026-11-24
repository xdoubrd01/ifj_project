#include "paramlists.h"
#include "array_utils.h"
#include "semantic.h"
#include "str_utils.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

bool paramlists_append(ParamLists *lists, size_t length) {
    size_t capacity;
    if (lists->count == SIZE_MAX || !array_next_capacity(lists->capacity, lists->count + 1,
                                                         sizeof(*lists->sizes), 4, &capacity))
        return false;
    if (capacity != lists->capacity) {
        void *sizes = realloc(lists->sizes, capacity * sizeof(*lists->sizes));
        if (sizes == NULL)
            return false;
        lists->sizes    = sizes;
        lists->capacity = capacity;
    }
    lists->sizes[lists->count++] = length;
    return true;
}
void paramlists_free(ParamLists *lists) {
    free(lists->sizes);
    *lists = (ParamLists){0};
}
int paramlists_set_signature(SymTable *table, const char *name, const ParamLists *lists) {
    symbol_data *fn = symtable_search_func(table, name);
    if (fn == NULL || lists->count > SIZE_MAX / sizeof(size_t))
        return 99;
    size_t *sizes = lists->count ? malloc(lists->count * sizeof(*sizes)) : NULL;
    if (lists->count && sizes == NULL)
        return 99;
    if (lists->count)
        memcpy(sizes, lists->sizes, lists->count * sizeof(*sizes));
    free(fn->data.func.param_list_sizes);
    fn->data.func.param_list_sizes = sizes;
    fn->data.func.param_list_count = lists->count;
    return 0;
}
int paramlists_check_shape(SymTable *table, const char *name, const ParamLists *lists) {
    symbol_data *fn = symtable_search_func(table, name);
    if (fn == NULL)
        return 3;
    if (lists->count != fn->data.func.param_list_count)
        return 5;
    for (size_t i = 0; i < lists->count; i++) {
        size_t length = fn->data.func.param_list_sizes ? fn->data.func.param_list_sizes[i]
                                                       : fn->data.func.param_count;
        if (length != lists->sizes[i])
            return 5;
    }
    return 0;
}
bool paramlists_is_bare(SymTable *table, const char *name) {
    symbol_data *fn = symtable_search_func(table, name);
    return fn != NULL && fn->data.func.param_list_count == 0;
}
int paramlists_bare_call(SymTable *table, char **name, ExprNode **out) {
    ParamLists no_lists = {0};
    int        rc       = paramlists_check_shape(table, *name, &no_lists);
    if (rc != 0)
        return rc;
    data_type type;
    rc = sem_check_call(table, *name, NULL, 0, &type);
    if (rc != 0)
        return rc;
    ExprNode *node = expr_new(EXPR_CALL, type);
    if (node == NULL)
        return 99;
    node->u.call.name = *name;
    *name             = NULL;
    *out              = node;
    return 0;
}
static int commit_binding(ParamBindings *b) {
    if (b->pending == NULL)
        return 0;
    size_t capacity;
    if (b->count == SIZE_MAX ||
        !array_next_capacity(b->capacity, b->count + 1, sizeof(*b->items), 8, &capacity))
        return 99;
    if (capacity != b->capacity) {
        void *items = realloc(b->items, capacity * sizeof(*b->items));
        if (items == NULL)
            return 99;
        b->items    = items;
        b->capacity = capacity;
    }
    b->items[b->count++] = (ParamBinding){b->pending, b->depth};
    b->pending           = NULL;
    return 0;
}
int paramlists_track(ParamBindings *b, const Token *token, TokenType next, int *reference) {
    TokenType type = token->type;
    *reference     = 0;
    if (type == TOKEN_EOL || type == TOKEN_SEMICOLON || type == TOKEN_DEDENT || type == TOKEN_EOF) {
        int rc = commit_binding(b);
        if (rc != 0)
            return rc;
    }
    if (type == TOKEN_INDENT) {
        if (b->depth == SIZE_MAX)
            return 99;
        b->depth++;
    } else if (type == TOKEN_DEDENT) {
        while (b->count && b->items[b->count - 1].depth >= b->depth)
            free(b->items[--b->count].name);
        if (b->depth)
            b->depth--;
    } else if (type == TOKEN_IDENTIFIER) {
        const char *name = token->attribute.s_val;
        if (b->previous == TOKEN_KW_VAL || b->previous == TOKEN_KW_VAR) {
            free(b->pending);
            b->pending = string_duplicate(name);
            if (b->pending == NULL)
                return 99;
        } else if (b->previous != TOKEN_DOT && b->previous != TOKEN_COLON && next != TOKEN_ASSIGN) {
            if (next == TOKEN_LPAREN)
                *reference = 1;
            else if (symtable_search_var(b->table, name) == NULL) {
                bool local = false;
                for (size_t i = b->count; i > 0; i--) {
                    if (strcmp(b->items[i - 1].name, name) == 0) {
                        local = true;
                        break;
                    }
                }
                if (!local)
                    *reference = 2;
            }
        }
    }
    b->previous = type;
    return 0;
}
void paramlists_bindings_free(ParamBindings *b) {
    for (size_t i = 0; i < b->count; i++)
        free(b->items[i].name);
    free(b->items);
    free(b->pending);
    *b = (ParamBindings){0};
}
