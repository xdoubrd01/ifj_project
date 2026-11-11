/*
 * IFJ26 compiler - token_dump.c
 * Temporary helper: prints all tokens produced by the scanner.
 * Authors: (doplnit login a jmeno)
 *
 * AI-generated segment (Claude), prompt: "kostra testovaciho mainu, ktery
 * vypise vsechny tokeny ze scanner.h".
 *
 * Build: gcc -Wall -Wextra -o token_dump token_dump.c scanner.c
 * Usage: ./token_dump < program.ifj
 */
#include <stdio.h>
#include <stdlib.h>

#include "scanner.h"

static const char *token_name(TokenType t) {
    switch (t) {
    case TOKEN_EOF: return "EOF";
    case TOKEN_EOL: return "EOL";
    case TOKEN_IDENTIFIER: return "IDENTIFIER";
    case TOKEN_INT_LITERAL: return "INT_LITERAL";
    case TOKEN_DOUBLE_LITERAL: return "DOUBLE_LITERAL";
    case TOKEN_STRING_LITERAL: return "STRING_LITERAL";
    case TOKEN_KW_DEF: return "KW_DEF";
    case TOKEN_KW_DO: return "KW_DO";
    case TOKEN_KW_ELSE: return "KW_ELSE";
    case TOKEN_KW_IF: return "KW_IF";
    case TOKEN_KW_RETURN: return "KW_RETURN";
    case TOKEN_KW_THEN: return "KW_THEN";
    case TOKEN_KW_VAL: return "KW_VAL";
    case TOKEN_KW_VAR: return "KW_VAR";
    case TOKEN_KW_WHILE: return "KW_WHILE";
    case TOKEN_KW_IMPORT: return "KW_IMPORT";
    case TOKEN_TYPE_INT: return "TYPE_INT";
    case TOKEN_TYPE_DOUBLE: return "TYPE_DOUBLE";
    case TOKEN_TYPE_STRING: return "TYPE_STRING";
    case TOKEN_TYPE_UNIT: return "TYPE_UNIT";
    case TOKEN_ASSIGN: return "ASSIGN";
    case TOKEN_PLUS: return "PLUS";
    case TOKEN_MINUS: return "MINUS";
    case TOKEN_MUL: return "MUL";
    case TOKEN_DIV: return "DIV";
    case TOKEN_EQ: return "EQ";
    case TOKEN_NEQ: return "NEQ";
    case TOKEN_LT: return "LT";
    case TOKEN_LEQ: return "LEQ";
    case TOKEN_GT: return "GT";
    case TOKEN_GEQ: return "GEQ";
    case TOKEN_LPAREN: return "LPAREN";
    case TOKEN_RPAREN: return "RPAREN";
    case TOKEN_COLON: return "COLON";
    case TOKEN_COMMA: return "COMMA";
    case TOKEN_INDENT: return "INDENT";
    case TOKEN_DEDENT: return "DEDENT";
    case TOKEN_DOT: return "DOT";
    case TOKEN_AT: return "AT";
    case TOKEN_SEMICOL: return "SEMICOLON";
    default: return "?? (doplnit do token_name)";
    }
}

int main(void) {
    scanner_init(stdin);

    Token tok;
    int n = 0;
    for (;;) {
        int rc = get_next_token(&tok);
        if (rc != 0) {
            fprintf(stderr, "Lexical error (code %d) after %d tokens\n", rc, n);
            scanner_free();
            return 1;
        }

        printf("%3d  %-15s", n++, token_name(tok.type));
        switch (tok.type) {
        case TOKEN_INT_LITERAL:
            printf(" %d", tok.attribute.i_val);
            break;
        case TOKEN_DOUBLE_LITERAL:
            printf(" %g", tok.attribute.d_val);
            break;
        case TOKEN_IDENTIFIER:
        case TOKEN_STRING_LITERAL:
            printf(" \"%s\"", tok.attribute.s_val);
            free(tok.attribute.s_val);
            break;
        default:
            break;
        }
        printf("\n");

        if (tok.type == TOKEN_EOF)
            break;
    }

    scanner_free();
    return 0;
}
