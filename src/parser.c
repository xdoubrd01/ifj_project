#include "parser.h"
#include "scanner.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Token tok; // current token

static void advance(void) {
    if (tok.type == TOKEN_IDENTIFIER || tok.type == TOKEN_STRING_LITERAL) {
        free(tok.attribute.s_val);
    }
    int rc = get_next_token(&tok);
    if (rc != 0) {
        scanner_free();
        exit(rc); // 1 lexical error, 99 intern error
    }
}

void syntax_error() {
    fprintf(stderr, "Syntax error\n");
    scanner_free();
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
static void parse_functions(void);
static void parse_opt_annotation(void);
static void parse_opt_eol(void);
static void parse_function(void);
static void parse_block(void);
static void parse_type(void);
static void parse_stmts(void);
static void parse_stmt(void);
static void parse_opt_type(void);
/* expression analysis is Kuba's part; temporary stand-in until his module exists */
static void parse_expression(void);

/* 2. PROGRAM -> PROLOG FUNCTIONS EOF */
static void parse_program(void) {
    parse_prolog();
    parse_functions();
    expect(TOKEN_EOF);
}

void parse(void) {
    advance();
    parse_program();
    scanner_free(); 
}

static void parse_functions(void) {
    if (tok.type == TOKEN_KW_DEF || tok.type == TOKEN_AT){
    parse_function();
    parse_functions();
    }
}


/* 5. OPT_ANNOTATION -> AT IDENTIFIER OPT_EOL
 * 6. OPT_ANNOTATION -> eps
 * NOTE: the identifier must be "main" (check s_val, mind who frees it) */
static void parse_opt_annotation(void) {
    if (tok.type == TOKEN_AT){
        advance();
    if (tok.type == TOKEN_IDENTIFIER && strcmp(tok.attribute.s_val, "main") == 0) {
        advance();
        parse_opt_eol();
        
    }else {
        syntax_error();
        }
    }
}

/* 7. OPT_EOL -> EOL
 * 8. OPT_EOL -> eps */
static void parse_opt_eol(void) {
    if (tok.type == TOKEN_EOL)
        advance();
}

/* 9. FUNCTION -> OPT_ANNOTATION KW_DEF IDENTIFIER LPAREN RPAREN COLON TYPE ASSIGN EOL BLOCK */
static void parse_function(void) {
    parse_opt_annotation();
    expect(TOKEN_KW_DEF);
    expect(TOKEN_IDENTIFIER);
    expect(TOKEN_LPAREN);
    expect(TOKEN_RPAREN);
    expect(TOKEN_COLON);
    parse_type();
    expect(TOKEN_ASSIGN);
    expect(TOKEN_EOL);
    parse_block();
}

/* 10. BLOCK -> INDENT STMT STMTS DEDENT */
static void parse_block(void) {
    expect(TOKEN_INDENT);
    parse_stmt();
    parse_stmts();
    expect(TOKEN_DEDENT);

}

/* 11-14. TYPE -> TYPE_INT | TYPE_DOUBLE | TYPE_STRING | TYPE_UNIT */
static void parse_type(void) {
    if(tok.type == TOKEN_TYPE_INT) expect(TOKEN_TYPE_INT);
    else if (tok.type == TOKEN_TYPE_DOUBLE) expect(TOKEN_TYPE_DOUBLE);
    else if (tok.type == TOKEN_TYPE_STRING) expect(TOKEN_TYPE_STRING);
    else if (tok.type == TOKEN_TYPE_UNIT) expect(TOKEN_TYPE_UNIT);
    else syntax_error();
    
}

/* 15. STMTS -> STMT STMTS
 * 16. STMTS -> eps */
static void parse_stmts(void) {
    if(tok.type == TOKEN_KW_VAR || tok.type == TOKEN_KW_VAL ||tok.type == TOKEN_KW_WHILE){
        parse_stmt();
        parse_stmts();
    }
}

/* 17. STMT -> KW_VAR IDENTIFIER OPT_TYPE ASSIGN EXPRESSION EOL
 * 18. STMT -> KW_VAL IDENTIFIER OPT_TYPE ASSIGN EXPRESSION EOL
 * 21. STMT -> KW_WHILE EXPRESSION KW_DO EOL BLOCK */
static void parse_stmt(void) {
    if(tok.type == TOKEN_KW_VAR || tok.type == TOKEN_KW_VAL){
        advance();
        expect(TOKEN_IDENTIFIER);
        parse_opt_type();
        expect(TOKEN_ASSIGN);
        parse_expression();
        expect(TOKEN_EOL);
    }
    else if(tok.type == TOKEN_KW_WHILE){
        advance();
        parse_expression();
        expect(TOKEN_KW_DO);
        expect(TOKEN_EOL);
        parse_block();

    }
    else syntax_error();

}

/* 19. OPT_TYPE -> COLON TYPE
 * 20. OPT_TYPE -> eps */
static void parse_opt_type(void) {
    if(tok.type == TOKEN_COLON){
        advance();
        parse_type();
    }
}

/*  expression mockup*/
static void parse_expression(void) {
    if (tok.type == TOKEN_INT_LITERAL || tok.type == TOKEN_DOUBLE_LITERAL ||
        tok.type == TOKEN_STRING_LITERAL || tok.type == TOKEN_IDENTIFIER) {
        advance();
    } else {
        syntax_error();
    }
}