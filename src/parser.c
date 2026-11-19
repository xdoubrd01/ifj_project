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
static void parse_stmts(void);
static void parse_stmt(void);
static void parse_stmt_tail(void);
static void parse_stmt_tail_id(char *var_name);
static void parse_opt_return_tail(void);
static int is_stmt_start(TokenType t);

// Vyrazy a volani funkci
static void parse_term(void);
static void parse_expr_tail(void);
static void parse_call_arguments_tail(void);
static void parse_call_arguments(void);
static void parse_non_id_expression(void);
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
// source musi jit vratit na zacatek pomoci rewind().
void parse(FILE *source) {
    ts = symtable_create(0);
    if(ts == NULL) exit(99);

    if (!symtable_insert_builtins(ts)) {
        exit(99);
    }

    // prvni pruchod
    scanner_init(source);
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
    rewind(source);
    scanner_init(source);

    // druhy pruchod
    code_gen_init();
    advance();
    parse_program(SECOND_ROUND);
    
    scanner_free();
}


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
    parse_stmt();
    parse_stmts();
    expect(TOKEN_DEDENT);

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

/*code generated by gemini, prompt:  
"44. TERM             -> IDENTIFIER
 45. TERM             -> INT_LITERAL
 46. TERM             -> DOUBLE_LITERAL
 47. TERM             -> STRING_LITERAL
 48. TERM             -> LPAREN RPAREN udelej mi z toho fukci parse_term" */ 

static void parse_term(void) {
    if (tok.type == TOKEN_IDENTIFIER) {
        advance();
    } 
    else if (tok.type == TOKEN_INT_LITERAL) {
        advance();
    } 
    else if (tok.type == TOKEN_DOUBLE_LITERAL) {
        advance();
    } 
    else if (tok.type == TOKEN_STRING_LITERAL) {
        advance();
    } 
    else if (tok.type == TOKEN_LPAREN) {
        advance();
        expect(TOKEN_RPAREN);
    } 
    else {
        syntax_error();
    }
}

static void parse_expr_tail(void){

}

static void parse_call_arguments_tail(void){
 if (tok.type == TOKEN_COMMA) {
        advance();
        parse_term();
        parse_call_arguments_tail();
    }
}

static void parse_call_arguments(void){
   if (tok.type == TOKEN_IDENTIFIER || tok.type == TOKEN_INT_LITERAL || 
        tok.type == TOKEN_DOUBLE_LITERAL || tok.type == TOKEN_STRING_LITERAL || 
        tok.type == TOKEN_LPAREN) {
        
        parse_term();
        parse_call_arguments_tail();
    }
}

static void parse_non_id_expression(void){
  if (tok.type == TOKEN_DOUBLE_LITERAL ||
        tok.type == TOKEN_STRING_LITERAL || tok.type == TOKEN_INT_LITERAL) {
        advance();
    } else {
        syntax_error();
    }
}

static int is_non_id_expression(TokenType t){
    if (t == TOKEN_STRING_LITERAL || t == TOKEN_DOUBLE_LITERAL || t == TOKEN_INT_LITERAL || t == TOKEN_MINUS || t == TOKEN_LPAREN ){
        return 1;
    }
    else{
        return 0;
    }

}

// Kontrola poctu a typu argumentu oproti hlavicce funkce.
static data_type parse_function_call(void) {
    if(tok.type == TOKEN_IDENTIFIER){

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

        size_t arg_count = 0;
        if (tok.type != TOKEN_RPAREN) {
            ExprNode *arg = parse_expression_wrapper();
            if (arg == NULL) exit(99);

            data_type arg_type = arg->type;
        

            if (arg_count >= function->data.func.param_count) {
                free(function_name);
                expr_free(arg);
                exit(5);
            }

            data_type expected_type = function->data.func.params[arg_count];

            if (expected_type != type_any && arg_type != expected_type) {
                free(function_name);
                expr_free(arg);
                exit(5);
            }

            process_expr_node(arg);
            expr_free(arg);
            arg_count++;

            while (tok.type == TOKEN_COMMA) {
                advance(); 

        
                ExprNode *arg = parse_expression_wrapper();
                if (arg == NULL) exit(99);

                // proccess argument expression, and push to stack
                process_expr_node(arg);
                
                data_type type = arg->type;
                expr_free(arg);

                
                if (arg_count >= function->data.func.param_count) {
                    exit(5);
                }

                
                data_type expected = function->data.func.params[arg_count];

                if (expected != type_any && type != expected) {
                    exit(5);
                }

                arg_count++;
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
    
    else {
    syntax_error();
    }
}

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
    else syntax_error();
}

static void parse_stmt_tail_id(char *var_name){
if (tok.type == TOKEN_ASSIGN) {
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
        // TODO: prirazeni vysledku volani funkce
        // parse_call_or_exp() uz v tomto pripade generoval CALL.
        // Zde je potreba ulozit navratovou hodnotu do promenne.
    }

    expr_free(expr);
}

else if(tok.type == TOKEN_LPAREN){
    advance();
    parse_call_arguments();
    expect(TOKEN_RPAREN);
}

else{
    parse_expr_tail();
}

parse_stmt_tail();
}

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
    if (tok.type == TOKEN_EOL || tok.type == TOKEN_SEMICOLON) {

        if (current_return_type != type_unit &&
            current_return_type != type_unknown) {
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

        gen_func_return(expr);
        expr_free(expr);

        parse_stmt_tail();
        return;
    }

    syntax_error();
}

static void parse_function_body(void){
    if(tok.type == TOKEN_EOL){
        advance();
        parse_block();
    }

    else if(tok.type == TOKEN_IDENTIFIER || is_non_id_expression(tok.type)){
        ExprNode *expr = NULL;
        parse_call_or_exp(&expr);
        expr_free(expr);

        expect(TOKEN_EOL);
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
        if(expr != NULL){
        gen_def_var(ts, var_name, var_type, expr);

        }
        else{

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
    }

    else if(tok.type == TOKEN_KW_RETURN){
        advance();
        parse_opt_return_tail();
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
        expect(TOKEN_KW_ELSE);
        expect(TOKEN_EOL);
        gen_if_else();
        parse_block();
        gen_if_end();
    }

    else if(tok.type == TOKEN_IDENTIFIER){
        char *var_name = strdup(tok.attribute.s_val);
        if (var_name == NULL) exit(99);

        advance();
        parse_stmt_tail_id(var_name);
        free(var_name);
    }

    else if(is_non_id_expression(tok.type) == 1){
        parse_non_id_expression();
        parse_stmt_tail();
    }

    else syntax_error();

}


static void parse_stmts(void) {
    if(is_stmt_start(tok.type) == 1){
        parse_stmt();
        parse_stmts();
    }
}