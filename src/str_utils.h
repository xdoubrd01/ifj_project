/**
 * @par Projekt
 * Implementace překladače imperativního jazyka IFJ26
 * @file str_utils.h
 * @author Denis Doubrava (xdoubrd01)
 * @brief Rozhraní pomocných funkcí pro kopírování řetězců.
 * Deklaruje vytvoření kopie celého řetězce i jeho úseku zadaného délkou.
 * Uvádí, že vlastnictví výsledné alokace přechází na volajícího.
 * @note AI byla využita k lepšímu znění a obsahu komentářů.
 */

#ifndef STR_UTILS_H
#define STR_UTILS_H

// Vrácené kopie mají vlastní alokaci a používají stejné uvolňování pomocí free().
#include <stddef.h>

/**
 * @brief Vytvoří vlastněnou kopii celého řetězce.
 *
 * Délku zjistí pomocí strlen() a kopírování předá string_duplicate_n(). Výsledek obsahuje
 * samostatnou alokaci ukončenou nulovým znakem.
 * @param text Řetězec ke zkopírování.
 * @return Nově alokovaná kopie, nebo NULL pro vstup NULL či při selhání alokace.
 * @note Volající vlastní výsledek a uvolňuje jej pomocí free().
 */
char *string_duplicate(const char *text);

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
char *string_duplicate_n(const char *text, size_t length);

#endif