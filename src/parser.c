#include "parser.h"
#include "scanner.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "symtable.h"
#include "../parser_semantics/exp_parser.h"
#include "../parser_semantics/semantic.h"

static Token tok; // current token
static Token lookahead;
static bool has_lookahead = false;
static SymTable *ts = NULL;

typedef enum {
    FIRST_ROUND,
    SECOND_ROUND
} ParserRound;


static void advance(void) {
    // Uvolnění aktuálního tokenu
    if (tok.type == TOKEN_IDENTIFIER ||
        tok.type == TOKEN_STRING_LITERAL) {
        free(tok.attribute.s_val);
    }

    // Pokud už máme načtený další token
    if (has_lookahead) {
        tok = lookahead;
        has_lookahead = false;
    }
    // Jinak načteme nový token ze scanneru
    else {
        int rc = get_next_token(&tok);

        if (rc != 0) {
            exit(rc);
        }
    }
}

void syntax_error(void) {
    fprintf(stderr, "Syntax error: unexpected token: %d\n", tok.type);

    exit(2);
    
}

static void expect(TokenType type) {
    if (tok.type != type) {
        syntax_error();
        scanner_free();
        exit(2);
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

/*
 * AI-generated segment (Claude), prompt: "vygeneruj mi kostru vsech funkci
 * pravidel" (skeleton only: one empty function per grammar rule written by
 * the author; the bodies are to be implemented by the author).
 *
 * Paste below your existing static helpers (advance, expect, syntax_error)
 * and keep your own parse_prolog().
 */

/* ---- forward declarations (one function per non-terminal) ---- */

static void parse_prolog(void);
static void parse_functions();
static void parse_opt_annotation(void);
static void parse_opt_eol(void);
static void parse_function(ParserRound round);
static void parse_block(void);
static data_type parse_type(void);
static void parse_stmts(void);
static void parse_stmt(void);
static data_type parse_opt_type(void);
static void parse_function_body(void);
static ExprNode *parse_expression_wrapper(void);

static TokenType peek_token(void){
    if (has_lookahead == false){
        int succes = get_next_token(&lookahead);

        if(succes != 0) exit(succes);

        has_lookahead = true;
    }
    return lookahead.type;
}

static void parse_program(void) {
    parse_prolog();
    parse_functions();
    expect(TOKEN_EOF);
}

void parse(void) {
    ts = symtable_create(0);
    if(ts == NULL) syntax_error();
    advance();
    parse_program();
    scanner_free(); 
}

static void parse_functions(void) {
    while(tok.type == TOKEN_EOL){
        advance();
    }

    if (tok.type == TOKEN_KW_DEF || tok.type == TOKEN_AT_MAIN){
    parse_function();
    parse_functions();
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

static data_type parse_parameters_start(){

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
    free(param_name);
    return param_type;
}

static void parse_parameters_tail(data_type **types, size_t *count){
    if(tok.type == TOKEN_COMMA){
        advance();
        data_type param_type = parse_parameters_start();
        append_param(types, count, param_type);
        parse_parameters_tail(types, count); 

    }
}

static void parse_parameters_list(data_type **types, size_t *count){

    if(tok.type == TOKEN_IDENTIFIER){
        data_type param_type = parse_parameters_start();
        append_param(types, count, param_type);
        parse_parameters_tail(types, count);
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
    size_t param_count = 0;
    expect(TOKEN_LPAREN);
    parse_parameters_list(&param_types, &param_count);
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
    }
    expect(TOKEN_ASSIGN);

    if(round == FIRST_ROUND){
        skip_function_body();
    }

    else{
        parse_function_body();
    }

    symtable_leave_scope(ts);

    free(func_name);
    free(param_types);
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

static void parse_call_or_exp_opt(void){
    if(tok.type == TOKEN_LPAREN){
        advance();
        parse_call_arguments();
        expect(TOKEN_RPAREN);
    }
    else parse_expr_tail();
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

static ExprNode *parse_call_or_exp(void) {
    if (tok.type == TOKEN_IDENTIFIER &&
        peek_token() == TOKEN_LPAREN) {

      char *function_name = strdup(tok.attribute.s_val);
      if (function_name == NULL) exit(99);

      symbol_data *function = symtable_search_func(ts, function_name);

      if (function == NULL) {
        fprintf(stderr, "Undefined function: %s\n", function_name);
        free(function_name);
        exit(3); 
    }

    } else if (tok.type == TOKEN_IDENTIFIER ||
               is_non_id_expression(tok.type)) {

        
        return parse_expression_wrapper();

    } else {
        syntax_error();
    }
}

static void parse_stmt_tail(void){
    if (tok.type == TOKEN_SEMICOL || tok.type == TOKEN_EOL) advance();
    else syntax_error();
}

static void parse_stmt_tail_id(void){
if(tok.type == TOKEN_ASSIGN){
    advance();
    parse_call_or_exp();
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


static ExprNode *parse_expression_wrapper(void) {
    ExprNode *expr = NULL;

    int result = parse_expression(ts, NULL, &tok, &expr);

    if (result != 0) {
        exit(result);
    }

    return expr;
}

static void parse_opt_return_tail(void){
if(tok.type == TOKEN_EOL || tok.type == TOKEN_SEMICOL){
    parse_stmt_tail();
}

else if(tok.type == TOKEN_IDENTIFIER || is_non_id_expression(tok.type) == 1){
    parse_call_or_exp();
    parse_stmt_tail();
    }
}

static void parse_function_body(void){
    if(tok.type == TOKEN_EOL){
        advance();
        parse_block();
    }

    else if(tok.type == TOKEN_IDENTIFIER || is_non_id_expression(tok.type)){
        parse_call_or_exp();
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

        char *var_name = strdup(tok.attribute.s_val); //copies s_val into own memmory

        if(var_name == NULL) syntax_error();
        
        expect(TOKEN_IDENTIFIER);
        data_type var_type = parse_opt_type();
       ExprNode *expr = parse_call_or_exp();

        if (expr == NULL) {
            exit(99);
        }

        data_type expr_type = expr->type;

        int rc = sem_declare_variable(
            ts,
            var_name,
            var_type,
            expr_type,
            is_var,
            NULL
        );

        if (rc != 0) {
            expr_free(expr);
            free(var_name);
            exit(rc);
        }

        expr_free(expr);
        free(var_name);

        parse_stmt_tail();
    }

    else if(tok.type == TOKEN_KW_WHILE){
        advance();
        parse_expression_wrapper();
        expect(TOKEN_KW_DO);
        expect(TOKEN_EOL);
        parse_block();

    }

    else if(tok.type == TOKEN_KW_RETURN){
        advance();
        parse_opt_return_tail();
    }

    else if(tok.type == TOKEN_KW_IF){
        advance();
        parse_expression_wrapper();
        expect(TOKEN_KW_THEN);
        expect(TOKEN_EOL);
        parse_block();
        expect(TOKEN_KW_ELSE);
        expect(TOKEN_EOL);
        parse_block();
    }

    else if(tok.type == TOKEN_IDENTIFIER){
        advance();
        parse_stmt_tail_id();
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