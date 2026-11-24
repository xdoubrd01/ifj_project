// IFJ26: grouped parameter lists and bare function calls.
#ifndef PARAMLISTS_H
#define PARAMLISTS_H
#include "expr_tree.h"
#include "scanner.h"
typedef struct {
    size_t *sizes;
    size_t  count, capacity;
} ParamLists;
bool paramlists_append(ParamLists *lists, size_t length);
void paramlists_free(ParamLists *lists);
int  paramlists_set_signature(SymTable *table, const char *name, const ParamLists *lists);
int  paramlists_check_shape(SymTable *table, const char *name, const ParamLists *lists);
bool paramlists_is_bare(SymTable *table, const char *name);
/* Takes *name only on success. A visible variable is resolved before this call. */
int paramlists_bare_call(SymTable *table, char **name, ExprNode **out);

/* First-pass dependency collection: lexical bindings prevent false call cycles. */
typedef struct {
    char  *name;
    size_t depth;
} ParamBinding;
typedef struct {
    ParamBinding *items;
    size_t        count, capacity, depth;
    char         *pending;
    TokenType     previous;
    SymTable     *table;
} ParamBindings;
/* reference: 0 none, 1 parenthesized call, 2 possible bare function call. */
int  paramlists_track(ParamBindings *bindings, const Token *token, TokenType next, int *reference);
void paramlists_bindings_free(ParamBindings *bindings);
#endif
