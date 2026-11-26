/**
 * @par Projekt
 * Implementace překladače imperativního jazyka IFJ26
 * @file token_utils.c
 * @author Denis Doubrava (xdoubrd01)
 * @brief Uvolňování textových atributů tokenů a obnovení jejich výchozího stavu.
 * Podle typu tokenu uvolní jeho vlastněný řetězcový atribut.
 * Poté nastaví token do stavu EOF a vynuluje příznak předchozího nového řádku.
 * @note AI byla využita k lepšímu znění a obsahu komentářů.
 */

#include "token_utils.h"
#include <stdlib.h>

/**
 * @brief Uvolní vlastněný atribut tokenu a token resetuje.
 *
 * Pro identifikátor a řetězcový literál uvolní attribute.s_val. Poté vynuluje celou
 * strukturu a nastaví typ TOKEN_EOF, takže newline_before je false a další atributy se
 * zahodí.
 * @param token Token k resetování; NULL se ignoruje.
 * @note Funkce neuvolňuje samotnou strukturu Token.
 * @note Textový atribut musí pocházet z alokace, kterou lze předat free().
 */
void token_destroy(Token *token) {
    // Volání bez tokenu je neškodné a nic neuvolňuje.
    if (token == NULL) {
        return;
    }

    // Vlastní alokovaný text pouze tyto dva typy; číselné atributy se neuvolňují.
    if (token->type == TOKEN_IDENTIFIER || token->type == TOKEN_STRING_LITERAL) {
        free(token->attribute.s_val);
    }

    // Nulová inicializace vymaže atribut i newline_before a zabrání opětovnému uvolnění textu.
    *token = (Token){.type = TOKEN_EOF};
}