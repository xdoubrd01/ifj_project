#include "hashtable.h"
#include <string.h>

unsigned long hash_djb2(const char *str)
{
    unsigned long hash = 5381;
    int           c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}

// TODO: Implementovat alokaci paměti pro pole `items` a nastavit počáteční stavy buněk na `empty`.
void ht_init(hashtable *table, size_t initial_capacity)
{
    // Implementace zde
}

// TODO: Implementovat zvětšení tabulky (rehashování) při překročení povoleného load factoru
// (zabrání degradaci výkonu u otevřené adresace).
static void ht_resize(hashtable *table)
{
    // Implementace zde
}

// TODO: Implementovat vkládání položky s řešením kolizí pomocí otevřené adresace (linear/quadratic
// probing).
bool ht_insert(hashtable *table, const char *key, void *data)
{
    // Implementace zde
    return false;
}

// TODO: Implementovat vyhledávání položky podle klíče, pamatovat na přeskakování `dead` buněk.
void *ht_search(hashtable *table, const char *key)
{
    // Implementace zde
    return NULL;
}

// TODO: Implementovat bezpečné uvolnění všech alokovaných klíčů, uložených datových struktur a
// samotného pole.
void ht_free(hashtable *table)
{
    // Implementace zde
}