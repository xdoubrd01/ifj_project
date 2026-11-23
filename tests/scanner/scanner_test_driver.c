/*
 * AI-assisted test utility.
 * Prompt: "Připrav komplexní testy pouze pro scanner IFJ26, včetně přesného
 * ověření typů tokenů, atributů tokenů a lexikálních chyb."
 *
 * Tento soubor je pouze testovací pomůcka, není součástí implementace překladače.
 */

#include "scanner.h"

#include <stdio.h>
#include <stdlib.h>

static const char *token_name(TokenType type) {
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
        case TOKEN_ASSIGN: return "TOKEN_ASSIGN";
        case TOKEN_PLUS: return "TOKEN_PLUS";
        case TOKEN_MINUS: return "TOKEN_MINUS";
        case TOKEN_MUL: return "TOKEN_MUL";
        case TOKEN_DIV: return "TOKEN_DIV";
        case TOKEN_EQ: return "TOKEN_EQ";
        case TOKEN_NEQ: return "TOKEN_NEQ";
        case TOKEN_LT: return "TOKEN_LT";
        case TOKEN_LEQ: return "TOKEN_LEQ";
        case TOKEN_GT: return "TOKEN_GT";
        case TOKEN_GEQ: return "TOKEN_GEQ";
        case TOKEN_LPAREN: return "TOKEN_LPAREN";
        case TOKEN_RPAREN: return "TOKEN_RPAREN";
        case TOKEN_COLON: return "TOKEN_COLON";
        case TOKEN_COMMA: return "TOKEN_COMMA";
        case TOKEN_DOT: return "TOKEN_DOT";
        case TOKEN_SEMICOLON: return "TOKEN_SEMICOLON";
        case TOKEN_AT_MAIN: return "TOKEN_AT_MAIN";
        case TOKEN_INDENT: return "TOKEN_INDENT";
        case TOKEN_DEDENT: return "TOKEN_DEDENT";
        default: return "TOKEN_UNKNOWN";
    }
}

static void print_escaped(const char *s) {
    putchar('"');
    if (s != NULL) {
        const unsigned char *p = (const unsigned char *)s;
        while (*p != '\0') {
            switch (*p) {
                case '\\': fputs("\\\\", stdout); break;
                case '"':  fputs("\\\"", stdout); break;
                case '\n': fputs("\\n", stdout); break;
                case '\r': fputs("\\r", stdout); break;
                case '\t': fputs("\\t", stdout); break;
                default:
                    if (*p < 32 || *p > 126) {
                        printf("\\x%02X", (unsigned int)*p);
                    } else {
                        putchar((int)*p);
                    }
                    break;
            }
            ++p;
        }
    }
    putchar('"');
}

static void print_token(const Token *token) {
    fputs(token_name(token->type), stdout);

    switch (token->type) {
        case TOKEN_IDENTIFIER:
        case TOKEN_STRING_LITERAL:
            putchar('(');
            print_escaped(token->attribute.s_val);
            putchar(')');
            break;

        case TOKEN_INT_LITERAL:
            printf("(%d)", token->attribute.i_val);
            break;

        case TOKEN_DOUBLE_LITERAL:
            printf("(%.17g)", token->attribute.d_val);
            break;

        default:
            break;
    }

    putchar('\n');
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s INPUT.ifj26\n", argv[0]);
        return 99;
    }

    FILE *input = fopen(argv[1], "rb");
    if (input == NULL) {
        perror("fopen");
        return 99;
    }

    scanner_init(input);

    while (1) {
        Token token = {0};
        int rc = get_next_token(&token);

        if (rc != 0) {
            printf("ERROR(%d)\n", rc);
            scanner_free();
            fclose(input);
            return rc;
        }

        print_token(&token);

        if (token.type == TOKEN_IDENTIFIER ||
            token.type == TOKEN_STRING_LITERAL) {
            free(token.attribute.s_val);
        }

        if (token.type == TOKEN_EOF) {
            break;
        }
    }

    scanner_free();
    fclose(input);
    return 0;
}
