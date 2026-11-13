#include "scanner.h"
#include <stdio.h>
#include <stdlib.h> // Přidáno pro použití free()

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
 * Prompt: "napis pomocnou funkci v jazyce C, ktera prekopiruje
 * obsah stdin do docasneho souboru pomoci tmpfile(), aby bylo mozne
 * vstup opakovane cist pri dvoupruchodove analy ze. Osetri take chyby
 * cteni, zapisu a alokace souboru."
 */
static FILE *prepare_input(void) {
    FILE *temp = tmpfile();

    if (temp == NULL) {
        return NULL;
    }

    int c;

    while ((c = fgetc(stdin)) != EOF) {
        if (fputc(c, temp) == EOF) {
            fclose(temp);
            return NULL;
        }
    }

    if (ferror(stdin) || fflush(temp) == EOF) {
        fclose(temp);
        return NULL;
    }

    rewind(temp);

    if (ferror(temp)) {
        fclose(temp);
        return NULL;
    }

    return temp;
}

int main() {
    FILE *source = prepare_input();

    if (source == NULL) {
        fprintf(stderr, "Internal error: cannot prepare input\n");
        return 99;
    }

    scanner_init(source);

    Token token;
    int result;

    while (true) {
        result = get_next_token(&token);

        if (result != 0) {
            printf("CHYBA: Lexikální analyzátor vrátil chybový kód %d\n", result);
            break;
        }

        printf("Typ: %s", token_type_to_string(token.type));

        // Vypíšeme atribut pro identifikátory a literály a hned uvolníme
        if (token.type == TOKEN_IDENTIFIER || token.type == TOKEN_STRING_LITERAL) {
            printf(" | Hodnota: \"%s\"", token.attribute.s_val);
            // Uvolníme dynamicky alokovanou paměť vrácenou skenerem
            free(token.attribute.s_val);
            token.attribute.s_val = NULL; // Bezpečnostní nulování ukazatele
        } else if (token.type == TOKEN_INT_LITERAL) {
            printf(" | Hodnota: %d", token.attribute.i_val);
        } else if (token.type == TOKEN_DOUBLE_LITERAL) {
            printf(" | Hodnota: %g", token.attribute.d_val);
        }

        printf("\n");

        if (token.type == TOKEN_EOF) {
            break;
        }
    }

    // Korektní ukončení a smazání globálního bufferu uvnitř skeneru
    scanner_free();
    fclose(source);
    return result;
}