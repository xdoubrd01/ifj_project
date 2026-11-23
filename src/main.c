#define _POSIX_C_SOURCE 200809L
#include <stdint.h>
#include "scanner.h"
#include <stdio.h>
#include <stdlib.h> // Přidáno pro použití free()
#include "parser.h"
// Pomocná funkce pro převod typu tokenu na čitelný text
const char* token_type_to_string(TokenType type) {
    switch (type) {
        case TOKEN_EOF: return "TOKEN_EOF";
        case TOKEN_EOL: return "TOKEN_EOL";
        case TOKEN_IDENTIFIER: return "TOKEN_IDENTIFIER";
        case TOKEN_INT_LITERAL: return "TOKEN_INT_LITERAL";
        case TOKEN_DOUBLE_LITERAL: return "TOKEN_DOUBLE_LITERAL";
        case TOKEN_STRING_LITERAL: return "TOKEN_STRING_LITERAL";
        case TOKEN_KW_DEF: return "TOKEN_KW_DEF";
        case TOKEN_KW_DO: return "TOKEN_KW_DO";
        case TOKEN_KW_ELSE: return "TOKEN_KW_ELSE";
        case TOKEN_KW_IF: return "TOKEN_KW_IF";
        case TOKEN_KW_RETURN: return "TOKEN_KW_RETURN";
        case TOKEN_KW_THEN: return "TOKEN_KW_THEN";
        case TOKEN_KW_VAL: return "TOKEN_KW_VAL";
        case TOKEN_KW_VAR: return "TOKEN_KW_VAR";
        case TOKEN_KW_WHILE: return "TOKEN_KW_WHILE";
        case TOKEN_KW_IMPORT: return "TOKEN_KW_IMPORT";
        case TOKEN_TYPE_INT: return "TOKEN_TYPE_INT";
        case TOKEN_TYPE_DOUBLE: return "TOKEN_TYPE_DOUBLE";
        case TOKEN_TYPE_STRING: return "TOKEN_TYPE_STRING";
        case TOKEN_TYPE_UNIT: return "TOKEN_TYPE_UNIT";
        case TOKEN_ASSIGN: return "TOKEN_ASSIGN (=)";
        case TOKEN_PLUS: return "TOKEN_PLUS (+)";
        case TOKEN_MINUS: return "TOKEN_MINUS (-)";
        case TOKEN_MUL: return "TOKEN_MUL (*)";
        case TOKEN_DIV: return "TOKEN_DIV (/)";
        case TOKEN_EQ: return "TOKEN_EQ (==)";
        case TOKEN_NEQ: return "TOKEN_NEQ (!=)";
        case TOKEN_LT: return "TOKEN_LT (<)";
        case TOKEN_LEQ: return "TOKEN_LEQ (<=)";
        case TOKEN_GT: return "TOKEN_GT (>)";
        case TOKEN_GEQ: return "TOKEN_GEQ (>=)";
        case TOKEN_LPAREN: return "TOKEN_LPAREN (()";
        case TOKEN_RPAREN: return "TOKEN_RPAREN ())";
        case TOKEN_COLON: return "TOKEN_COLON (:)";
        case TOKEN_COMMA: return "TOKEN_COMMA (,)";
        case TOKEN_DOT: return "TOKEN_DOT (.)";
        case TOKEN_SEMICOLON: return "TOKEN_SEMICOLON (;)";
        case TOKEN_AT_MAIN: return "TOKEN_AT_MAIN (@main)";
        case TOKEN_INDENT: return "TOKEN_INDENT (začátek bloku)";
        case TOKEN_DEDENT: return "TOKEN_DEDENT (konec bloku)";
        default: return "UNKNOWN_TOKEN";
    }
}

/*
 * AI-generated code (ChatGPT).
 * Prompt: "nahrad pomocnou funkci prepare_input(), ktera pouziva
 * tmpfile() resenim s dynamickym polem a fmemopen().
 * vstup se nacita ze stdin, musi podporovat dva pruchody
 * parseru spravne osetrovat chyby a uvolnneni pameti."
 */
static FILE *prepare_input(char **buffer_out) {
    size_t capacity = 4096;
    size_t length = 0;

    *buffer_out = NULL;

    char *buffer = malloc(capacity);
    if (buffer == NULL) {
        return NULL;
    }

    int c;

    while ((c = fgetc(stdin)) != EOF) {
        if (length == capacity) {
            if (capacity > SIZE_MAX / 2) {
                free(buffer);
                return NULL;
            }

            capacity *= 2;

            char *tmp = realloc(buffer, capacity);
            if (tmp == NULL) {
                free(buffer);
                return NULL;
            }

            buffer = tmp;
        }

        buffer[length++] = (char)c;
    }

    if (ferror(stdin)) {
        free(buffer);
        return NULL;
    }

    FILE *source = fmemopen(buffer, length, "r");

    if (source == NULL) {
        free(buffer);
        return NULL;
    }

    *buffer_out = buffer;
    return source;
}

int main(void) {
    char *buffer = NULL;
    FILE *source = prepare_input(&buffer);

    if (source == NULL) {
        fprintf(stderr, "Internal error: cannot prepare input\n");
        return 99;
    }

    parse(source);

    fclose(source);
    free(buffer);

    return 0;
}