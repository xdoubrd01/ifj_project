/**
 * @authors Denis Doubrava xdoubrd01
 *         Eliáš Oharek   xohare00
 * Tvorba komentářů byla za pomoci umělé inteligence: překlad, formátování a
 * lepší formulace vět
 */

/**
 * @file hashtable.c
 * @brief Hash table implementation using djb2 and linear probing.
 *
 * Public API contracts and ownership rules are documented in hashtable.h.
 */
#include "hashtable.h"
#include "str_utils.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/** @brief Default cell count when the requested capacity is zero. */
#define HT_DEFAULT_CAPACITY 16

/**
 * @brief Cell states used by open addressing.
 * @details A deleted cell keeps the probe chain intact.
 */
typedef enum { empty, full, dead } cell_state;

/** @brief Internal cell containing an owned key and a caller-owned value. */
typedef struct {
    char      *key;
    void      *data;
    cell_state state;
} hash_item;

/**
 * @brief Private hash table representation.
 * @details size counts occupied cells; used counts occupied and deleted cells.
 */
struct hashtable {
    hash_item *items;
    size_t     size;
    size_t     capacity;
    size_t     used;
};

/**
 * @brief Computes the djb2 hash of a string.
 * @param str Non-NULL, null-terminated string.
 * @return Hash value; the caller maps it to a cell using the capacity.
 */
static unsigned long hash_djb2(const char *str) {
    /* Start with the djb2 seed and fold each input byte into the hash. */
    unsigned long hash = 5381;
    int           c;

    while ((c = (unsigned char)*str++)) {
        /*
         * Shifting by five and adding the old hash multiplies it by 33.
         * Unsigned arithmetic wraps around when the result exceeds its range.
         */
        hash = ((hash << 5) + hash) + c;
    }

    return hash;
}

/**
 * @brief Initializes the internal array of an allocated table object.
 * @param table Previously uninitialized object; NULL is accepted.
 * @param initial_capacity Initial cell count; 0 selects the default.
 * @post For a non-NULL table, items is NULL on failure.
 * @warning Does not release existing resources. Do not reinitialize a live
 * table.
 */
static void ht_init(hashtable *table, size_t initial_capacity) {
    if (table == NULL) {
        return;
    }

    /* Zero is a request for the default, not an array with no cells. */
    if (initial_capacity == 0) {
        initial_capacity = HT_DEFAULT_CAPACITY;
    }

    /* Reject sizes whose allocation would overflow size_t. */
    if (initial_capacity > SIZE_MAX / sizeof(hash_item)) {
        table->capacity = 0;
        table->size     = 0;
        table->used     = 0;
        table->items    = NULL;
        return;
    }

    /* Allocate the cell array separately from the table object. */
    table->items = malloc(initial_capacity * sizeof(hash_item));

    /* malloc already stored NULL in items; ht_create checks that field. */
    if (table->items == NULL) {
        table->capacity = 0;
        table->size     = 0;
        table->used     = 0;
        return;
    }

    /* Mark every cell as never used before accepting any entries. */
    for (size_t i = 0; i < initial_capacity; i++) {
        table->items[i].key   = NULL;
        table->items[i].data  = NULL;
        table->items[i].state = empty;
    }

    /* No live entries or tombstones exist in a newly initialized array. */
    table->capacity = initial_capacity;
    table->size     = 0;
    table->used     = 0;
}

/**
 * @brief Moves an existing entry into a new array during rehashing.
 * @param items Initialized destination array containing an available cell.
 * @param capacity Nonzero destination capacity.
 * @param key Existing key pointer; the key is not copied.
 * @param data Existing value pointer.
 * @note Does not allocate memory or check for duplicate keys.
 */
static void ht_insert_raw(hash_item *items, size_t capacity, char *key, void *data) {
    size_t idx = hash_djb2(key) % capacity;

    /* Probe linearly, wrapping around at the end of the new array. */
    while (items[idx].state == full) {
        idx = (idx + 1) % capacity;
    }

    /* Reuse the original allocations; ownership moves to the new array. */
    items[idx].key   = key;
    items[idx].data  = data;
    items[idx].state = full;
}

/**
 * @brief Doubles the capacity and rehashes occupied entries.
 * @param table Valid, initialized table.
 * @return true on success; false on size overflow or allocation failure.
 * @note Transfers key and value pointers without copying their contents.
 * @post On success, used equals size. On failure, the table is unchanged.
 */
static bool ht_resize(hashtable *table) {
    /* Check that both capacity doubling and the allocation size fit. */
    if (table->capacity > SIZE_MAX / 2 / sizeof(hash_item)) {
        return false;
    }

    /* Allocate the replacement while the original array is still intact. */
    size_t     new_capacity = table->capacity * 2;
    hash_item *new_items    = malloc(new_capacity * sizeof(hash_item));

    if (new_items == NULL) {
        return false;
    }

    /* Prepare an empty destination array for rehashing. */
    for (size_t i = 0; i < new_capacity; i++) {
        new_items[i].key   = NULL;
        new_items[i].data  = NULL;
        new_items[i].state = empty;
    }

    /*
     * Recompute positions because the modulus depends on capacity.
     * Transfer existing pointers; do not allocate new keys or values.
     */
    for (size_t i = 0; i < table->capacity; i++) {
        if (table->items[i].state == full) {
            ht_insert_raw(new_items, new_capacity, table->items[i].key, table->items[i].data);
        }
    }

    /* Release only the old array: its key and value pointers were transferred. */
    free(table->items);

    table->items    = new_items;
    table->capacity = new_capacity;
    table->used     = table->size; // Deleted cells have been removed.

    return true;
}

hashtable *ht_create(size_t initial_capacity) {
    hashtable *table;

    /* Allocate the table object; ht_init allocates its cell array. */
    table = (hashtable *)malloc(sizeof(hashtable));

    if (table == NULL) {
        return NULL;
    }

    /* If array initialization fails, release the allocated object. */
    ht_init(table, initial_capacity);

    if (table->items == NULL) {
        free(table);
        return NULL;
    }

    /* Both allocations succeeded; transfer the table object to the caller. */
    return table;
}

bool ht_insert(hashtable *table, const char *key, void *data) {
    if (table == NULL || table->items == NULL || table->capacity == 0 || key == NULL ||
        data == NULL) {
        return false;
    }

    size_t idx = hash_djb2(key) % table->capacity;

    /* Remember a reusable deleted cell, but keep checking for duplicates. */
    /* capacity is a sentinel because valid indices end at capacity - 1. */
    size_t first_dead = table->capacity;

    /* Bound probing by the cell count so an unsuccessful scan always ends. */
    for (size_t i = 0; i < table->capacity; i++) {
        hash_item *item = &table->items[idx];

        /* A never-used cell ends the search for an existing key. */
        if (item->state == empty) {
            break;
        }

        /*
         * Save the first tombstone, but search further for the same key.
         * Inserting immediately could create a duplicate entry.
         */
        if (item->state == dead) {
            if (first_dead == table->capacity) {
                first_dead = idx;
            }
        } else if (strcmp(item->key, key) == 0) {
            /* Replace only the value pointer; do not free the old value. */
            item->data = data;
            return true;
        }

        idx = (idx + 1) % table->capacity;
    }

    /* Reusing a deleted cell does not increase the used-cell count. */
    bool reuse_dead = (first_dead != table->capacity);

    /* Grow before consuming a new empty cell above 75% used occupancy. */
    if (!reuse_dead && (table->used + 1) * 4 > table->capacity * 3) {
        if (!ht_resize(table)) {
            return false;
        }

        /* Resizing changed the capacity, so find the destination again. */
        idx = hash_djb2(key) % table->capacity;

        while (table->items[idx].state == full) {
            idx = (idx + 1) % table->capacity;
        }
    } else if (reuse_dead) {
        /* Prefer the first reusable tombstone found in the probe chain. */
        idx = first_dead;
    } else if (table->items[idx].state != empty) {
        return false;
    }

    char *key_copy = string_duplicate(key);

    if (key_copy == NULL) {
        return false;
    }

    /* Publish the new entry only after its key allocation has succeeded. */
    table->items[idx].key   = key_copy;
    table->items[idx].data  = data;
    table->items[idx].state = full;
    table->size++;

    /* Deleted cells were already included in used. */
    if (!reuse_dead) {
        table->used++;
    }

    return true;
}

/** @brief Finds an occupied cell without changing the table. */
static hash_item *find_item(hashtable *table, const char *key) {
    if (table == NULL || table->items == NULL || table->capacity == 0 || key == NULL) {
        return NULL;
    }

    size_t idx = hash_djb2(key) % table->capacity;

    /* Never-used cells end the search; deleted cells do not. */
    for (size_t i = 0; i < table->capacity; i++) {
        hash_item *item = &table->items[idx];

        if (item->state == empty) {
            return NULL;
        }

        /* Only occupied cells contain a key that may be compared. */
        if (item->state == full && strcmp(item->key, key) == 0) {
            return item;
        }

        idx = (idx + 1) % table->capacity;
    }

    return NULL;
}

void *ht_search(hashtable *table, const char *key) {
    hash_item *item = find_item(table, key);
    return item != NULL ? item->data : NULL;
}

bool ht_delete(hashtable *table, const char *key) {
    hash_item *item = find_item(table, key);
    if (item == NULL) {
        return false;
    }

    /* Release the owned key, but leave value cleanup to the caller. */
    free(item->key);

    item->key  = NULL;
    item->data = NULL;

    /* A tombstone preserves access to later colliding entries. */
    item->state = dead;

    /* used stays unchanged: a deleted cell still belongs to the chain. */
    table->size--;

    return true;
}

void ht_free(hashtable *table) {
    if (table == NULL) {
        return;
    }

    /* Free keys from live entries. Values are owned by the caller. */
    if (table->items != NULL) {
        for (size_t i = 0; i < table->capacity; i++) {
            if (table->items[i].state == full) {
                free(table->items[i].key);
            }
        }

        /* Key allocations are gone, so the cell array itself can be released. */
        free(table->items);
    }

    /* Reset this object before releasing it; this does not reset the caller. */
    table->items    = NULL;
    table->size     = 0;
    table->capacity = 0;
    table->used     = 0;

    /* Finally release the table object created by ht_create. */
    free(table);
}

void ht_foreach(hashtable *table, ht_value_callback cb) {
    if (table == NULL || table->items == NULL || table->capacity == 0 || cb == NULL) {
        return;
    }

    /* Invoke the callback once per occupied entry, passing its value. */
    for (size_t i = 0; i < table->capacity; i++) {
        hash_item *item = &table->items[i];

        /*
         * Empty and deleted cells are skipped, even if data is NULL there.
         * Callback behavior is supplied by the caller, for example free_symbol.
         */
        if (item->state == full) {
            cb(item->data);
        }
    }
}