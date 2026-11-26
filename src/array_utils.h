/**
 * @par Projekt
 * Implementace překladače imperativního jazyka IFJ26
 * @file array_utils.h
 * @author Denis Doubrava (xdoubrd01)
 * @brief Rozhraní pro výpočet kapacity dynamických polí s kontrolou přetečení.
 * Deklaruje pomocnou funkci pro volbu nové kapacity podle požadovaného
 * počtu prvků. Umožňuje sdílet pravidla růstu a kontrolu přetečení
 * mezi moduly používajícími dynamická pole.
 * @note AI byla využita k lepšímu znění a obsahu komentářů.
 */

#ifndef ARRAY_UTILS_H
#define ARRAY_UTILS_H

// Rozhraní vrací pouze vypočítanou kapacitu; přidělení paměti řeší volající.
#include <stdbool.h>
#include <stddef.h>

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
                         size_t *result);

#endif // ARRAY_UTILS_H
