/**
 * @par Projekt
 * Implementace překladače imperativního jazyka IFJ26
 * @file token_utils.h
 * @author Denis Doubrava (xdoubrd01)
 * @brief Rozhraní pomocné funkce pro uvolnění a reset tokenu.
 * Deklaruje funkci token_destroy(), která uvolňuje atribut podle typu tokenu.
 * Umožňuje ostatním analyzátorům jednotně ukončit práci s tokenem
 * bez opakování stejné obsluhy paměti.
 * @note AI byla využita k lepšímu znění a obsahu komentářů.
 */

#ifndef TOKEN_UTILS_H
#define TOKEN_UTILS_H

// Před opětovným použitím tokenu lze tímto rozhraním bezpečně uvolnit jeho text.
#include "scanner.h"

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
void token_destroy(Token *token);

#endif