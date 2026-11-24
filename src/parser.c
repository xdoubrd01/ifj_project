#include "parser.h"
#include "code_generator.h"
#include "expr_tree.h"
#include "scanner.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "symtable.h"
#include "exp_parser.h"
#include "semantic.h"
#include "exp_parser.h"
#include "semantic.h"


// Aktualni token a pripadny prednacteny token ze scanneru.
static Token tok; // current token
static Token lookahead;
static bool has_lookahead = false;
static SymTable *ts = NULL;
static data_type current_return_type = type_unknown;
static char *entry_function_name = NULL;
static bool stmt_returns = false;
static bool block_returns = false;
// AI: update
static data_type stmt_value = type_unit;  // hodnota posledniho prikazu
static data_type block_value = type_unit; // hodnota posledniho prikazu bloku
static int block_depth = 0;               // 1 = primo telo funkce
static bool has_explicit_type = false;    // hlavicka ma uvedeny navratovy typ
static bool emit_code = false;            // kod generuje az posledni pruchod

// prompt: "Navratovy typ funkce bez anotace se odvozuje z jejiho tela. Priprav
//          jednoduchy graf volani z prvniho pruchodu, abychom poznali rekurzi
//          a vedeli, v jakem poradi typy odvozovat."
typedef struct {
    char *name;
    bool typed;
    char **callees;      // jmena volanych funkci z prvniho pruchodu
    size_t *callee_ids;  // jejich indexy, doplni resolve_callees()
    size_t callee_count;
} FuncInfo;

static FuncInfo *func_infos = NULL;
static size_t func_info_count = 0;
static size_t func_info_pos = 0; // funkce jsou v kazdem pruchodu ve stejnem poradi


typedef enum {
    FIRST_ROUND,
    INFER_ROUND, // AI: update
    SECOND_ROUND
} ParserRound;


/*
 * AI-generated segment (Claude), prompt: "vygeneruj mi kostru vsech funkci
 * pravidel" (skeleton only: one empty function per grammar rule written by
 * the author; the bodies are to be implemented by the author).
 */

// Deklarace internich funkci parseru (implementace jsou nize).
// Prace s tokeny
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

// Parametry, typy a pomocna pole
static void append_param(data_type **types, size_t *count, data_type type);
static void append_param_name(char ***names, size_t count, char *name);
static data_type parse_parameters_start(char **out_name);
static void parse_parameters_tail(data_type **types, char ***names, size_t *count);
static void parse_parameters_list(data_type **types, char ***names, size_t *count);
static data_type parse_type(void);
static data_type parse_opt_type(void);
static data_type parse_opt_return_type(void);

// Bloky, prikazy a navratove hodnoty
static void parse_block(bool new_scope);
static void parse_stmts(void);
static void parse_stmt(void);
static void parse_stmt_tail(void);
static void parse_stmt_tail_id(char *var_name);
static void parse_opt_return_tail(void);
static int is_stmt_start(TokenType t);

// Vyrazy a volani funkci
static int is_non_id_expression(TokenType t);
static data_type parse_call_or_exp(ExprNode **out_expr);
static ExprNode *parse_expression_wrapper(void);
static void check_function_value(data_type value);
static void parse_value_stmt(void);


// Pri posunu nejdriv uvolnime text stareho tokenu.
// Pokud uz jsme token nacetli pres peek_token(), pouzijeme ho.
static void advance(void) {
    
    if (tok.type == TOKEN_IDENTIFIER ||
        tok.type == TOKEN_STRING_LITERAL) {
        free(tok.attribute.s_val);
    }

    // Uz prednacteny token
    if (has_lookahead) {
        tok = lookahead;
        has_lookahead = false;
    }
    // Jinak cteme primo ze scanneru
    else {
        int rc = get_next_token(&tok);

        if (rc != 0) {
            exit(rc);
        }
    }
}


_Noreturn void syntax_error(void) {
    fprintf(stderr, "Syntax error: unexpected token: %d\n", tok.type);

    exit(2);
    
}


static void expect(TokenType type) {
    if (tok.type != type) {
        syntax_error();
    }
    advance();
}


static void parse_prolog(void) {
    expect(TOKEN_KW_IMPORT);
    if (tok.type == TOKEN_IDENTIFIER && strcmp(tok.attribute.s_val, "ifj26") == 0) {
        advance();
        expect(TOKEN_DOT);
        expect(TOKEN_MUL);
        expect(TOKEN_EOL);
    }

    else {
        syntax_error();
    }
}

// Nacte dalsi token dopredu, ale zatim ho nepreda jako aktualni.
static TokenType peek_token(void){
    if (has_lookahead == false){
        int succes = get_next_token(&lookahead);

        if(succes != 0) exit(succes);

        has_lookahead = true;
    }
    return lookahead.type;
}


static void parse_program(ParserRound round) {
    parse_prolog();
    parse_functions(round);
    expect(TOKEN_EOF);
}

// prompt: "Pridej male pomocne funkce nad grafem volani: preved jmena volanych
//          funkci na indexy, zjisti primou i neprimou rekurzi a over, jestli
//          uz maji volane funkce znamy navratovy typ. At je to rychle i pro
//          velmi dlouhe programy."
static int compare_func_ids(const void *a, const void *b) {
    return strcmp(func_infos[*(const size_t *)a].name, func_infos[*(const size_t *)b].name);
}

static int compare_name_to_id(const void *name, const void *id) {
    return strcmp((const char *)name, func_infos[*(const size_t *)id].name);
}

// Vestavene a nedefinovane funkce se do grafu nepridavaji.
static void resolve_callees(void) {
    size_t *order = malloc(func_info_count * sizeof(size_t));
    if (order == NULL) exit(99);

    for (size_t i = 0; i < func_info_count; i++) {
        order[i] = i;
    }
    qsort(order, func_info_count, sizeof(size_t), compare_func_ids);

    for (size_t i = 0; i < func_info_count; i++) {
        FuncInfo *info = &func_infos[i];
        size_t kept = 0;

        if (info->callee_count > 0) {
            info->callee_ids = malloc(info->callee_count * sizeof(size_t));
            if (info->callee_ids == NULL) exit(99);
        }
        for (size_t j = 0; j < info->callee_count; j++) {
            size_t *found = bsearch(info->callees[j], order, func_info_count,
                                    sizeof(size_t), compare_name_to_id);
            if (found != NULL) {
                info->callee_ids[kept++] = *found;
            }
            free(info->callees[j]);
        }
        free(info->callees);
        info->callees = NULL;
        info->callee_count = kept;
    }
    free(order);
}

// true, pokud se z funkce start lze volanim vratit zpet do ni.
static bool is_recursive(size_t start, size_t *stack, size_t *visited) {
    size_t top = 0;
    stack[top++] = start;
    visited[start] = start + 1;

    while (top > 0) {
        const FuncInfo *info = &func_infos[stack[--top]];

        for (size_t i = 0; i < info->callee_count; i++) {
            size_t next = info->callee_ids[i];
            if (next == start) {
                return true;
            }
            if (visited[next] != start + 1) {
                visited[next] = start + 1;
                stack[top++] = next;
            }
        }
    }
    return false;
}

// Primo i neprimo rekurzivni funkce musi mit uvedeny navratovy typ.
static void check_inferred_recursion(void) {
    size_t *stack = malloc(func_info_count * sizeof(size_t));
    size_t *visited = calloc(func_info_count, sizeof(size_t));
    if (stack == NULL || visited == NULL) exit(99);

    for (size_t i = 0; i < func_info_count; i++) {
        if (!func_infos[i].typed && is_recursive(i, stack, visited)) {
            exit(6);
        }
    }
    free(stack);
    free(visited);
}

static bool has_unresolved_function(void) {
    for (size_t i = 0; i < func_info_count; i++) {
        if (!func_infos[i].typed) {
            return true;
        }
    }
    return false;
}

static bool callees_resolved(const FuncInfo *info) {
    for (size_t i = 0; i < info->callee_count; i++) {
        if (!func_infos[info->callee_ids[i]].typed) {
            return false;
        }
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

// prompt: "Dalsi pruchod musi zacit znovu od zacatku vstupu, priprav na to
//          jednu funkci, at se stejny kod neopakuje."
static void restart_scanner(const char *input, size_t length) {
    if (has_lookahead) {
        if (lookahead.type == TOKEN_IDENTIFIER || lookahead.type == TOKEN_STRING_LITERAL) {
            free(lookahead.attribute.s_val);
        }
        has_lookahead = false;
    }
    scanner_free();
    scanner_init_buffer(input, length);
    func_info_pos = 0;
    advance();
}

// Prvni pruchod si ulozi hlavicky funkci, aby slo volat i funkce
// definovane az pozdeji. Ve druhem se zpracovavaji jejich tela.
// AI: update
// AI ASSISTED START
// AI PROMPT: Umoznovat dva pruchody nad pametovym bufferem bez rewind a fmemopen.
void parse(const char *input, size_t length) {
    ts = symtable_create(0);
    if(ts == NULL) exit(99);

    if (!symtable_insert_builtins(ts)) {
        exit(99);
    }

    // prvni pruchod
    scanner_init_buffer(input, length);
    advance();
    parse_program(FIRST_ROUND);

    // druhy pruchod
   if (entry_function_name == NULL) {
    exit(3);
    }
    resolve_callees();
    check_inferred_recursion();

    // Kazde kolo odvodi funkce, jejichz volane funkce uz typ maji.
    // Cykly vyloucila kontrola rekurze, takze smycka vzdy skonci.
    while (has_unresolved_function()) {
        restart_scanner(input, length);
        parse_program(INFER_ROUND);
    }

    symbol_data *entry = symtable_search_func(ts, entry_function_name);
    if (entry == NULL || entry->data.func.ret_type != type_unit) {
        exit(6);
    }

    // Tabulku symbolu nechavame pro posledni pruchod.
    restart_scanner(input, length);
    code_gen_init(entry_function_name);
    emit_code = true;
    parse_program(SECOND_ROUND);

    scanner_free();
    free_func_infos();
}
// AI ASSISTED END


static void parse_functions(ParserRound round) {
    while(tok.type == TOKEN_EOL){
        advance();
    }

    if (tok.type == TOKEN_KW_DEF || tok.type == TOKEN_AT_MAIN){
    parse_function(round);
    parse_functions(round);
    }
}


static void parse_opt_annotation(void) {
    if (tok.type == TOKEN_AT_MAIN){
        advance();
        parse_opt_eol();
    }
}
        
 

/* 7. OPT_EOL -> EOL
 * 8. OPT_EOL -> eps */
static void parse_opt_eol(void) {
    if (tok.type == TOKEN_EOL)
        advance();
}

/*
 * AI-generated code (ChatGPT).
 * Prompt: "udelej pomocnou funkci pro pridani jednoho datoveho typu
 * na konec dynamickeho pole pomoci realoc."
 */
static void append_param(data_type **types, size_t *count, data_type type) {
    data_type *tmp = realloc(*types, (*count + 1) * sizeof(data_type));

    if (tmp == NULL) {
        exit(99);
    }

    *types = tmp;
    (*types)[*count] = type;
    (*count)++;
}

// count uz pocita i nove pridavane jmeno.
static void append_param_name(char ***names, size_t count, char *name) {
    char **tmp = realloc(*names, count * sizeof(char *));

    if (tmp == NULL) {
        free(name);
        exit(99);
    }

    *names = tmp;
    (*names)[count - 1] = name;
}

// Jmeno parametru kopirujeme, protoze text aktualniho tokenu
// se pri advance() uvolni. Kopii potom prevezme volajici.
static data_type parse_parameters_start(char **out_name){

    if (tok.type != TOKEN_IDENTIFIER) {
    syntax_error();
    }

    char *param_name = strdup(tok.attribute.s_val);

    if (param_name == NULL) {
        exit(99);
    }

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


// AI: update
static void parse_parameters_tail(
    data_type **types,
    char ***names,
    size_t *count
) {
    if (tok.type == TOKEN_COMMA) {
        advance();

        // koncova carka jen pred ')' na dalsim radku
        if (tok.type == TOKEN_RPAREN && tok.newline_before) {
            return;
        }

        char *name = NULL;
        data_type type = parse_parameters_start(&name);

        append_param(types, count, type);
        append_param_name(names, *count, name);

        parse_parameters_tail(types, names, count);
    }
}


static void parse_parameters_list(
    data_type **types,
    char ***names,
    size_t *count
) {
    if (tok.type == TOKEN_IDENTIFIER) {
        char *name = NULL;
        data_type type = parse_parameters_start(&name);

        append_param(types, count, type);
        append_param_name(names, *count, name);

        parse_parameters_tail(types, names, count);
    }
}


static data_type parse_opt_return_type(void){
    if(tok.type == TOKEN_COLON){
        advance();
        data_type ret_type = parse_type();

        return ret_type;
    }

    return type_unknown;
}

// prompt: "Pri preskakovani tela si zapamatuj, ktere funkce se v nem volaji."
static void add_callee(FuncInfo *info, const char *name) {
    char **grown = realloc(info->callees, (info->callee_count + 1) * sizeof(char *));
    if (grown == NULL) exit(99);
    info->callees = grown;
    info->callees[info->callee_count] = strdup(name);
    if (info->callees[info->callee_count] == NULL) exit(99);
    info->callee_count++;
}

// Posune se o token; identifikator pred '(' zapise jako volanou funkci.
static void skip_token(FuncInfo *info) {
    if (info != NULL && tok.type == TOKEN_IDENTIFIER && peek_token() == TOKEN_LPAREN) {
        add_callee(info, tok.attribute.s_val);
    }
    advance();
}

// V prvnim pruchodu preskocime telo. U vice radku sledujeme
// zanoreni, aby nas nezastavil DEDENT vnitrniho bloku.
// AI: update
static void skip_function_body(FuncInfo *info){
    if(tok.type == TOKEN_EOL){
        int depth = 0;
        advance();
        expect(TOKEN_INDENT);
        depth++;

        while(depth > 0){
            if(tok.type == TOKEN_EOF) syntax_error();
            if(tok.type == TOKEN_INDENT) depth ++;
            if(tok.type == TOKEN_DEDENT) depth --;
             skip_token(info);
        }
    }

    else {
        while (tok.type != TOKEN_EOL && tok.type != TOKEN_EOF) {
            skip_token(info);
        }

        if (tok.type == TOKEN_EOL) {
            advance();
        }
    }
}


// V obou pruchodech nacteme hlavicku funkce, ale registrujeme ji
// jen v prvnim. Jeji telo se kontroluje az ve druhem.
// AI: update
static void parse_function(ParserRound round) {
    bool is_entry = (tok.type == TOKEN_AT_MAIN);

    parse_opt_annotation();
    expect(TOKEN_KW_DEF);

    if (tok.type != TOKEN_IDENTIFIER) {
    syntax_error();
    }

    char *func_name = strdup(tok.attribute.s_val);

    if (func_name == NULL) {
        exit(99);
    }

    expect(TOKEN_IDENTIFIER);

    if (symtable_enter_scope(ts) == false) exit(99);

    data_type *param_types = NULL;
    char **param_names = NULL;
    size_t param_count = 0;
    expect(TOKEN_LPAREN);
    parse_parameters_list(&param_types, &param_names, &param_count);
    expect(TOKEN_RPAREN);

    data_type ret_type = parse_opt_return_type();

    if (round == FIRST_ROUND && is_entry) {
    if (entry_function_name != NULL) {
        exit(4);
    }

    if (param_count != 0) {
        exit(5);
    }

    // bez uvedeneho typu se Unit overi po odvozeni
    if (ret_type != type_unit && ret_type != type_unknown) {
        exit(6);
    }

    entry_function_name = strdup(func_name);
    if (entry_function_name == NULL) {
        exit(99);
    }
}

    if(round == FIRST_ROUND){
        /*
    * AI-generated code (ChatGPT).
    * Prompt: "vygeneruj kod pro registraci funkce do tabulky symbolu
    * pomoci symtable_insert_func(), vcetne kontroly uspesnosti vlozeni
    * a uvolneni alokovane pameti pri chybe."
    */
        bool success = symtable_insert_func(
            ts,
            func_name,
            ret_type,
            param_count,
            param_types
        );

        if (!success) {
            free(func_name);
            free(param_types);
            exit(4);
        }

        // Jmena musi byt ve stejnem poradi jako typy parametru.
        if (!symtable_set_func_param_names(ts,func_name,param_count,(const char *const *)param_names)) {
            exit(99);
        }

        // zaznam pro graf volani
        FuncInfo *grown = realloc(func_infos, (func_info_count + 1) * sizeof(FuncInfo));
        if (grown == NULL) exit(99);
        func_infos = grown;
        func_infos[func_info_count].name = strdup(func_name);
        if (func_infos[func_info_count].name == NULL) exit(99);
        func_infos[func_info_count].typed = (ret_type != type_unknown);
        func_infos[func_info_count].callees = NULL;
        func_infos[func_info_count].callee_ids = NULL;
        func_infos[func_info_count].callee_count = 0;
        func_info_count++;
    }
    expect(TOKEN_ASSIGN);

    FuncInfo *info = &func_infos[func_info_pos++];

    if(round == FIRST_ROUND){
        skip_function_body(info);
    }

    else if (round == INFER_ROUND) {
        if (info->typed || !callees_resolved(info)) {
            skip_function_body(NULL);
        }
        else {
            has_explicit_type = false;
            current_return_type = type_unknown;

            data_type inferred = parse_function_body();

            if (!symtable_set_func_return_type(ts, func_name, inferred)) exit(99);
            info->typed = true;
        }
    }

    else {
    has_explicit_type = (ret_type != type_unknown);
    // u funkce bez anotace je v tabulce uz odvozeny typ
    ret_type = symtable_search_func(ts, func_name)->data.func.ret_type;
    current_return_type = ret_type;

    gen_func_start(ts, func_name, ret_type);

    parse_function_body();

    gen_func_end();

    current_return_type = type_unknown;
}

    symtable_leave_scope(ts);

    // Tabulka symbolu si jmena kopiruje, docasne pole uz nepotrebujeme.
    for (size_t i = 0; i < param_count; i++) {
        free(param_names[i]);
    }

    free(param_names);
    free(param_types);
    free(func_name);
}

/* 10. BLOCK -> INDENT STMT STMTS DEDENT */

// AI: update
static void parse_block(bool new_scope) {
    expect(TOKEN_INDENT);

    // telo funkce sdili rozsah s parametry
    if (new_scope && !symtable_enter_scope(ts)) {
        exit(99);
    }
    block_depth++;

    parse_stmt();
    parse_stmts();

    // Vysledek bloku urcuje jen jeho posledni prikaz.
    bool last_returns = stmt_returns;
    data_type last_value = stmt_value;

    expect(TOKEN_DEDENT);

    block_depth--;
    if (new_scope) {
        symtable_leave_scope(ts);
    }
    block_returns = last_returns;
    block_value = last_value;
}



// AI: update
static data_type parse_type(void) {
    if(tok.type == TOKEN_TYPE_INT) {
        expect(TOKEN_TYPE_INT);
        return type_int;
    }
    else if (tok.type == TOKEN_TYPE_DOUBLE){
        expect(TOKEN_TYPE_DOUBLE);
        return type_double;
    }
    else if (tok.type == TOKEN_TYPE_STRING) {
        expect(TOKEN_TYPE_STRING);
        return type_string;
    }
    else if (tok.type == TOKEN_TYPE_UNIT){
        expect(TOKEN_TYPE_UNIT);
        return type_unit;
    }
    else{
        // identifikator na miste typu je nedefinovany typ
        if (tok.type == TOKEN_IDENTIFIER) {
            exit(3);
        }
        syntax_error();
        exit(2);
    }
}

// AI: update
static int is_non_id_expression(TokenType t){
    if (t == TOKEN_STRING_LITERAL || t == TOKEN_DOUBLE_LITERAL || t == TOKEN_INT_LITERAL || t == TOKEN_MINUS || t == TOKEN_PLUS || t == TOKEN_LPAREN ){
        return 1;
    }
    else{
        return 0;
    }

}


// Volani funkce i ostatni vyrazy zpracuje precedencni parser.
// AI-assisted (ChatGPT): pridani vystupniho parametru pro ExprNode.
// AI: update
static data_type parse_call_or_exp(ExprNode **out_expr) {
    *out_expr = NULL;

    if (tok.type == TOKEN_IDENTIFIER || is_non_id_expression(tok.type)) {
        ExprNode *expr = parse_expression_wrapper();
        if (expr == NULL) exit(99);

        *out_expr = expr;
        return expr->type;
    }

    syntax_error();
}

// AI: update
static void parse_stmt_tail(void){
    if (tok.type == TOKEN_SEMICOLON) {
        advance();
        if (tok.type == TOKEN_EOL) advance();
    }
    else if (tok.type == TOKEN_EOL) advance();
    else if(tok.type == TOKEN_DEDENT || tok.type == TOKEN_EOF) return;
    else syntax_error();
}



// AI ASSISTED START
// AI PROMPT:
// Oprav parse_stmt_tail_id tak, aby po identifikatoru
// vyzadovala platne prirazeni. Odmitni neplatne
// samostatne identifikatory a zachovej semantickou
// kontrolu i generovani prirazeni.

// AI: update
static void parse_stmt_tail_id(char *var_name) {
    if (tok.type != TOKEN_ASSIGN) {
        syntax_error();
    }

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

    if (emit_code) gen_var_assign(ts, var_name, var_type, expr);

    expr_free(expr);
    parse_stmt_tail();
}

// AI ASSISTED END


/* 19. OPT_TYPE -> COLON TYPE
 * 20. OPT_TYPE -> eps */
static data_type parse_opt_type(void) {
    if(tok.type == TOKEN_COLON){
        advance();
        return parse_type();
    }
    else{

    return type_unknown;
    }
}

// Precedencni parser potrebuje znat i pripadne prednacteny token.
// Vraceny strom musi uvolnit ten, kdo funkci zavolal.
static ExprNode *parse_expression_wrapper(void) {
    ExprNode *expr = NULL;
    int result;

    if (has_lookahead == true) {
        result = parse_expression(ts, &tok, &lookahead, &expr);

        tok = lookahead;
        has_lookahead = false;
    }
    else {
        result = parse_expression(ts, NULL, &tok, &expr);
    }

    if (result != 0) {
        exit(result);
    }

    return expr;
}

// prompt: "Vyraz muze stat jako prikaz. Pokud je posledni v tele funkce, je to
//          jeji navratova hodnota, jinde se vyhodnoti a vysledek se zahodi.
//          Zkontroluj typ podle pravidel navratove hodnoty ze zadani."
static void check_function_value(data_type value) {
    if (value == type_bool) {
        exit(6); // Boolean nelze vratit
    }
    if (current_return_type != type_unknown && current_return_type != type_unit &&
        !symtable_type_compatible(current_return_type, value)) {
        exit(6);
    }
}

static void parse_value_stmt(void) {
    ExprNode *expr = parse_expression_wrapper();
    data_type type = expr->type;

    parse_stmt_tail();

    if (block_depth == 1 && tok.type == TOKEN_DEDENT) {
        check_function_value(type);
        if (emit_code) gen_func_return(expr);
    }
    else if (emit_code) {
        process_expr_node(expr);

        // Vysledek samostatneho vyrazu se zahodi.
        gen_inst1("POPS", "GF@%tmp_res");
    }

    expr_free(expr);
    stmt_value = type;
}

// AI: update
static void parse_opt_return_tail(void) {
    // return vyzaduje explicitne uvedeny navratovy typ
    if (!has_explicit_type) {
        exit(6);
    }

    if (tok.type == TOKEN_EOL || tok.type == TOKEN_SEMICOLON || tok.type == TOKEN_DEDENT || tok.type == TOKEN_EOF) {

        if (current_return_type != type_unit) {
            exit(6);
        }

        if (emit_code) gen_func_return(NULL);
        parse_stmt_tail();
        return;
    }

    else if (tok.type == TOKEN_IDENTIFIER ||
             is_non_id_expression(tok.type)) {

        ExprNode *expr = NULL;
        data_type expr_type = parse_call_or_exp(&expr);

        check_function_value(expr_type);

        if (emit_code) gen_func_return(expr);
        expr_free(expr);

        parse_stmt_tail();
        return;
    }

    syntax_error();
}

// Vrati typ vysledku tela funkce.
// AI: update
static data_type parse_function_body(void){
    if (tok.type == TOKEN_EOL) {
        advance();
        parse_block(false);

        if (block_returns) {
            return current_return_type;
        }
        if (current_return_type == type_unit) {
            return type_unit;
        }
        if (block_value == type_unknown) {
            exit(6); // if s hodnotami ve vetvich neni vyraz
        }
        check_function_value(block_value);
        return block_value;
}

    else if (tok.type == TOKEN_IDENTIFIER || is_non_id_expression(tok.type)) {
    ExprNode *expr = NULL;
    data_type expr_type = parse_call_or_exp(&expr);

    check_function_value(expr_type);

    if (emit_code) gen_func_return(expr);

    expr_free(expr);
    if (tok.type != TOKEN_EOF) {
        expect(TOKEN_EOL);
    }
    return expr_type;
    }

    else{
        syntax_error();
    }
}

static int is_stmt_start(TokenType t){
    if(t == TOKEN_KW_VAR || t == TOKEN_KW_VAL || t == TOKEN_IDENTIFIER || is_non_id_expression(t) == 1 || t == TOKEN_KW_RETURN || t == TOKEN_KW_IF || t == TOKEN_KW_WHILE){
        return 1;
    }
    else {
        return 0;
    }
}


// AI: update
static void parse_stmt(void) {
    stmt_returns = false;
    stmt_value = type_unit;

    // unarni plus patri jen dovnitr vyrazu, radek jim nemuze zacinat
    if (tok.type == TOKEN_PLUS) syntax_error();

    if(tok.type == TOKEN_KW_VAR || tok.type == TOKEN_KW_VAL){
        bool is_var =(tok.type == TOKEN_KW_VAR);
        advance();
        if(tok.type != TOKEN_IDENTIFIER) syntax_error();

        char *var_name = strdup(tok.attribute.s_val); // vlastni kopie jmena pred advance()

        if(var_name == NULL) syntax_error();
        
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

        if (emit_code) gen_def_var(ts, var_name, var_type, expr);


        expr_free(expr);

        free(var_name);  
        parse_stmt_tail();
    }

    else if(tok.type == TOKEN_KW_WHILE){
        advance();
        ExprNode *expr = parse_expression_wrapper();
        
        int rc = sem_check_condition(expr);
        if (rc != 0) {
            expr_free(expr);
            exit(rc);
        }
        
        expect(TOKEN_KW_DO);
        expect(TOKEN_EOL);

        if (emit_code) gen_while_start(expr);
        expr_free(expr);
        
        parse_block(true);
        if (emit_code) gen_while_end();

        // Hodnoty z tela cyklu se do nadrazeneho bloku neprenaseji.
        stmt_returns = false;
        stmt_value = type_unit;
    }

    else if(tok.type == TOKEN_KW_RETURN){
        advance();
        parse_opt_return_tail();
        stmt_returns = true;
    }

    else if(tok.type == TOKEN_KW_IF){
        advance();
        ExprNode *expr = parse_expression_wrapper();
        
        int rc = sem_check_condition(expr);
        if (rc != 0) {
            expr_free(expr);
            exit(rc);
        }
       
        expect(TOKEN_KW_THEN);
        expect(TOKEN_EOL);

        if (emit_code) gen_if_start(expr);
        expr_free(expr);

        parse_block(true);
        bool then_returns = block_returns;
        data_type then_value = block_value;

        expect(TOKEN_KW_ELSE);
        expect(TOKEN_EOL);
        if (emit_code) gen_if_else();

        parse_block(true);
        bool else_returns = block_returns;
        data_type else_value = block_value;

        if (emit_code) gen_if_end();

        // IF zarucene vraci pouze tehdy,
        // kdyz vraci obe vetve.
        stmt_returns = then_returns && else_returns;

        // if neni vyraz, bez return smi mit vetve jen vysledek Unit
        stmt_value = ((then_returns || then_value == type_unit) &&
                      (else_returns || else_value == type_unit))
                         ? type_unit
                         : type_unknown;
    }

    else if (tok.type == TOKEN_IDENTIFIER) {

    // Vse krome prirazeni je vyraz nebo volani.
    if (peek_token() != TOKEN_ASSIGN) {
        parse_value_stmt();
    }

    else {
        char *var_name = strdup(tok.attribute.s_val);
        if (var_name == NULL) exit(99);

            advance();
            parse_stmt_tail_id(var_name);

            free(var_name);
        }
    }

    
    // AI ASSISTED START
    // AI PROMPT:
    // oprav zpracovani samostatnych vyrazovych prikazu
    // v parse_stmt(). Misto jednoho tokenu zpracuj cely
    // vyraz pomoci existujiciho precedencniho parseru.
    // Vysledek vyrazu se nepouziva.

    else if (is_non_id_expression(tok.type)) {
        parse_value_stmt();
    }

    // AI ASSISTED END

    else syntax_error();

}

// AI: update
static void parse_stmts(void) {
    while (is_stmt_start(tok.type)){
        parse_stmt();
    }
}