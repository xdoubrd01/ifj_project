/**
 * @par Projekt
 * Implementace překladače imperativního jazyka IFJ26
 * @file array_utils.c
 * @author Denis Doubrava (xdoubrd01)
 * @brief Výpočet kapacity dynamických polí s kontrolou přetečení velikostí.
 * Určuje potřebnou kapacitu podle aktuální velikosti, požadovaného počtu
 * prvků a velikosti jednoho prvku. Při výpočtu kontroluje přetečení,
 * aby ostatní moduly mohly bezpečně připravit alokaci.
 * @note AI byla využita k lepšímu znění a obsahu komentářů.
 */

#include "array_utils.h"
#include <stdint.h>

/**
 * @brief Vypočítá bezpečnou kapacitu dynamického pole.
 *
 * Kapacita je vyjádřena počtem prvků, nikoli počtem bajtů. Funkce kontroluje mez SIZE_MAX /
 * element_size, při růstu obvykle zdvojnásobuje kapacitu a poblíž limitu použije přesný
 * požadovaný počet. Paměť nealokuje.
 * @param current Dosavadní počet dostupných prvků.
 * @param required Nejmenší požadovaný počet prvků.
 * @param element_size Velikost jednoho prvku v bajtech; musí být nenulová.
 * @param initial Výchozí kapacita při current == 0; hodnota 0 znamená 1.
 * @param [out] result Výsledná kapacita; ukazatel nesmí být NULL.
 * @return True při úspěchu; false při neplatných argumentech nebo nereprezentovatelné velikosti.
 * @note Při neúspěchu se do result nezapisuje.
 */
bool array_next_capacity(size_t current, size_t required, size_t element_size, size_t initial,
                         size_t *result) {
    // Bez výstupního ukazatele nebo při nulové velikosti prvku nelze kapacitu určit.
    if (result == NULL || element_size == 0) {
        return false;
    }

    // Maximální počet prvků určíme ještě před násobením velikostí prvku.
    size_t limit = SIZE_MAX / element_size;
    if (required > limit || current > limit) {
        return false;
    }

    // Pokud nynější kapacita stačí, není třeba žádný růst ani další alokace.
    if (required <= current) {
        *result = current;
        return true;
    }

    size_t capacity = current;
    // První alokace vychází z požadované počáteční kapacity, nejméně z jednoho prvku.
    if (capacity == 0) {
        capacity = initial == 0 ? 1 : initial;
        if (capacity > limit) {
            capacity = required;
        }
    }

    while (capacity < required) {
        // Zdvojnásobení by překročilo mez; postačí přesná požadovaná kapacita.
        if (capacity > limit / 2) {
            capacity = required;
            break;
        }
        capacity *= 2;
    }

    // Výsledek zveřejníme až po ověření, že požadovaný počet prvků lze bezpečně uložit.
    *result = capacity;
    return true;
}