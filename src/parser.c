/**
 * @par Projekt
 * Implementace překladače imperativního jazyka IFJ26
 * @file parser.c
 * @author Eliáš Oharek
 * @brief Implementace syntaktického analyzátoru metodou rekurzivního sestupu.
 *
 * @note Při jazykovém sjednocení a zpřesnění komentářů byla využita AI (ChatGPT).
 */

#include "code_generator.h"
#include "exp_parser.h"
#include "expr_tree.h"
#include "paramlists.h"
#include "parser.h"
#include "scanner.h"
#include "semantic.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "symtable.h"

// --- Stav parseru a pomocné datové struktury ---

// Aktuální token a případný přednačtený token z lexeru.
static Token tok; // aktuální token
static Token lookahead;
static bool has_lookahead = false;
static SymTable *ts = NULL;
static data_type current_return_type = type_unknown;
static char *entry_function_name = NULL;
static bool stmt_returns = false;
static bool block_returns = false;
// Úprava s pomocí AI (původní označení bez uvedeného promptu).
static data_type stmt_value = type_unit; // typ hodnoty posledního příkazu
static data_type block_value = type_unit; // typ hodnoty posledního příkazu bloku
static int block_depth = 0; // 1 = přímo tělo funkce
static bool has_explicit_type = false; // hlavička má uvedený návratový typ
static bool emit_code = false; // kód se generuje až v posledním průchodu

// Prompt AI: "Navratovy typ funkce bez anotace se odvozuje z jejiho tela. Priprav
//          jednoduchy graf volani z prvniho pruchodu, abychom poznali rekurzi
//          a vedeli, v jakem poradi typy odvozovat."
typedef struct {
    char *name;
    bool typed;
    bool *callee_bare; // odkazy bez závorek se vyřeší po načtení všech hlaviček
    char **callees; // jména volaných funkcí z prvního průchodu
    size_t *callee_ids; // jejich indexy, doplní resolve_callees()
    size_t callee_count;
} FuncInfo;

static FuncInfo *func_infos = NULL;
static size_t func_info_count = 0;
static size_t func_info_pos = 0; // funkce jsou v každém průchodu ve stejném pořadí

typedef enum {
    FIRST_ROUND,
    INFER_ROUND,
    SECOND_ROUND
} ParserRound;

/*
 * Úsek vygenerovaný pomocí AI (Claude), prompt: "vygeneruj mi kostru vsech funkci
 * pravidel".
 */

// --- Deklarace interních funkcí ---

// Deklarace interních funkcí parseru (implementace jsou níže).
// Práce s tokeny
static void advance(void);
static void expect(TokenType type);
static TokenType peek_token(void);

// Program a funkce
static void parse_prolog(void);
static void parse_program(ParserRound round);
static void parse_functions(ParserRound round);
static void parse_function(ParserRound round);
static void parse_opt_annotation(void);
static void parse_opt_eol(void);
static data_type parse_function_body(void);
static void skip_function_body(FuncInfo *info);

// Parametry, typy a pomocná pole
static void append_param(data_type **types, size_t *count, data_type type);
static void append_param_name(char ***names, size_t count, char *name);
static data_type parse_parameters_start(char **out_name);
static void parse_parameters_tail(data_type **types, char ***names, size_t *count);
static void parse_parameters_list(data_type **types, char ***names, size_t *count);
static data_type parse_type(void);
static data_type parse_opt_type(void);
static data_type parse_opt_return_type(void);

// Bloky, příkazy a návratové hodnoty
static void parse_block(bool new_scope);
static void parse_stmts(void);
static void parse_stmt(void);
static void parse_stmt_tail(void);
static void parse_stmt_tail_id(char *var_name);
static void parse_opt_return_tail(void);
static int is_stmt_start(TokenType t);

// Výrazy a volání funkcí
static int is_non_id_expression(TokenType t);
static data_type parse_call_or_exp(ExprNode **out_expr);
static ExprNode *parse_expression_wrapper(void);
static void check_function_value(data_type value);
static void parse_value_stmt(void);

// --- Práce s tokeny a základní pravidla ---

// Při posunu nejdřív uvolníme text starého tokenu.
// Pokud už jsme token načetli přes peek_token(), použijeme ho.
static void advance(void) {
    if (tok.type == TOKEN_IDENTIFIER || tok.type == TOKEN_STRING_LITERAL)
        free(tok.attribute.s_val);

    // Přednačtený token
    if (has_lookahead) {
        tok = lookahead;
        has_lookahead = false;
    }
    // Jinak čteme přímo z lexeru.
    else {
        int rc = get_next_token(&tok);

        if (rc != 0)
            exit(rc);
    }
}

// Ukončí překlad se syntaktickou chybou.
_Noreturn void syntax_error(void) {
    fprintf(stderr, "Syntax error: unexpected token: %d\n", tok.type);

    exit(2);
}

// Ověří očekávaný token a posune se na další.
static void expect(TokenType type) {
    if (tok.type != type)
        syntax_error();
    advance();
}

// Kontrola povinného prologu import ifj26.*
static void parse_prolog(void) {
    expect(TOKEN_KW_IMPORT);
    if (tok.type == TOKEN_IDENTIFIER && strcmp(tok.attribute.s_val, "ifj26") == 0) {
        advance();
        expect(TOKEN_DOT);
        expect(TOKEN_MUL);
        expect(TOKEN_EOL);
    } else {
        syntax_error();
    }
}

// Načte další token dopředu, ale zatím ho nepředá jako aktuální.
static TokenType peek_token(void) {
    if (has_lookahead == false) {
        int succes = get_next_token(&lookahead);

        if (succes != 0)
            exit(succes);

        has_lookahead = true;
    }
    return lookahead.type;
}

// Hlavní pravidlo gramatiky PROGRAM.
static void parse_program(ParserRound round) {
    parse_prolog();
    parse_functions(round);
    expect(TOKEN_EOF);
}

// --- Graf volání a odvozování typů ---

// Prompt AI: "Pridej male pomocne funkce nad grafem volani: preved jmena volanych
//          funkci na indexy, zjisti primou i neprimou rekurzi a over, jestli
//          uz maji volane funkce znamy navratovy typ. At je to rychle i pro
//          velmi dlouhe programy."
static int compare_func_ids(const void *a, const void *b) {
    return strcmp(func_infos[*(const size_t *)a].name, func_infos[*(const size_t *)b].name);
}

static int compare_name_to_id(const void *name, const void *id) {
    return strcmp((const char *)name, func_infos[*(const size_t *)id].name);
}

// Vestavěné a nedefinované funkce se do grafu nepřidávají.

static void resolve_callees(void) {
    size_t *order = malloc(func_info_count * sizeof(size_t));
    if (order == NULL)
        exit(99);

    for (size_t i = 0; i < func_info_count; i++)
        order[i] = i;
    qsort(order, func_info_count, sizeof(size_t), compare_func_ids);

    for (size_t i = 0; i < func_info_count; i++) {
        FuncInfo *info = &func_infos[i];
        size_t kept = 0;

        if (info->callee_count > 0) {
            info->callee_ids = malloc(info->callee_count * sizeof(size_t));
            if (info->callee_ids == NULL)
                exit(99);
        }
        for (size_t j = 0; j < info->callee_count; j++) {
            size_t *found = bsearch(info->callees[j], order, func_info_count, sizeof(size_t), compare_name_to_id);
            if (found != NULL && (!info->callee_bare[j] || paramlists_is_bare(ts, info->callees[j]))) {
                info->callee_ids[kept++] = *found;
            }
            free(info->callees[j]);
        }
        free(info->callees);
        free(info->callee_bare);
        info->callee_bare = NULL;
        info->callees = NULL;
        info->callee_count = kept;
    }
    free(order);
}

// Vrátí true, pokud se z funkce start lze voláním vrátit zpět do ní.
static bool is_recursive(size_t start, size_t *stack, size_t *visited) {
    size_t top = 0;
    stack[top++] = start;
    visited[start] = start + 1;

    while (top > 0) {
        const FuncInfo *info = &func_infos[stack[--top]];

        for (size_t i = 0; i < info->callee_count; i++) {
            size_t next = info->callee_ids[i];
            if (next == start)
                return true;
            if (visited[next] != start + 1) {
                visited[next] = start + 1;
                stack[top++] = next;
            }
        }
    }
    return false;
}

// Přímo i nepřímo rekurzivní funkce musí mít uvedený návratový typ.
static void check_inferred_recursion(void) {
    size_t *stack = malloc(func_info_count * sizeof(size_t));
    size_t *visited = calloc(func_info_count, sizeof(size_t));
    if (stack == NULL || visited == NULL)
        exit(99);

    for (size_t i = 0; i < func_info_count; i++) {
        if (!func_infos[i].typed && is_recursive(i, stack, visited))
            exit(6);
    }
    free(stack);
    free(visited);
}

// Zjistí, zda ještě existuje funkce s neznámým návratovým typem.
static bool has_unresolved_function(void) {
    for (size_t i = 0; i < func_info_count; i++) {
        if (!func_infos[i].typed)
            return true;
    }
    return false;
}

// Ověří, zda jsou známé návratové typy všech volaných funkcí.
static bool callees_resolved(const FuncInfo *info) {
    for (size_t i = 0; i < info->callee_count; i++) {
        if (!func_infos[info->callee_ids[i]].typed)
            return false;
    }
    return true;
}

static void free_func_infos(void) {
    for (size_t i = 0; i < func_info_count; i++) {
        free(func_infos[i].callee_ids);
        free(func_infos[i].name);
    }
    free(func_infos);
    func_infos = NULL;
    func_info_count = 0;
}

// --- Průchody parseru ---

// Prompt AI: "Dalsi pruchod musi zacit znovu od zacatku vstupu, priprav na to
//          jednu funkci, at se stejny kod neopakuje."
// Nastaví lexer na začátek vstupu pro další průchod.
static void restart_scanner(const char *input, size_t length) {
    if (has_lookahead) {
        if (lookahead.type == TOKEN_IDENTIFIER || lookahead.type == TOKEN_STRING_LITERAL)
            free(lookahead.attribute.s_val);
        has_lookahead = false;
    }
    scanner_free();
    scanner_init_buffer(input, length);
    func_info_pos = 0;
    advance();
}

// První průchod načte hlavičky funkcí a sestaví graf volání.
// Mezilehlé průchody odvozují typy, poslední kontroluje těla a generuje kód.
// Úprava s pomocí AI (původní označení bez uvedeného promptu).
void parse(const char *input, size_t length) {
    ts = symtable_create(0);
    if (ts == NULL)
        exit(99);

    if (!symtable_insert_builtins(ts))
        exit(99);

    // První průchod
    scanner_init_buffer(input, length);
    advance();
    parse_program(FIRST_ROUND);

    // Kontrola vstupní funkce a rekurze
    if (entry_function_name == NULL)
        exit(3);
    resolve_callees();
    check_inferred_recursion();

    // Každý průchod odvodí typy funkcí, jejichž volané funkce už mají známý typ.
    // Kontrola rekurze vyloučila cykly mezi funkcemi s neznámým typem.
    while (has_unresolved_function()) {
        restart_scanner(input, length);
        parse_program(INFER_ROUND);
    }

    symbol_data *entry = symtable_search_func(ts, entry_function_name);
    if (entry == NULL || entry->data.func.ret_type != type_unit)
        exit(6);

    // Tabulku symbolů ponecháme pro poslední průchod.
    restart_scanner(input, length);
    code_gen_init(entry_function_name);
    emit_code = true;
    parse_program(SECOND_ROUND);

    scanner_free();
    free_func_infos();
}
// Konec úseku upraveného s pomocí AI

// --- Zpracování funkcí a parametrů ---

static void parse_functions(ParserRound round) {
    while (tok.type == TOKEN_EOL)
        advance();

    if (tok.type == TOKEN_KW_DEF || tok.type == TOKEN_AT_MAIN) {
        parse_function(round);
        parse_functions(round);
    }
}

static void parse_opt_annotation(void) {
    if (tok.type == TOKEN_AT_MAIN) {
        advance();
        parse_opt_eol();
    }
}

static void parse_opt_eol(void) {
    if (tok.type == TOKEN_EOL)
        advance();
}

/*
 * Kód vygenerovaný pomocí AI (ChatGPT).
 * Prompt AI: "udelej pomocnou funkci pro pridani jednoho datoveho typu
 * na konec dynamickeho pole pomoci realoc."
 */
static void append_param(data_type **types, size_t *count, data_type type) {
    data_type *tmp = realloc(*types, (*count + 1) * sizeof(data_type));

    if (tmp == NULL)
        exit(99);

    *types = tmp;
    (*types)[*count] = type;
    (*count)++;
}

// count už zahrnuje i nově přidávané jméno.
static void append_param_name(char ***names, size_t count, char *name) {
    char **tmp = realloc(*names, count * sizeof(char *));

    if (tmp == NULL) {
        free(name);
        exit(99);
    }

    *names = tmp;
    (*names)[count - 1] = name;
}

// Jméno parametru kopírujeme, protože text aktuálního tokenu
// se při advance() uvolní. Kopii potom převezme volající.
static data_type parse_parameters_start(char **out_name) {
    if (tok.type != TOKEN_IDENTIFIER)
        syntax_error();

    char *param_name = strdup(tok.attribute.s_val);

    if (param_name == NULL)
        exit(99);

    expect(TOKEN_IDENTIFIER);
    expect(TOKEN_COLON);
    data_type param_type = parse_type();

    bool success = symtable_insert_var(ts, param_name, param_type, false);

    if (success == false) {
        free(param_name);
        exit(4);
    }
    *out_name = param_name;
    return param_type;
}

static void parse_parameters_tail(data_type **types, char ***names, size_t *count) {
    if (tok.type == TOKEN_COMMA) {
        advance();

        // Koncová čárka je povolena jen před ')' na dalším řádku.
        if (tok.type == TOKEN_RPAREN && tok.newline_before)
            return;

        char *name = NULL;
        data_type type = parse_parameters_start(&name);

        append_param(types, count, type);
        append_param_name(names, *count, name);

        parse_parameters_tail(types, names, count);
    }
}

static void parse_parameters_list(data_type **types, char ***names, size_t *count) {
    if (tok.type == TOKEN_IDENTIFIER) {
        char *name = NULL;
        data_type type = parse_parameters_start(&name);

        append_param(types, count, type);
        append_param_name(names, *count, name);

        parse_parameters_tail(types, names, count);
    }
}

static data_type parse_opt_return_type(void) {
    if (tok.type == TOKEN_COLON) {
        advance();
        data_type ret_type = parse_type();

        return ret_type;
    }

    return type_unknown;
}

// --- Sběr volání a zpracování těla funkce ---

// Prompt AI: "Pri preskakovani tela si zapamatuj, ktere funkce se v nem volaji."
static void add_callee(FuncInfo *info, const char *name, bool bare) {
    char **grown = realloc(info->callees, (info->callee_count + 1) * sizeof(char *));
    if (grown == NULL)
        exit(99);
    info->callees = grown;
    bool *flags = realloc(info->callee_bare, (info->callee_count + 1) * sizeof(*flags));
    if (flags == NULL)
        exit(99);
    info->callee_bare = flags;
    info->callee_bare[info->callee_count] = bare;
    info->callees[info->callee_count] = strdup(name);
    if (info->callees[info->callee_count] == NULL)
        exit(99);
    info->callee_count++;
}

// Sbírá volání se zohledněním lokálních proměnných a jmen členů.
static void skip_token(FuncInfo *info, ParamBindings *bindings) {
    if (info != NULL) {
        int reference;
        TokenType next = tok.type == TOKEN_IDENTIFIER ? peek_token() : TOKEN_EOF;
        int rc = paramlists_track(bindings, &tok, next, &reference);
        if (rc != 0)
            exit(rc);
        if (reference)
            add_callee(info, tok.attribute.s_val, reference == 2);
    }
    advance();
}

// Při přeskakování víceřádkového těla sledujeme úroveň zanoření,
// aby nás nezastavil DEDENT vnitřního bloku.
static void skip_function_body(FuncInfo *info) {
    ParamBindings bindings = {.table = ts, .previous = TOKEN_EOF};
    if (tok.type == TOKEN_EOL) {
        int depth = 0;
        advance();
        expect(TOKEN_INDENT);
        depth++;

        while (depth > 0) {
            if (tok.type == TOKEN_EOF)
                syntax_error();
            if (tok.type == TOKEN_INDENT)
                depth++;
            if (tok.type == TOKEN_DEDENT)
                depth--;
            skip_token(info, &bindings);
        }
    } else {
        while (tok.type != TOKEN_EOL && tok.type != TOKEN_EOF)
            skip_token(info, &bindings);

        if (tok.type == TOKEN_EOL)
            advance();
    }
    paramlists_bindings_free(&bindings);
}

// Hlavičku funkce načítáme při každém průchodu, registrujeme ji jen v prvním.
// Zpracování těla závisí na aktuálním průchodu parseru.
// Úprava s pomocí AI (původní označení bez uvedeného promptu).
static void parse_function(ParserRound round) {
    bool is_entry = (tok.type == TOKEN_AT_MAIN);

    parse_opt_annotation();
    expect(TOKEN_KW_DEF);

    if (tok.type != TOKEN_IDENTIFIER)
        syntax_error();

    char *func_name = strdup(tok.attribute.s_val);

    if (func_name == NULL)
        exit(99);

    expect(TOKEN_IDENTIFIER);

    if (symtable_enter_scope(ts) == false)
        exit(99);

    data_type *param_types = NULL;
    char **param_names = NULL;
    size_t param_count = 0;
    ParamLists parameter_lists = {0};
    // Funkce může obsahovat více seznamů parametrů nebo žádný.
    while (tok.type == TOKEN_LPAREN) {
        size_t start = param_count;
        advance();
        parse_parameters_list(&param_types, &param_names, &param_count);
        expect(TOKEN_RPAREN);
        if (!paramlists_append(&parameter_lists, param_count - start))
            exit(99);
    }

    data_type ret_type = parse_opt_return_type();

    // Kontrola podmínek vstupní funkce označené @main.
    if (round == FIRST_ROUND && is_entry) {
        if (entry_function_name != NULL)
            exit(4);

        if (param_count != 0)
            exit(5);

        // Bez uvedeného typu se Unit ověří po odvození.
        if (ret_type != type_unit && ret_type != type_unknown)
            exit(6);

        entry_function_name = strdup(func_name);
        if (entry_function_name == NULL)
            exit(99);
    }

    if (round == FIRST_ROUND) {
       
        bool success = symtable_insert_func(ts, func_name, ret_type, param_count, param_types);

        if (!success) {
            free(func_name);
            free(param_types);
            exit(4);
        }

        // Jména musí být ve stejném pořadí jako typy parametrů.
        if (!symtable_set_func_param_names(ts, func_name, param_count, (const char *const *)param_names))
            exit(99);

        int list_rc = paramlists_set_signature(ts, func_name, &parameter_lists);
        if (list_rc != 0)
            exit(list_rc);

        // Záznam pro graf volání
        FuncInfo *grown = realloc(func_infos, (func_info_count + 1) * sizeof(FuncInfo));
        if (grown == NULL)
            exit(99);
        func_infos = grown;
        func_infos[func_info_count].name = strdup(func_name);
        if (func_infos[func_info_count].name == NULL)
            exit(99);
        func_infos[func_info_count].typed = (ret_type != type_unknown);
        func_infos[func_info_count].callee_bare = NULL;
        func_infos[func_info_count].callees = NULL;
        func_infos[func_info_count].callee_ids = NULL;
        func_infos[func_info_count].callee_count = 0;
        func_info_count++;
    }
    expect(TOKEN_ASSIGN);

    FuncInfo *info = &func_infos[func_info_pos++];

    // Zpracování těla podle aktuálního průchodu.
    if (round == FIRST_ROUND) {
        skip_function_body(info);
    } else if (round == INFER_ROUND) {
        if (info->typed || !callees_resolved(info)) {
            skip_function_body(NULL);
        } else {
            has_explicit_type = false;
            current_return_type = type_unknown;

            data_type inferred = parse_function_body();

            if (!symtable_set_func_return_type(ts, func_name, inferred))
                exit(99);
            info->typed = true;
        }
    } else {
        has_explicit_type = (ret_type != type_unknown);
        // U funkce bez uvedeného typu je v tabulce už odvozený typ.
        ret_type = symtable_search_func(ts, func_name)->data.func.ret_type;
        current_return_type = ret_type;

        gen_func_start(ts, func_name, ret_type);

        parse_function_body();

        gen_func_end();

        current_return_type = type_unknown;
    }

    symtable_leave_scope(ts);

    // Tabulka symbolů si jména kopíruje, dočasná pole už nepotřebujeme.
    for (size_t i = 0; i < param_count; i++)
        free(param_names[i]);

    free(param_names);
    free(param_types);
    paramlists_free(&parameter_lists);
    free(func_name);
}

// --- Bloky, typy a výrazy ---

static void parse_block(bool new_scope) {
    expect(TOKEN_INDENT);

    // Tělo funkce sdílí rozsah platnosti s parametry.
    if (new_scope && !symtable_enter_scope(ts))
        exit(99);
    block_depth++;

    parse_stmt();
    parse_stmts();

    // Výsledek bloku určuje jeho poslední příkaz.
    bool last_returns = stmt_returns;
    data_type last_value = stmt_value;

    expect(TOKEN_DEDENT);

    block_depth--;
    if (new_scope)
        symtable_leave_scope(ts);
    block_returns = last_returns;
    block_value = last_value;
}

static data_type parse_type(void) {
    if (tok.type == TOKEN_TYPE_INT) {
        expect(TOKEN_TYPE_INT);
        return type_int;
    } else if (tok.type == TOKEN_TYPE_DOUBLE) {
        expect(TOKEN_TYPE_DOUBLE);
        return type_double;
    } else if (tok.type == TOKEN_TYPE_STRING) {
        expect(TOKEN_TYPE_STRING);
        return type_string;
    } else if (tok.type == TOKEN_TYPE_UNIT) {
        expect(TOKEN_TYPE_UNIT);
        return type_unit;
    } else {
        // Identifikátor na místě typu je nedefinovaný typ.
        if (tok.type == TOKEN_IDENTIFIER)
            exit(3);
        syntax_error();
        exit(2);
    }
}

static int is_non_id_expression(TokenType t) {
    if (t == TOKEN_STRING_LITERAL || t == TOKEN_DOUBLE_LITERAL || t == TOKEN_INT_LITERAL || t == TOKEN_MINUS || t == TOKEN_PLUS || t == TOKEN_LPAREN) {
        return 1;
    } else {
        return 0;
    }
}

// Volání funkcí i ostatní výrazy zpracuje precedenční parser.
// Úprava s pomocí AI (ChatGPT): přidání výstupního parametru pro ExprNode.
static data_type parse_call_or_exp(ExprNode **out_expr) {
    *out_expr = NULL;

    if (tok.type == TOKEN_IDENTIFIER || is_non_id_expression(tok.type)) {
        ExprNode *expr = parse_expression_wrapper();
        if (expr == NULL)
            exit(99);

        *out_expr = expr;
        return expr->type;
    }

    syntax_error();
}

// Zpracuje oddělovač příkazů nebo konec bloku či souboru.
static void parse_stmt_tail(void) {
    if (tok.type == TOKEN_SEMICOLON) {
        advance();
        if (tok.type == TOKEN_EOL)
            advance();
    } else if (tok.type == TOKEN_EOL)
        advance();
    else if (tok.type == TOKEN_DEDENT || tok.type == TOKEN_EOF)
        return;
    else
        syntax_error();
}

// --- Přiřazení a návratové hodnoty ---

// Začátek úseku upraveného s pomocí AI
// Prompt AI:
// Oprav parse_stmt_tail_id tak, aby po identifikatoru
// vyzadovala platne prirazeni. Odmitni neplatne
// samostatne identifikatory a zachovej semantickou
// kontrolu i generovani prirazeni.

// Zpracuje přiřazení do proměnné a ověří jeho sémantiku.
static void parse_stmt_tail_id(char *var_name) {
    if (tok.type != TOKEN_ASSIGN)
        syntax_error();

    advance();

    ExprNode *expr = NULL;
    data_type expr_type = parse_call_or_exp(&expr);

    unsigned var_id;
    int rc = sem_check_assignment(ts, var_name, expr_type, &var_id);

    if (rc != 0) {
        expr_free(expr);
        exit(rc);
    }

    data_type var_type;
    rc = sem_get_variable_type(ts, var_name, &var_type, NULL);

    if (rc != 0) {
        expr_free(expr);
        exit(rc);
    }

    if (emit_code)
        gen_var_assign(ts, var_name, var_type, expr);

    expr_free(expr);
    parse_stmt_tail();
}

// Konec úseku upraveného s pomocí AI

static data_type parse_opt_type(void) {
    if (tok.type == TOKEN_COLON) {
        advance();
        return parse_type();
    } else {
        return type_unknown;
    }
}

// Precedenční parser potřebuje znát i případný přednačtený token.
// Vrácený strom musí uvolnit ten, kdo funkci zavolal.
static ExprNode *parse_expression_wrapper(void) {
    ExprNode *expr = NULL;
    int result;

    if (has_lookahead == true) {
        result = parse_expression(ts, &tok, &lookahead, &expr);

        tok = lookahead;
        has_lookahead = false;
    } else {
        result = parse_expression(ts, NULL, &tok, &expr);
    }

    if (result != 0)
        exit(result);

    return expr;
}

// Prompt AI: "Vyraz muze stat jako prikaz. Pokud je posledni v tele funkce, je to
//          jeji navratova hodnota, jinde se vyhodnoti a vysledek se zahodi.
//          Zkontroluj typ podle pravidel navratove hodnoty ze zadani."
static void check_function_value(data_type value) {
    if (value == type_bool) {
        exit(6); // Hodnotu typu Boolean nelze vrátit.
    }
    if (current_return_type != type_unknown && current_return_type != type_unit && !symtable_type_compatible(current_return_type, value))
        exit(6);
}

static void parse_value_stmt(void) {
    ExprNode *expr = parse_expression_wrapper();
    data_type type = expr->type;

    parse_stmt_tail();

    if (block_depth == 1 && tok.type == TOKEN_DEDENT) {
        check_function_value(type);
        if (emit_code)
            gen_func_return(expr);
    } else if (emit_code) {
        process_expr_node(expr);

        // Výsledek samostatného výrazu se zahodí.
        gen_inst1("POPS", "GF@%tmp_res");
    }

    expr_free(expr);
    stmt_value = type;
}

static void parse_opt_return_tail(void) {
    // Příkaz return vyžaduje explicitně uvedený návratový typ.
    if (!has_explicit_type)
        exit(6);

    if (tok.type == TOKEN_EOL || tok.type == TOKEN_SEMICOLON || tok.type == TOKEN_DEDENT || tok.type == TOKEN_EOF) {
        if (current_return_type != type_unit)
            exit(6);

        if (emit_code)
            gen_func_return(NULL);
        parse_stmt_tail();
        return;
    } else if (tok.type == TOKEN_IDENTIFIER || is_non_id_expression(tok.type)) {
        ExprNode *expr = NULL;
        data_type expr_type = parse_call_or_exp(&expr);

        check_function_value(expr_type);

        if (emit_code)
            gen_func_return(expr);
        expr_free(expr);

        parse_stmt_tail();
        return;
    }

    syntax_error();
}

// Vrátí typ výsledku těla funkce.
static data_type parse_function_body(void) {
    if (tok.type == TOKEN_EOL) {
        advance();
        parse_block(false);

        if (block_returns)
            return current_return_type;
        if (current_return_type == type_unit)
            return type_unit;
        if (block_value == type_unknown) {
            exit(6); // if s hodnotami ve větvích není výraz.
        }
        check_function_value(block_value);
        return block_value;
    } else if (tok.type == TOKEN_IDENTIFIER || is_non_id_expression(tok.type)) {
        ExprNode *expr = NULL;
        data_type expr_type = parse_call_or_exp(&expr);

        check_function_value(expr_type);

        if (emit_code)
            gen_func_return(expr);

        expr_free(expr);
        if (tok.type != TOKEN_EOF)
            expect(TOKEN_EOL);
        return expr_type;
    } else {
        syntax_error();
    }
}

// --- Zpracování příkazů ---

static int is_stmt_start(TokenType t) {
    if (t == TOKEN_KW_VAR || t == TOKEN_KW_VAL || t == TOKEN_IDENTIFIER || is_non_id_expression(t) == 1 || t == TOKEN_KW_RETURN || t == TOKEN_KW_IF || t == TOKEN_KW_WHILE) {
        return 1;
    } else {
        return 0;
    }
}

static void parse_stmt(void) {
    stmt_returns = false;
    stmt_value = type_unit;

    // Samostatný výrazový příkaz nesmí začínat unárním plus.
    if (tok.type == TOKEN_PLUS)
        syntax_error();

    if (tok.type == TOKEN_KW_VAR || tok.type == TOKEN_KW_VAL) {
        bool is_var = (tok.type == TOKEN_KW_VAR);
        advance();
        if (tok.type != TOKEN_IDENTIFIER)
            syntax_error();

        char *var_name = strdup(tok.attribute.s_val); // vlastní kopie jména před advance()

        if (var_name == NULL)
            syntax_error();

        expect(TOKEN_IDENTIFIER);
        data_type var_type = parse_opt_type();
        expect(TOKEN_ASSIGN);
        ExprNode *expr = NULL;
        data_type expr_type = parse_call_or_exp(&expr);

        int rc = sem_declare_variable(ts, var_name, var_type, expr_type, is_var, NULL);

        if (rc != 0) {
            free(var_name);
            exit(rc);
        }

        if (emit_code)
            gen_def_var(ts, var_name, var_type, expr);

        expr_free(expr);

        free(var_name);
        parse_stmt_tail();
    } else if (tok.type == TOKEN_KW_WHILE) {
        advance();
        ExprNode *expr = parse_expression_wrapper();

        int rc = sem_check_condition(expr);
        if (rc != 0) {
            expr_free(expr);
            exit(rc);
        }

        expect(TOKEN_KW_DO);
        expect(TOKEN_EOL);

        if (emit_code)
            gen_while_start(expr);
        expr_free(expr);

        parse_block(true);
        if (emit_code)
            gen_while_end();

        // Hodnoty z těla cyklu se do nadřazeného bloku nepřenášejí.
        stmt_returns = false;
        stmt_value = type_unit;
    } else if (tok.type == TOKEN_KW_RETURN) {
        advance();
        parse_opt_return_tail();
        stmt_returns = true;
    } else if (tok.type == TOKEN_KW_IF) {
        advance();
        ExprNode *expr = parse_expression_wrapper();

        int rc = sem_check_condition(expr);
        if (rc != 0) {
            expr_free(expr);
            exit(rc);
        }

        expect(TOKEN_KW_THEN);
        expect(TOKEN_EOL);

        if (emit_code)
            gen_if_start(expr);
        expr_free(expr);

        parse_block(true);
        bool then_returns = block_returns;
        data_type then_value = block_value;

        expect(TOKEN_KW_ELSE);
        expect(TOKEN_EOL);
        if (emit_code)
            gen_if_else();

        parse_block(true);
        bool else_returns = block_returns;
        data_type else_value = block_value;

        if (emit_code)
            gen_if_end();

        // Příkaz if zaručeně vrací pouze tehdy,
        // když vracejí obě větve.
        stmt_returns = then_returns && else_returns;

        // if není výraz, bez return smějí mít větve jen výsledek Unit.
        stmt_value =
            ((then_returns || then_value == type_unit) && (else_returns || else_value == type_unit))
                ? type_unit
                : type_unknown;
    } else if (tok.type == TOKEN_IDENTIFIER) {
        // Vše kromě přiřazení zpracujeme jako výraz nebo volání funkce.
        if (peek_token() != TOKEN_ASSIGN) {
            parse_value_stmt();
        } else {
            char *var_name = strdup(tok.attribute.s_val);
            if (var_name == NULL)
                exit(99);

            advance();
            parse_stmt_tail_id(var_name);

            free(var_name);
        }
    } else if (is_non_id_expression(tok.type)) {
        parse_value_stmt();
    } else
        syntax_error();
}

static void parse_stmts(void) {
    while (is_stmt_start(tok.type))
        parse_stmt();
}