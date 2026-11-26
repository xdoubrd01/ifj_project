/**
 * @project Implementace překladače imperativního jazyka IFJ26
 * @file hashtable.h
 * @author Denis Doubrava, Jakub Grásgruber, Eliáš Oharek, Michal Zedníček
 * @brief Veřejné rozhraní hashovací tabulky s řetězcovými klíči
 * a obecnými hodnotami. Deklaruje operace pro vytvoření a uvolnění
 * tabulky a pro přístup k jejím položkám. Skrývá vnitřní reprezentaci
 * tabulky a popisuje vlastnictví klíčů a uložených hodnot.
 * @note AI byla využita k lepšímu znění a obsahu komentářů.
 */

#ifndef HASHTABLE_H
#define HASHTABLE_H

#include <stdbool.h>
#include <stddef.h>

// Vnitřní buňky a jejich stavy nejsou součástí veřejného rozhraní.
struct hashtable;

/**
 * @brief Neprůhledný typ hashovací tabulky se skrytou reprezentací.
 */
typedef struct hashtable hashtable;

/**
 * @brief Typ obslužné funkce pro jednu uloženou hodnotu.
 *
 * Přijímá ukazatel data uložený v položce a nevrací výsledek. Slouží například pro uvolnění
 * hodnot ve ht_foreach().
 */
typedef void (*ht_value_callback)(void *);

/**
 * @brief Vytvoří prázdnou hashovací tabulku.
 * @param initial_capacity Počáteční počet buněk; 0 volí výchozí kapacitu 16.
 * @return Nová tabulka, nebo NULL při nereprezentovatelné velikosti či selhání alokace.
 * @note Vytvořenou tabulku musí volající nakonec zrušit pomocí ht_free().
 */
hashtable *ht_create(size_t initial_capacity);

/**
 * @brief Zavolá obslužnou funkci pro každou obsazenou položku.
 *
 * Prázdné a odstraněné buňky se přeskakují. Pořadí není určeno; je-li stejný ukazatel uložen
 * pod více klíči, obsluha jej dostane jednou za každou položku.
 * @param ht Procházená tabulka; NULL se ignoruje.
 * @param cb Obslužná funkce přijímající ukazatel na hodnotu; NULL se ignoruje.
 * @warning Obsluha nesmí měnit položky ani zrušit právě procházenou tabulku.
 * @warning Při uvolňování sdílených hodnot zabraňte opakovanému free(). Po uvolnění hodnot tabulku
 * zrušte; její datové ukazatele již nejsou platné.
 */
void ht_foreach(hashtable *ht, ht_value_callback cb);

/**
 * @brief Vloží položku nebo nahradí hodnotu existujícího klíče.
 *
 * Při kolizi používá lineární prohledávání s návratem na začátek pole. Nový klíč kopíruje;
 * samotná hodnota zůstává předaným ukazatelem. První buňku dead využije až po vyloučení
 * existujícího shodného klíče.
 * @param table Platná cílová tabulka.
 * @param key Nenulový ukazatel na řetězec zakončený nulou.
 * @param data Nenulový ukazatel na uloženou hodnotu.
 * @return True při úspěchu; false při neplatných argumentech nebo selhání alokace.
 * @warning Nahrazení hodnoty neuvolní předchozí hodnotu; její životnost řeší volající.
 */
bool ht_insert(hashtable *table, const char *key, void *data);

/**
 * @brief Vyhledá hodnotu podle řetězcového klíče.
 *
 * Přes buňky dead pokračuje; buňka empty nebo prohledání celé kapacity znamená neúspěch.
 * Obsah hodnoty nekopíruje a nepředává její vlastnictví.
 * @param table Prohledávaná tabulka.
 * @param key Hledaný klíč zakončený nulou.
 * @return Uložený ukazatel, nebo NULL při neúspěchu či neplatných argumentech.
 */
void *ht_search(hashtable *table, const char *key);

/**
 * @brief Odstraní položku a zachová návaznost prohledávání.
 *
 * Uvolní kopii klíče, vyprázdní ukazatele buňky a nastaví stav dead. Sníží počet obsazených
 * buněk size, nikoli počet použitých buněk used.
 * @param table Tabulka obsahující odstraňovanou položku.
 * @param key Klíč odstraňované položky zakončený nulou.
 * @return True při odstranění; false při nenalezení klíče nebo neplatných argumentech.
 * @note Hodnotu neuvolňuje; potřebný ukazatel si volající musí získat před odstraněním.
 */
bool ht_delete(hashtable *table, const char *key);

/**
 * @brief Zruší tabulku, její kopie klíčů a pole buněk.
 * @param table Tabulka vytvořená ht_create(); NULL se ignoruje.
 * @warning Uložené hodnoty se neuvolňují; jejich zrušení musí zajistit volající.
 * @warning Předaný ukazatel po návratu není platný; proměnná volajícího se automaticky nenastaví na
 * NULL.
 */
void ht_free(hashtable *table);

#endif // HASHTABLE_H