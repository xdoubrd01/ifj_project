/**
 * @par Projekt
 * Implementace překladače imperativního jazyka IFJ26
 * @file valmem.c
 * @author Denis Doubrava (xdoubrd01), Jakub Grásgruber (xgrasgj00), Eliáš Oharek (xoharee00),
 * Michal Zedníček (xzednim00)
 * @brief Implementace typových kontrol členských přístupů rozšíření VALMEM.
 * Rozpoznává členy toString, toInt, floor a length podle jejich jména.
 * Ověřuje přípustnost členu pro datový typ přijímající hodnoty.
 * Přiřazuje označení operace a výsledný typ pro navazující zpracování výrazu.
 * @note AI byla využita k lepšímu znění a obsahu komentářů.
 */

#include "valmem.h"
#include <string.h>

/**
 * @brief Přiřadí známému členu operaci a ověří typ jeho příjemce.
 * @param name Jméno členu ukončené nulovým znakem; nesmí být NULL.
 * @param receiver Datový typ hodnoty před tečkou.
 * @param operation Platný výstupní ukazatel na označení operace z výčtu ValMember.
 * @param result Platný výstupní ukazatel na výsledný datový typ.
 * @return 0 při platném přístupu; 3 pro neznámý člen; 6 pro nekompatibilní typ příjemce.
 * @note Známý člen nastaví výstupy ještě před kontrolou typu; při chybě je volající nepoužije.
 * @note Funkce pouze kontroluje typy, neprovádí samotný převod ani generování instrukcí.
 */
int valmem_check(const char *name, data_type receiver, unsigned *operation, data_type *result) {
    if (strcmp(name, "toString") == 0) {
        // Převod na řetězec nepovolujeme pro neznámý, zástupný ani interní pravdivostní typ.
        *operation = VALMEM_STRING;
        *result    = type_string;
        return receiver == type_unknown || receiver == type_any || receiver == type_bool ? 6 : 0;
    }
    if (strcmp(name, "toInt") == 0) {
        // Člen toInt přijímá pouze Double a výsledkem je Int.
        *operation = VALMEM_INT;
        *result    = type_int;
        return receiver == type_double ? 0 : 6;
    }
    if (strcmp(name, "floor") == 0) {
        // Člen floor pracuje s hodnotou Double a zachovává výsledný typ Double.
        *operation = VALMEM_FLOOR;
        *result    = type_double;
        return receiver == type_double ? 0 : 6;
    }
    if (strcmp(name, "length") == 0) {
        // Délka je dostupná pouze pro String a má typ Int.
        *operation = VALMEM_LENGTH;
        *result    = type_int;
        return receiver == type_string ? 0 : 6;
    }
    // Žádný podporovaný člen neodpovídá zadanému jménu; výstupy se zde nemění.
    return 3;
}
