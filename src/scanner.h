#ifndef SCANNER_H
#define SCANNER_H

#include <stdbool.h>
#include <stdio.h>

// Výčet všech možných typů tokenů pro jazyk IFJ26.
// TODO (Kolega 2 - Parser): Tento výčet budeš používat ve svém rekurzivním sestupu
// pro kontrolu očekávané syntaxe (např. if (token.type == TOKEN_KW_DEF) ...).
typedef enum {
    TOKEN_EOF, // Konec souboru (Parser zde ukončí analýzu)
    TOKEN_EOL, // Konec logického řádku (oddělovač příkazů, pokud není středník)

    // Identifikátory a literály
    TOKEN_IDENTIFIER,     // např. nazev_promenne, jmeno_funkce
    TOKEN_INT_LITERAL,    // Celočíselný literál (např. 42)
    TOKEN_DOUBLE_LITERAL, // Desetinný literál (např. 3.14, 1e-5)
    TOKEN_STRING_LITERAL, // Řetězcový literál (vč. víceřádkových a escape sekvencí)

    // Klíčová slova jazyka IFJ26
    TOKEN_KW_DEF,
    TOKEN_KW_DO,
    TOKEN_KW_ELSE,
    TOKEN_KW_IF,
    TOKEN_KW_RETURN,
    TOKEN_KW_THEN,
    TOKEN_KW_VAL,
    TOKEN_KW_VAR,
    TOKEN_KW_WHILE,
    TOKEN_KW_IMPORT,

    // Datové typy
    TOKEN_TYPE_INT,
    TOKEN_TYPE_DOUBLE,
    TOKEN_TYPE_STRING,
    TOKEN_TYPE_UNIT,

    // Operátory a oddělovače
    TOKEN_ASSIGN, // =
    TOKEN_PLUS,   // +
    TOKEN_MINUS,  // -
    TOKEN_MUL,    // *
    TOKEN_DIV,    // /
    TOKEN_EQ,     // ==
    TOKEN_NEQ,    // !=
    TOKEN_LT,     // <
    TOKEN_LTE,    // <=
    TOKEN_GT,     // >
    TOKEN_GTE,    // >=
    TOKEN_LPAREN, // (
    TOKEN_RPAREN, // )
    TOKEN_COLON,  // :
    TOKEN_COMMA,  // ,

    // Speciální tokeny pro řízení bloků pomocí odsazení mezerami.
    // TODO (Kolega 1 - Lexer): Tyto tokeny musíš generovat automaticky při změně počtu
    // mezer na začátku nového logického řádku, protože IFJ26 nepoužívá složené závorky.
    TOKEN_INDENT, // Zvětšení úrovně odsazení (otevření bloku)
    TOKEN_DEDENT  // Zmenšení úrovně odsazení (uzavření bloku)
} TokenType;

// Struktura reprezentující lexikální jednotku (token).
// TODO (Kolega 1 - Lexer): Ujisti se, že při vrácení tokenu správně naplníš atribut `attribute`,
// pokud se jedná o literál nebo identifikátor. Pro operátory a klíčová slova atribut nevyplňuj.
typedef struct {
    TokenType type; // Typ tokenu

    // Atribut tokenu (přídavná hodnota nesoucí sémantickou informaci)
    union {
        int    i_val; // Vyplň pro TOKEN_INT_LITERAL
        double d_val; // Vyplň pro TOKEN_DOUBLE_LITERAL
        char  *s_val; // Vyplň pro TOKEN_STRING_LITERAL a TOKEN_IDENTIFIER (musí být dynamicky
                      // alokováno!)
    } attribute;
} Token;

/**
 * @brief Inicializuje lexikální analyzátor.
 * @param source_file Ukazatel na otevřený soubor se zdrojovým kódem IFJ26.
 */
void scanner_init(FILE *source_file);

/**
 * @brief Hlavní komunikační funkce pro Parser (Kolegu 2).
 * Přečte další lexém ze vstupu a naplní jím předanou strukturu Token.
 *
 * TODO (Kolega 2 - Parser): Tuto funkci budeš volat v cyklu nebo na začátku
 * každé funkce rekurzivního sestupu, abys zjistil, jaký prvek následuje.
 *
 * @param token Ukazatel na strukturu, do které se zapíše výsledek.
 * @return Hodnota 0 při úspěchu, chybový kód 1 při lexikální chybě (např. neznámý znak).
 */
int get_next_token(Token *token);

/**
 * @brief Uvolní veškerou dynamicky alokovanou paměť skeneru (např. zásobník odsazení).
 */
void scanner_free(void);

#endif // SCANNER_H