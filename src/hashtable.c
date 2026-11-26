/**
 * @project Implementace překladače imperativního jazyka IFJ26
 * @file hashtable.c
 * @authors Denis Doubrava (xdoubrd01), Jakub Grásgruber (xgrasgj00), Eliáš Oharek (xoharee00),
 * Michal Zedníček (xzednim00)
 * @brief Hashovací tabulka s otevřenou adresací, funkcí djb2 a lineárním
 * prohledáváním. Poskytuje vkládání, vyhledávání a odstraňování položek
 * s řetězcovými klíči. Rozlišuje prázdné, obsazené a odstraněné buňky
 * a při zvětšení tabulky přehashuje uložené položky.
 * @note AI byla využita k lepšímu znění a obsahu komentářů.
 */

#include "hashtable.h"
#include "str_utils.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief Výchozí počet buněk při požadované kapacitě 0.
 */
#define HT_DEFAULT_CAPACITY 16

/**
 * @brief Stavy buněk při otevřené adresaci.
 *
 * Odstraněná buňka zachovává návaznost prohledávání po kolizi.
 */
typedef enum { empty, full, dead } cell_state;

/**
 * @brief Interní buňka s vlastněným klíčem a hodnotou spravovanou volajícím.
 */
typedef struct {
    char      *key;   ///< Vlastněná kopie klíče; u neobsazených buněk NULL.
    void      *data;  ///< Ukazatel na hodnotu, jejíž uvolnění zajišťuje volající.
    cell_state state; ///< Stav empty, full nebo dead.
} hash_item;

/**
 * @brief Skrytá reprezentace hashovací tabulky.
 *
 * size počítá obsazené buňky; used zahrnuje obsazené i odstraněné buňky.
 */
struct hashtable {
    hash_item *items;    ///< Vlastněné pole buněk.
    size_t     size;     ///< Počet buněk ve stavu full.
    size_t     capacity; ///< Počet všech buněk alokovaného pole.
    size_t     used;     ///< Počet buněk full a dead pro řízení růstu.
};

/**
 * @brief Vypočítá hash řetězce pomocí djb2.
 * @param str Řetězec ukončený nulou; nesmí být NULL.
 * @return Hash klíče; index určí volající zbytkem po dělení kapacitou.
 */
static unsigned long hash_djb2(const char *str) {
    // Začneme hodnotou 5381 a postupně započítáme každý bajt klíče.
    unsigned long hash = 5381;
    int           c;

    while ((c = (unsigned char)*str++)) {
        /* Posun o pět bitů a přičtení původní hodnoty odpovídá násobení 33.
         * Neznaménková aritmetika při překročení rozsahu přeteče definovaným způsobem. */
        hash = ((hash << 5) + hash) + c;
    }

    return hash;
}

/**
 * @brief Inicializuje pole buněk již alokovaného objektu tabulky.
 * @param table Dosud neinicializovaný objekt; NULL se ignoruje.
 * @param initial_capacity Počáteční počet buněk; 0 volí výchozí kapacitu.
 * @warning Funkce neuvolňuje dosavadní prostředky; nepoužívejte ji nad živou tabulkou.
 * @post Při selhání je table->items nastaveno na NULL.
 */
static void ht_init(hashtable *table, size_t initial_capacity) {
    if (table == NULL) {
        return;
    }

    // Nula vybírá výchozí kapacitu, nikoli pole bez buněk.
    if (initial_capacity == 0) {
        initial_capacity = HT_DEFAULT_CAPACITY;
    }

    // Před násobením ověříme, že počet bajtů lze vyjádřit typem size_t.
    if (initial_capacity > SIZE_MAX / sizeof(hash_item)) {
        table->capacity = 0;
        table->size     = 0;
        table->used     = 0;
        table->items    = NULL;
        return;
    }

    // Pole buněk je samostatná alokace oddělená od objektu tabulky.
    table->items = malloc(initial_capacity * sizeof(hash_item));

    // Výsledek malloc() je již v items; ht_create() podle něj pozná neúspěch.
    if (table->items == NULL) {
        table->capacity = 0;
        table->size     = 0;
        table->used     = 0;
        return;
    }

    // Před prvním vložením označíme všechny buňky jako dosud nepoužité.
    for (size_t i = 0; i < initial_capacity; i++) {
        table->items[i].key   = NULL;
        table->items[i].data  = NULL;
        table->items[i].state = empty;
    }

    // Nové pole dosud neobsahuje platné ani odstraněné položky.
    table->capacity = initial_capacity;
    table->size     = 0;
    table->used     = 0;
}

/**
 * @brief Přenese existující položku do nového pole při přehashování.
 * @param items Inicializované cílové pole s volným místem.
 * @param capacity Nenulová kapacita cílového pole.
 * @param key Existující ukazatel na klíč; řetězec se nekopíruje.
 * @param data Existující ukazatel na hodnotu.
 * @note Nealokuje nové klíče ani hodnoty a nekontroluje duplicity.
 */
static void ht_insert_raw(hash_item *items, size_t capacity, char *key, void *data) {
    size_t idx = hash_djb2(key) % capacity;

    // Lineárně hledáme volnou buňku a za koncem pole pokračujeme od nuly.
    while (items[idx].state == full) {
        idx = (idx + 1) % capacity;
    }

    // Přeneseme původní ukazatele; nové pole převezme správu klíče.
    items[idx].key   = key;
    items[idx].data  = data;
    items[idx].state = full;
}

/**
 * @brief Zdvojnásobí kapacitu a přehashuje obsazené položky.
 * @param table Platná inicializovaná tabulka.
 * @return True při úspěchu; false při přetečení velikosti nebo selhání alokace.
 * @note Přenáší ukazatele bez kopírování klíčů a hodnot.
 * @post Po úspěchu used == size; při neúspěchu zůstává původní tabulka zachována.
 */
static bool ht_resize(hashtable *table) {
    // Kontrolujeme zdvojnásobení i výsledný počet bajtů nové alokace.
    if (table->capacity > SIZE_MAX / 2 / sizeof(hash_item)) {
        return false;
    }

    // Náhradní pole alokujeme před zásahem do původní tabulky.
    size_t     new_capacity = table->capacity * 2;
    hash_item *new_items    = malloc(new_capacity * sizeof(hash_item));

    if (new_items == NULL) {
        return false;
    }

    // Před přehashováním označíme celé nové pole jako prázdné.
    for (size_t i = 0; i < new_capacity; i++) {
        new_items[i].key   = NULL;
        new_items[i].data  = NULL;
        new_items[i].state = empty;
    }

    /* Index závisí na kapacitě, proto se musí znovu vypočítat.
     * Přenášíme existující ukazatele; klíče ani hodnoty znovu nealokujeme. */
    for (size_t i = 0; i < table->capacity; i++) {
        if (table->items[i].state == full) {
            ht_insert_raw(new_items, new_capacity, table->items[i].key, table->items[i].data);
        }
    }

    // Uvolníme pouze staré pole; ukazatele na klíče a hodnoty již přešly do nového.
    free(table->items);

    table->items    = new_items;
    table->capacity = new_capacity;
    table->used     = table->size; // Odstraněné buňky se nepřenášely, takže zůstávají jen obsazené.

    return true;
}

/**
 * @brief Vytvoří prázdnou hashovací tabulku.
 * @param initial_capacity Počáteční počet buněk; 0 volí výchozí kapacitu 16.
 * @return Nová tabulka, nebo NULL při nereprezentovatelné velikosti či selhání alokace.
 * @note Vytvořenou tabulku musí volající nakonec zrušit pomocí ht_free().
 */
hashtable *ht_create(size_t initial_capacity) {
    hashtable *table;

    // Alokujeme objekt tabulky; vlastní pole buněk vytvoří ht_init().
    table = (hashtable *)malloc(sizeof(hashtable));

    if (table == NULL) {
        return NULL;
    }

    // Když nelze vytvořit pole, uvolníme i vnější objekt.
    ht_init(table, initial_capacity);

    if (table->items == NULL) {
        free(table);
        return NULL;
    }

    // Po úspěchu obou alokací předáme tabulku volajícímu.
    return table;
}

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
bool ht_insert(hashtable *table, const char *key, void *data) {
    if (table == NULL || table->items == NULL || table->capacity == 0 || key == NULL ||
        data == NULL) {
        return false;
    }

    size_t idx = hash_djb2(key) % table->capacity;

    // Zapamatujeme první odstraněnou buňku, ale dál hledáme případný shodný klíč.
    // Kapacita slouží jako zástupná hodnota mimo rozsah platných indexů.
    // Dokud není nalezena žádná odstraněná buňka, držíme zde neplatný index.
    size_t first_dead = table->capacity;

    // Počet kroků omezíme kapacitou, aby hledání skončilo i bez prázdné buňky.
    for (size_t i = 0; i < table->capacity; i++) {
        hash_item *item = &table->items[idx];

        // Dosud nepoužitá buňka ukončuje hledání existujícího klíče.
        if (item->state == empty) {
            break;
        }

        /* Odstraněnou buňku si uložíme, ale dál kontrolujeme duplicitu klíče.
         * Okamžité vložení by mohlo vytvořit dvě položky se stejným klíčem. */
        if (item->state == dead) {
            if (first_dead == table->capacity) {
                first_dead = idx;
            }
        } else if (strcmp(item->key, key) == 0) {
            // Nahradíme pouze ukazatel na hodnotu; předchozí hodnotu musí případně uvolnit
            // volající.
            item->data = data;
            return true;
        }

        idx = (idx + 1) % table->capacity;
    }

    // Opětovné obsazení buňky dead nezvyšuje počet použitých buněk used.
    bool reuse_dead = (first_dead != table->capacity);

    // Při obsazení nové prázdné buňky nesmí podíl used překročit 75 % kapacity.
    if (!reuse_dead && (table->used + 1) * 4 > table->capacity * 3) {
        if (!ht_resize(table)) {
            return false;
        }

        // Po změně kapacity znovu vypočítáme index a nalezneme cílovou buňku.
        idx = hash_djb2(key) % table->capacity;

        while (table->items[idx].state == full) {
            idx = (idx + 1) % table->capacity;
        }
    } else if (reuse_dead) {
        // Přednostně využijeme první odstraněnou buňku nalezenou při prohledávání.
        idx = first_dead;
    } else if (table->items[idx].state != empty) {
        return false;
    }

    // Volající může svůj původní klíč změnit či uvolnit; tabulka ukládá vlastní kopii.
    char *key_copy = string_duplicate(key);

    if (key_copy == NULL) {
        return false;
    }

    // Položku zveřejníme až po úspěšném vytvoření vlastní kopie klíče.
    table->items[idx].key   = key_copy;
    table->items[idx].data  = data;
    table->items[idx].state = full;
    // Počet skutečně obsazených položek se zvýší i při opětovném využití buňky dead.
    table->size++;

    // Buňka dead již byla v used započítána.
    if (!reuse_dead) {
        table->used++;
    }

    return true;
}

/**
 * @brief Vyhledá obsazenou buňku bez změny tabulky.
 * @param table Prohledávaná tabulka.
 * @param key Hledaný řetězcový klíč.
 * @return Ukazatel na obsazenou buňku, nebo NULL při neúspěchu či neplatném vstupu.
 */
static hash_item *find_item(hashtable *table, const char *key) {
    if (table == NULL || table->items == NULL || table->capacity == 0 || key == NULL) {
        return NULL;
    }

    size_t idx = hash_djb2(key) % table->capacity;

    // Prázdná buňka hledání ukončí; přes odstraněnou se pokračuje.
    for (size_t i = 0; i < table->capacity; i++) {
        hash_item *item = &table->items[idx];

        if (item->state == empty) {
            return NULL;
        }

        // Klíč lze porovnávat jen v obsazené buňce.
        if (item->state == full && strcmp(item->key, key) == 0) {
            return item;
        }

        idx = (idx + 1) % table->capacity;
    }

    return NULL;
}

/**
 * @brief Vyhledá hodnotu podle řetězcového klíče.
 *
 * Přes buňky dead pokračuje; buňka empty nebo prohledání celé kapacity znamená neúspěch.
 * Obsah hodnoty nekopíruje a nepředává její vlastnictví.
 * @param table Prohledávaná tabulka.
 * @param key Hledaný klíč zakončený nulou.
 * @return Uložený ukazatel, nebo NULL při neúspěchu či neplatných argumentech.
 */
void *ht_search(hashtable *table, const char *key) {
    hash_item *item = find_item(table, key);
    // Vyhledávání vrací původní ukazatel na hodnotu bez kopírování a převodu vlastnictví.
    return item != NULL ? item->data : NULL;
}

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
bool ht_delete(hashtable *table, const char *key) {
    // Mazání používá stejnou posloupnost prohledávání jako běžné hledání.
    hash_item *item = find_item(table, key);
    if (item == NULL) {
        return false;
    }

    // Vlastněný klíč uvolníme, ale správa hodnoty zůstává volajícímu.
    free(item->key);

    item->key  = NULL;
    item->data = NULL;

    // Stav dead zachová dostupnost dalších položek posunutých kvůli kolizi.
    item->state = dead;

    // used se nemění: odstraněná buňka zůstává součástí prohledávaného úseku.
    table->size--;

    return true;
}

/**
 * @brief Zruší tabulku, její kopie klíčů a pole buněk.
 * @param table Tabulka vytvořená ht_create(); NULL se ignoruje.
 * @warning Uložené hodnoty se neuvolňují; jejich zrušení musí zajistit volající.
 * @warning Předaný ukazatel po návratu není platný; proměnná volajícího se automaticky nenastaví na
 * NULL.
 */
void ht_free(hashtable *table) {
    if (table == NULL) {
        return;
    }

    // Uvolníme klíče obsazených buněk; hodnoty jsou odpovědností volajícího.
    if (table->items != NULL) {
        for (size_t i = 0; i < table->capacity; i++) {
            if (table->items[i].state == full) {
                free(table->items[i].key);
            }
        }

        // Po uvolnění klíčů můžeme uvolnit samotné pole buněk.
        free(table->items);
    }

    // Vynulujeme vnitřní údaje objektu; ukazatel u volajícího tím nezměníme.
    table->items    = NULL;
    table->size     = 0;
    table->capacity = 0;
    table->used     = 0;

    // Nakonec uvolníme objekt vytvořený funkcí ht_create().
    free(table);
}

/**
 * @brief Zavolá obslužnou funkci pro každou obsazenou položku.
 *
 * Prázdné a odstraněné buňky se přeskakují. Pořadí není určeno; je-li stejný ukazatel uložen
 * pod více klíči, obsluha jej dostane jednou za každou položku.
 * @param table Procházená tabulka; NULL se ignoruje.
 * @param cb Obslužná funkce přijímající ukazatel na hodnotu; NULL se ignoruje.
 * @warning Obsluha nesmí měnit položky ani zrušit právě procházenou tabulku.
 * @warning Při uvolňování sdílených hodnot zabraňte opakovanému free(). Po uvolnění hodnot tabulku
 * zrušte; její datové ukazatele již nejsou platné.
 */
void ht_foreach(hashtable *table, ht_value_callback cb) {
    if (table == NULL || table->items == NULL || table->capacity == 0 || cb == NULL) {
        return;
    }

    // Pro každou obsazenou položku zavoláme obsluhu s jejím ukazatelem na hodnotu.
    for (size_t i = 0; i < table->capacity; i++) {
        hash_item *item = &table->items[i];

        /* Prázdné a odstraněné buňky se přeskakují.
         * Konkrétní práci s hodnotou určuje volající, například funkcí free_symbol(). */
        if (item->state == full) {
            cb(item->data);
        }
    }
}