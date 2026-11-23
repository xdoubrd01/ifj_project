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


typedef enum {
    FIRST_ROUND,
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
static void parse_function_body(void);
static void skip_function_body(void);

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
static void parse_block(void);
static bool parse_stmts(void);
static void parse_stmt(void);
static void parse_stmt_tail(void);
static void parse_stmt_tail_id(char *var_name);
static void parse_opt_return_tail(void);
static int is_stmt_start(TokenType t);

// Vyrazy a volani funkci
static int is_non_id_expression(TokenType t);
static data_type parse_function_call(void);
static data_type parse_call_or_exp(ExprNode **out_expr);
static ExprNode *parse_expression_wrapper(void);


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

// Prvni pruchod si ulozi hlavicky funkci, aby slo volat i funkce
// definovane az pozdeji. Ve druhem se zpracovavaji jejich tela.
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

    // priprava na druhy pruchod
    if (has_lookahead) {
        if (lookahead.type == TOKEN_IDENTIFIER ||lookahead.type == TOKEN_STRING_LITERAL) {
            free(lookahead.attribute.s_val);
        }
        has_lookahead = false;
    }
    // Tabulku symbolu nechavame pro druhy pruchod.
    scanner_free();
    scanner_init_buffer(input, length);

    // druhy pruchod
   if (entry_function_name == NULL) {
    exit(3);
    }

    // druhy pruchod
    advance();
    code_gen_init(entry_function_name);
    parse_program(SECOND_ROUND);
        
    scanner_free();
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


static void parse_parameters_tail(
    data_type **types,
    char ***names,
    size_t *count
) {
    if (tok.type == TOKEN_COMMA) {
        advance();

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

// V prvnim pruchodu preskocime telo. U vice radku sledujeme
// zanoreni, aby nas nezastavil DEDENT vnitrniho bloku.
static void skip_function_body(void){
    if(tok.type == TOKEN_EOL){
        int depth = 0;
        advance();
        expect(TOKEN_INDENT);
        depth++;

        while(depth > 0){
            if(tok.type == TOKEN_EOF) syntax_error();
            if(tok.type == TOKEN_INDENT) depth ++;
            if(tok.type == TOKEN_DEDENT) depth --;
             advance();
        }
    }

    else {
        while (tok.type != TOKEN_EOL && tok.type != TOKEN_EOF) {
            advance();
        }

        if (tok.type == TOKEN_EOL) {
            advance();
        }
    }
}


// V obou pruchodech nacteme hlavicku funkce, ale registrujeme ji
// jen v prvnim. Jeji telo se kontroluje az ve druhem.
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

    if (ret_type != type_unit) {
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
    }
    expect(TOKEN_ASSIGN);

    if(round == FIRST_ROUND){
        skip_function_body();
    }

    else {
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

static void parse_block(void) {
    expect(TOKEN_INDENT);

    if (!symtable_enter_scope(ts)) {
        exit(99);
    }

    parse_stmt();
    bool safe_return = stmt_returns;

    if(parse_stmts() == true) safe_return = true;
    
    expect(TOKEN_DEDENT);

    symtable_leave_scope(ts);
    block_returns = safe_return;
}



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
        
        syntax_error();
        exit(2);
    }
}

// AI ASSISTED START
// AI PROMPT:
// Zachovej puvodni pomocne funkce pro parsovani argumentu.
// Zpracovani skutecnych volani funkci resi parse_function_call().

// Preskoci pripadne layout tokeny uvnitr zavorek.
static void skip_call_layout(void) {
    while (tok.type == TOKEN_EOL ||
           tok.type == TOKEN_INDENT ||
           tok.type == TOKEN_DEDENT) {
        advance();
    }
}

// AI ASSISTED END

static int is_non_id_expression(TokenType t){
    if (t == TOKEN_STRING_LITERAL || t == TOKEN_DOUBLE_LITERAL || t == TOKEN_INT_LITERAL || t == TOKEN_MINUS || t == TOKEN_LPAREN ){
        return 1;
    }
    else{
        return 0;
    }

}


// AI ASSISTED START
// AI PROMPT:
// Rozsir parse_function_call() o vnorena volani funkci v argumentech,
// napr. print(answer()) a f(g(2)). Zachovej dosavadni podporu
// viceradkovych argumentu, kontrolu koncove carky podle
// newline_before, semantickou kontrolu typu a generovani argumentu.
// Povol implicitni konverzi Int -> Double u argumentu.
static data_type parse_function_call(void) {
    if (tok.type != TOKEN_IDENTIFIER) {
        syntax_error();
    }

    char *function_name = strdup(tok.attribute.s_val);
    if (function_name == NULL) exit(99);

    symbol_data *function = symtable_search_func(ts, function_name);
    if (function == NULL) {
        fprintf(stderr, "Undefined function: %s\n", function_name);
        free(function_name);
        exit(3);
    }

    advance();
    expect(TOKEN_LPAREN);
    skip_call_layout();

    size_t arg_count = 0;
    while (tok.type != TOKEN_RPAREN) {
        ExprNode *arg = NULL;
        data_type arg_type;

        // Vnorene volani se zpracuje rekurzivne a samo vlozi
        // navratovou hodnotu na datovy zasobnik.
        if (tok.type == TOKEN_IDENTIFIER && peek_token() == TOKEN_LPAREN) {
            arg_type = parse_function_call();
        } else {
            arg = parse_expression_wrapper();
            if (arg == NULL) exit(99);
            arg_type = arg->type;
        }

        if (arg_count >= function->data.func.param_count) {
            expr_free(arg);
            free(function_name);
            exit(5);
        }

        data_type expected_type = function->data.func.params[arg_count];
        if (expected_type != type_any && arg_type != expected_type &&
            !(expected_type == type_double && arg_type == type_int)) {
            expr_free(arg);
            free(function_name);
            exit(5);
        }

        // Bezny vyraz musi vygenerovat kod; vnorene volani uz
        // provedlo gen_funct_call() a hodnotu ma na zasobniku.
        if (arg != NULL) {
            process_expr_node(arg);
            expr_free(arg);
        }

        if (expected_type == type_double && arg_type == type_int) {
            gen_inst0("INT2FLOATS");
        }

        arg_count++;
        skip_call_layout();
        if (tok.type != TOKEN_COMMA) {
            break;
        }

        advance();
        skip_call_layout();

        // Scanner potlacuje EOL uvnitr zavorek; fyzicke zalomeni
        // radku pred ')' je dostupne v tok.newline_before.
        if (tok.type == TOKEN_RPAREN) {
            if (!tok.newline_before) {
                free(function_name);
                syntax_error();
            }
            break;
        }
    }

    expect(TOKEN_RPAREN);
    if (arg_count != function->data.func.param_count) {
        free(function_name);
        exit(5);
    }

    data_type return_type = function->data.func.ret_type;
    gen_funct_call(function_name);
    free(function_name);
    return return_type;
}
// AI ASSISTED END


// Volani funkce zatim resime zvlast, ostatni vyrazy pres precedencni parser.
// AI-assisted (ChatGPT): pridani vystupniho parametru pro ExprNode.
static data_type parse_call_or_exp(ExprNode **out_expr) {
    *out_expr = NULL;

    if (tok.type == TOKEN_IDENTIFIER && peek_token() == TOKEN_LPAREN) {
        return parse_function_call();
    }

    if (tok.type == TOKEN_IDENTIFIER || is_non_id_expression(tok.type)) {
        ExprNode *expr = parse_expression_wrapper();
        if (expr == NULL) exit(99);

        *out_expr = expr;
        return expr->type;
    }

    syntax_error();
}

static void parse_stmt_tail(void){
    if (tok.type == TOKEN_SEMICOLON || tok.type == TOKEN_EOL) advance();
    else if(tok.type == TOKEN_DEDENT || tok.type == TOKEN_EOF) return;
    else syntax_error();
}



// AI ASSISTED START
// AI PROMPT:
// Oprav parse_stmt_tail_id tak, aby po identifikatoru
// vyzadovala platne prirazeni. Odmitni neplatne
// samostatne identifikatory a zachovej semantickou
// kontrolu i generovani prirazeni.

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

    if (expr != NULL) {
        gen_var_assign(ts, var_name, var_type, expr);
    } else {
        if (var_type == type_double && expr_type == type_int) {
            gen_inst0("INT2FLOATS");
        }

        gen_var_assign(ts, var_name, var_type, NULL);
    }

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

static void parse_opt_return_tail(void) {
    if (tok.type == TOKEN_EOL || tok.type == TOKEN_SEMICOLON || tok.type == TOKEN_DEDENT || tok.type == TOKEN_EOF) {

        if (current_return_type != type_unit && current_return_type != type_unknown) {
            exit(6);
        }

        gen_func_return(NULL);
        parse_stmt_tail();
        return;
    }

    else if (tok.type == TOKEN_IDENTIFIER ||
             is_non_id_expression(tok.type)) {

        ExprNode *expr = NULL;
        data_type expr_type = parse_call_or_exp(&expr);

        if (current_return_type != type_unknown && expr_type != current_return_type && !(current_return_type == type_double && expr_type == type_int)) {
            expr_free(expr);
            exit(6);
        }
        
         if (expr == NULL && current_return_type == type_double && expr_type == type_int) {
            gen_inst0("INT2FLOATS");
        }

        gen_func_return(expr);
        expr_free(expr);

        parse_stmt_tail();
        return;
    }

    syntax_error();
}

static void parse_function_body(void){
    if (tok.type == TOKEN_EOL) {
        advance();
        parse_block();

        if (current_return_type != type_unit &&
            current_return_type != type_unknown &&
            !block_returns) {
            exit(6);
        }
}

    else if (tok.type == TOKEN_IDENTIFIER || is_non_id_expression(tok.type)) {
    ExprNode *expr = NULL;
    data_type expr_type = parse_call_or_exp(&expr);

    if (current_return_type != type_unknown &&
        expr_type != current_return_type &&
        !(current_return_type == type_double && expr_type == type_int)) {
        expr_free(expr);
        exit(6);
    }


    if (expr != NULL) {
        gen_func_return(expr);

    } else {
        if(expr_type == type_int && current_return_type == type_double){
            gen_inst0("INT2FLOATS");
        }

        gen_func_return(NULL);
    }

    expr_free(expr);
    expect(TOKEN_EOL);
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


static void parse_stmt(void) {
    stmt_returns = false;

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

        if (expr != NULL) {
            gen_def_var(ts, var_name, var_type, expr);
        } 
        else {
            gen_def_var(ts, var_name, var_type, NULL);

            if (var_type == type_double && expr_type == type_int) {
                gen_inst0("INT2FLOATS");
            }

            gen_var_assign(ts, var_name, var_type, NULL);

        }


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

        gen_while_start(expr);
        expr_free(expr);
        
        parse_block();
        gen_while_end();

        stmt_returns = false;
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

        gen_if_start(expr);
        expr_free(expr);

        parse_block();
        bool then_returns = block_returns;

        expect(TOKEN_KW_ELSE);
        expect(TOKEN_EOL);
        gen_if_else();

        parse_block();
        bool else_returns = block_returns;

        gen_if_end();

        // IF zarucene vraci pouze tehdy,
        // kdyz vraci obe vetve.
        stmt_returns = then_returns && else_returns;
    }

    else if (tok.type == TOKEN_IDENTIFIER) {

    if (peek_token() == TOKEN_LPAREN) {
        parse_function_call();
        parse_stmt_tail();
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
        ExprNode *expr = parse_expression_wrapper();

        if (expr == NULL) {
            exit(99);
        }

        process_expr_node(expr);

        // Vysledek samostatneho vyrazu se zahodi.
        gen_inst1("POPS", "GF@%tmp_res");

        expr_free(expr);
        parse_stmt_tail();
    }

    // AI ASSISTED END

    else syntax_error();

}

static bool parse_stmts(void) {
    bool safe_return = false;
    
    while (is_stmt_start(tok.type)){
        parse_stmt();

        if(stmt_returns){
            safe_return = true;
        }
    }
    return safe_return;
}