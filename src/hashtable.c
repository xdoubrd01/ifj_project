#include "hashtable.h"
#include <string.h>

unsigned long hash_djb2(const char *str) {
    unsigned long hash = 5381;
    int           c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}

// TODO: Implement memory allocation for the `items` array and set initial cell states to `empty`.
void ht_init(hashtable *table, size_t initial_capacity) {
    // Implementation here
}

// TODO: Implement table resizing (rehashing) when the allowed load factor is exceeded
// (prevents performance degradation in open addressing).
static void ht_resize(hashtable *table) {
    // Implementation here
}

// TODO: Implement item insertion with collision resolution using open addressing (linear/quadratic
// probing).
bool ht_insert(hashtable *table, const char *key, void *data) {
    // Implementation here
    return false;
}

// TODO: Implement item search by key, remember to skip `dead` cells.
void *ht_search(hashtable *table, const char *key) {
    // Implementation here
    return NULL;
}

// TODO: Implement safe deallocation of all allocated keys, stored data structures, and
// the array itself.
void ht_free(hashtable *table) {
    // Implementation here
}