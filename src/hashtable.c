/**
 * @file hashtable.c
 * @brief Hash table implementation using djb2 and linear probing.
 *
 * Public API contracts and ownership rules are documented in hashtable.h.
 */

#include "hashtable.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/** @brief Default cell count when the requested capacity is zero. */
#define HT_DEFAULT_CAPACITY 16

/**
 * @brief Cell states used by open addressing.
 *
 * empty: The cell has never been occupied since initialization or rehashing.
 * full:  The cell contains a valid entry.
 * dead:  The entry was deleted, but probing must continue through this cell.
 */
typedef enum { empty, full, dead } cell_state;

/**
 * @brief Internal hash table cell.
 *
 * The table owns the copied key.
 * The stored data pointer refers to a value owned by the caller.
 */
typedef struct {
    char      *key;
    void      *data;
    cell_state state;
} hash_item;

/**
 * @brief Private hash table representation.
 *
 * items points to the allocated cell array.
 * size counts occupied cells.
 * capacity is the total number of cells.
 * used counts occupied and deleted cells.
 */
struct hashtable {
    hash_item *items;
    size_t     size;
    size_t     capacity;
    size_t     used;
};

/**
 * @brief Computes the djb2 hash of a string.
 *
 * @param str Non-NULL, null-terminated string.
 * @return Hash value; the caller maps it to a cell using the capacity.
 */
static unsigned long hash_djb2(const char *str) {
    /* Start with the standard djb2 seed. */
    unsigned long hash = 5381;
    int           c;

    /*
     * Read each byte as an unsigned character.
     * The terminating zero ends the loop and is not included in the hash.
     */
    while ((c = (unsigned char)*str++)) {
        /*
         * Shifting left by five and adding the old hash multiplies it by 33.
         * Unsigned arithmetic wraps around when the result exceeds its range.
         */
        hash = ((hash << 5) + hash) + c;
    }

    return hash;
}

/**
 * @brief Initializes the internal array of an allocated table object.
 *
 * @param table Previously uninitialized object; NULL is accepted.
 * @param initial_capacity Initial cell count; 0 selects the default.
 *
 * @post For a non-NULL table, items is NULL on failure.
 * @warning Does not release existing resources. Do not reinitialize a live table.
 */
static void ht_init(hashtable *table, size_t initial_capacity) {
    if (table == NULL) {
        return;
    }

    /* Zero requests the default capacity rather than an array with no cells. */
    if (initial_capacity == 0) {
        initial_capacity = HT_DEFAULT_CAPACITY;
    }

    /*
     * Check the multiplication before calculating the allocation size.
     * The requested cell count must fit into a size_t byte count.
     */
    if (initial_capacity > SIZE_MAX / sizeof(hash_item)) {
        table->capacity = 0;
        table->size     = 0;
        table->used     = 0;
        table->items    = NULL;
        return;
    }

    /* Allocate the cell array separately from the table object. */
    table->items = malloc(initial_capacity * sizeof(hash_item));
    if (table->items == NULL) {
        /*
         * malloc has already stored NULL in items.
         * ht_create checks this field to detect initialization failure.
         */
        table->capacity = 0;
        table->size     = 0;
        table->used     = 0;
        return;
    }

    /*
     * Initialize every cell before any operation can inspect its state.
     * Empty cells do not own a key or refer to a stored value.
     */
    for (size_t i = 0; i < initial_capacity; i++) {
        table->items[i].key   = NULL;
        table->items[i].data  = NULL;
        table->items[i].state = empty;
    }

    /* A new table contains neither live entries nor deleted cells. */
    table->capacity = initial_capacity;
    table->size     = 0;
    table->used     = 0;
}

/**
 * @brief Moves an existing entry into a new array during rehashing.
 *
 * @param items Initialized destination array containing an available cell.
 * @param capacity Nonzero destination capacity.
 * @param key Existing key pointer; the key is not copied.
 * @param data Existing value pointer.
 *
 * @note Does not allocate memory or check for duplicate keys.
 */
static void ht_insert_raw(hash_item *items, size_t capacity, char *key, void *data) {
    /* Map the existing key to its initial position in the destination array. */
    size_t idx = hash_djb2(key) % capacity;

    /*
     * Resolve collisions using linear probing.
     * The caller guarantees that an available cell exists.
     */
    while (items[idx].state == full) {
        idx = (idx + 1) % capacity;
    }

    /*
     * Transfer the existing pointers without copying their contents.
     * Ownership of the key moves to the destination array.
     */
    items[idx].key   = key;
    items[idx].data  = data;
    items[idx].state = full;
}

/**
 * @brief Doubles the capacity and rehashes occupied entries.
 *
 * @param table Valid, initialized table.
 * @return true on success; false on size overflow or allocation failure.
 *
 * @note Transfers key and value pointers without copying their contents.
 * @post On success, used equals size. On failure, the table is unchanged.
 */
static bool ht_resize(hashtable *table) {
    /* Check that both capacity doubling and the allocation size fit. */
    if (table->capacity > SIZE_MAX / 2 / sizeof(hash_item)) {
        return false;
    }

    /*
     * Allocate the replacement while the original array is still intact.
     * Allocation failure therefore leaves the original table usable.
     */
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
     * Only occupied entries are transferred; deleted cells disappear.
     */
    for (size_t i = 0; i < table->capacity; i++) {
        if (table->items[i].state == full) {
            ht_insert_raw(new_items, new_capacity, table->items[i].key, table->items[i].data);
        }
    }

    /*
     * Release only the old array.
     * Its key and value pointers have already been transferred.
     */
    free(table->items);

    table->items    = new_items;
    table->capacity = new_capacity;

    /* No tombstones remain, so every used cell is occupied. */
    table->used = table->size;

    return true;
}

hashtable *ht_create(size_t initial_capacity) {
    hashtable *table;

    /* Allocate the outer object before initializing its cell array. */
    table = (hashtable *)malloc(sizeof(hashtable));
    if (table == NULL) {
        return NULL;
    }

    /*
     * ht_init prepares the cell array and counters.
     * It also handles the default capacity and allocation-size validation.
     */
    ht_init(table, initial_capacity);
    if (table->items == NULL) {
        /* Array initialization failed; release the already allocated object. */
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

    /*
     * Remember the first reusable deleted cell.
     * capacity is a sentinel because valid indices end at capacity - 1.
     */
    size_t first_dead = table->capacity;

    /*
     * Search for an existing key before inserting.
     * Bound probing by the cell count so an unsuccessful scan always ends.
     */
    for (size_t i = 0; i < table->capacity; i++) {
        hash_item *item = &table->items[idx];

        /* A never-used cell ends the search for an existing key. */
        if (item->state == empty) {
            break;
        }

        if (item->state == dead) {
            /*
             * Save the first tombstone, but continue searching.
             * Inserting immediately could create a duplicate key.
             */
            if (first_dead == table->capacity) {
                first_dead = idx;
            }
        } else if (strcmp(item->key, key) == 0) {
            /*
             * The key already exists: replace only its value pointer.
             * The old value is not freed, and entry counts stay unchanged.
             */
            item->data = data;
            return true;
        }

        /* Move to the next cell, wrapping around at the array boundary. */
        idx = (idx + 1) % table->capacity;
    }

    bool reuse_dead = (first_dead != table->capacity);

    /*
     * Grow before consuming a new empty cell above 75% used occupancy.
     * Reusing a tombstone does not increase the used-cell count.
     */
    if (!reuse_dead && (table->used + 1) * 4 > table->capacity * 3) {
        if (!ht_resize(table)) {
            return false;
        }

        /*
         * Resizing changed the capacity and removed tombstones.
         * Find a destination again using the new modulus.
         */
        idx = hash_djb2(key) % table->capacity;
        while (table->items[idx].state == full) {
            idx = (idx + 1) % table->capacity;
        }
    } else if (reuse_dead) {
        /* Prefer the first reusable tombstone found in the probe chain. */
        idx = first_dead;
    } else if (table->items[idx].state != empty) {
        /* No destination is available for a new entry. */
        return false;
    }

    /*
     * Own a copy of the key, including its terminating zero.
     * The stored value remains the caller-provided pointer.
     */
    char *key_copy = malloc(strlen(key) + 1);
    if (key_copy == NULL) {
        return false;
    }

    strcpy(key_copy, key);

    /* Publish the entry only after its key allocation has succeeded. */
    table->items[idx].key   = key_copy;
    table->items[idx].data  = data;
    table->items[idx].state = full;
    table->size++;

    /* A reused tombstone was already included in used. */
    if (!reuse_dead) {
        table->used++;
    }

    return true;
}

void *ht_search(hashtable *table, const char *key) {
    if (table == NULL || table->items == NULL || table->capacity == 0 || key == NULL) {
        return NULL;
    }

    size_t idx = hash_djb2(key) % table->capacity;

    /* Deleted cells keep the probe chain intact and must be skipped. */
    for (size_t i = 0; i < table->capacity; i++) {
        hash_item *item = &table->items[idx];

        /* No matching key can occur beyond the first never-used cell. */
        if (item->state == empty) {
            return NULL;
        }

        /* Only occupied cells contain a key that may be compared. */
        if (item->state == full && strcmp(item->key, key) == 0) {
            return item->data;
        }

        idx = (idx + 1) % table->capacity;
    }

    /* A complete scan found no matching occupied entry. */
    return NULL;
}

bool ht_delete(hashtable *table, const char *key) {
    if (table == NULL || table->items == NULL || table->capacity == 0 || key == NULL) {
        return false;
    }

    size_t idx = hash_djb2(key) % table->capacity;

    for (size_t i = 0; i < table->capacity; i++) {
        hash_item *item = &table->items[idx];

        /* A never-used cell proves that the key is absent. */
        if (item->state == empty) {
            return false;
        }

        if (item->state == full && strcmp(item->key, key) == 0) {
            /*
             * Release the copied key.
             * Value cleanup remains the caller's responsibility.
             */
            free(item->key);

            item->key  = NULL;
            item->data = NULL;

            /*
             * Mark the cell as deleted rather than empty.
             * Otherwise searches could miss later colliding entries.
             */
            item->state = dead;

            /*
             * The number of live entries decreases.
             * used stays unchanged because the tombstone still occupies a cell.
             */
            table->size--;
            return true;
        }

        idx = (idx + 1) % table->capacity;
    }

    return false;
}

void ht_free(hashtable *table) {
    if (table == NULL) {
        return;
    }

    /*
     * Free copied keys from occupied entries.
     * Deleted entries have already released their keys.
     * Stored values are never freed here.
     */
    if (table->items != NULL) {
        for (size_t i = 0; i < table->capacity; i++) {
            if (table->items[i].state == full) {
                free(table->items[i].key);
            }
        }

        /* Key allocations are gone; release the cell array itself. */
        free(table->items);
    }

    /*
     * Reset this object before releasing it.
     * These assignments do not reset the caller's pointer.
     */
    table->items    = NULL;
    table->size     = 0;
    table->capacity = 0;
    table->used     = 0;

    /* Release the outer object originally allocated by ht_create. */
    free(table);
}

void ht_foreach(hashtable *table, ht_value_callback cb) {
    if (table == NULL || table->items == NULL || table->capacity == 0 || cb == NULL) {
        return;
    }

    /*
     * Scan the physical array rather than following a particular probe chain.
     * This visits every occupied entry exactly once.
     */
    for (size_t i = 0; i < table->capacity; i++) {
        hash_item *item = &table->items[i];

        /* Skip both never-used cells and tombstones. */
        if (item->state == full) {
            /*
             * Pass the stored value to the caller-supplied operation.
             * For example, symtable uses this to release symbol records.
             * The callback must not modify the table's entries or destroy it.
             */
            cb(item->data);
        }
    }
}