/**
 * @par Projekt
 * Implementace překladače imperativního jazyka IFJ26
 * @file str_utils.c
 * @author Denis Doubrava (xdoubrd01)
 * @brief Vytváření dynamicky alokovaných kopií řetězců a jejich úseků.
 * Zajišťuje kopírování celého řetězce nebo přesně určeného počtu bajtů
 * a přidání koncového nulového znaku. Výslednou paměť předává volajícímu,
 * který odpovídá za její uvolnění.
 * @note AI byla využita k lepšímu znění a obsahu komentářů.
 */

#include "str_utils.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief Zkopíruje přesně zadaný počet bajtů a připojí nulový znak.
 *
 * Alokuje length + 1 bajtů a data kopíruje pomocí memcpy(). Funkce nehledá konec vstupního
 * řetězce; zadaný úsek proto musí být čitelný v celé délce.
 * @param text Začátek čitelného zdrojového úseku; nesmí být NULL.
 * @param length Počet kopírovaných bajtů bez přidaného terminátoru.
 * @return Vlastněná kopie, nebo NULL pro neplatný vstup, délku SIZE_MAX či nedostatek paměti.
 * @note Výsledek uvolňuje volající pomocí free(); délka 0 vytváří prázdný řetězec.
 */
char *string_duplicate_n(const char *text, size_t length) {
    // Hodnotu SIZE_MAX odmítáme, protože pro koncovou nulu potřebujeme délku zvýšit.
    if (text == NULL || length == SIZE_MAX) {
        return NULL;
    }

    // Samostatný bajt navíc slouží pro nulový terminátor.
    char *copy = malloc(length + 1);
    if (copy != NULL) {
        // Zadaná délka určuje přesný počet bajtů; vstup nemusí v tomto úseku končit nulou.
        memcpy(copy, text, length);
        // Terminátor přidáme až za všechny zkopírované bajty.
        copy[length] = '\0';
    }

    return copy;
}

/**
 * @brief Vytvoří vlastněnou kopii celého řetězce.
 *
 * Délku zjistí pomocí strlen() a kopírování předá string_duplicate_n(). Výsledek obsahuje
 * samostatnou alokaci ukončenou nulovým znakem.
 * @param text Řetězec ke zkopírování.
 * @return Nově alokovaná kopie, nebo NULL pro vstup NULL či při selhání alokace.
 * @note Volající vlastní výsledek a uvolňuje jej pomocí free().
 */
char *string_duplicate(const char *text) {
    // Pro běžný řetězec využijeme stejnou kopírovací funkci s délkou zjištěnou pomocí strlen().
    return text == NULL ? NULL : string_duplicate_n(text, strlen(text));
}