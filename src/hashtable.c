#include "hashtable.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define HT_DEFAULT_CAPACITY 16U
#define HT_MIN_CAPACITY 8U

typedef enum { CELL_EMPTY, CELL_FULL, CELL_DEAD } cell_state;

typedef struct {
    char      *key;
    void      *value;
    cell_state state;
} hash_item;

struct hashtable {
    hash_item *items;
    size_t     capacity;
    size_t     size;
    size_t     used; /* Full cells plus tombstones. */
};

static size_t hash_djb2(const char *key) {
    size_t hash = 5381;

    while (*key != '\0') {
        hash = hash * 33U + (unsigned char)*key++;
    }

    return hash;
}

hashtable *ht_create(size_t initial_capacity) {
    size_t capacity = initial_capacity == 0 ? HT_DEFAULT_CAPACITY : initial_capacity;

    if (capacity < HT_MIN_CAPACITY) {
        capacity = HT_MIN_CAPACITY;
    }

    if (capacity > SIZE_MAX / sizeof(hash_item)) {
        return NULL;
    }

    hashtable *table = malloc(sizeof(*table));
    if (table == NULL) {
        return NULL;
    }

    table->items = calloc(capacity, sizeof(*table->items));
    if (table->items == NULL) {
        free(table);
        return NULL;
    }

    table->capacity = capacity;
    table->size     = 0;
    table->used     = 0;

    return table;
}

void ht_destroy(hashtable *table, ht_destructor destroy_value) {
    if (table == NULL) {
        return;
    }

    for (size_t i = 0; i < table->capacity; ++i) {
        if (table->items[i].state == CELL_FULL) {
            if (destroy_value != NULL) {
                destroy_value(table->items[i].value);
            }

            free(table->items[i].key);
        }
    }

    free(table->items);
    free(table);
}

size_t ht_size(const hashtable *table) { return table == NULL ? 0 : table->size; }

/*
 * Returns an existing key's cell, a usable cell, or capacity if full.
 * Probing continues past tombstones to detect duplicate keys.
 */
static size_t find_slot(const hashtable *table, const char *key, bool *found) {
    size_t index      = hash_djb2(key) % table->capacity;
    size_t first_dead = table->capacity;

    *found = false;

    for (size_t n = 0; n < table->capacity; ++n) {
        const hash_item *item = &table->items[index];

        if (item->state == CELL_EMPTY) {
            return first_dead == table->capacity ? index : first_dead;
        }

        if (item->state == CELL_DEAD) {
            if (first_dead == table->capacity) {
                first_dead = index;
            }
        } else if (strcmp(item->key, key) == 0) {
            *found = true;
            return index;
        }

        index = (index + 1) % table->capacity;
    }

    return first_dead;
}

static bool rehash(hashtable *table, size_t capacity) {
    if (capacity > SIZE_MAX / sizeof(hash_item)) {
        return false;
    }

    hash_item *items = calloc(capacity, sizeof(*items));
    if (items == NULL) {
        return false;
    }

    for (size_t i = 0; i < table->capacity; ++i) {
        if (table->items[i].state == CELL_FULL) {
            size_t index = hash_djb2(table->items[i].key) % capacity;

            while (items[index].state == CELL_FULL) {
                index = (index + 1) % capacity;
            }

            items[index] = table->items[i];
        }
    }

    free(table->items);

    table->items    = items;
    table->capacity = capacity;
    table->used     = table->size;

    return true;
}

bool ht_insert(hashtable *table, const char *key, void *value) {
    if (table == NULL || key == NULL || value == NULL) {
        return false;
    }

    bool   found;
    size_t index = find_slot(table, key, &found);

    if (found) {
        table->items[index].value = value;
        return true;
    }

    /* floor(3 * capacity / 4), without multiplication overflow. */
    size_t limit = (table->capacity / 4) * 3 + (table->capacity % 4) * 3 / 4;

    bool adds_used = index == table->capacity || table->items[index].state == CELL_EMPTY;

    if (adds_used && table->used >= limit) {
        size_t capacity = table->capacity;

        if (table->size >= limit) {
            if (capacity > SIZE_MAX / sizeof(hash_item) / 2) {
                return false;
            }

            capacity *= 2;
        }

        /*
         * Keep the same capacity if tombstones, rather than live entries,
         * caused the occupancy limit to be reached.
         */
        if (!rehash(table, capacity)) {
            return false;
        }

        index = find_slot(table, key, &found);
    }

    if (index == table->capacity) {
        return false;
    }

    size_t length = strlen(key);
    if (length == SIZE_MAX) {
        return false;
    }

    char *copy = malloc(length + 1);
    if (copy == NULL) {
        return false;
    }

    memcpy(copy, key, length + 1);

    if (table->items[index].state == CELL_EMPTY) {
        ++table->used;
    }

    table->items[index] = (hash_item){copy, value, CELL_FULL};

    ++table->size;
    return true;
}

void *ht_search(const hashtable *table, const char *key) {
    if (table == NULL || key == NULL) {
        return NULL;
    }

    bool   found;
    size_t index = find_slot(table, key, &found);

    return found ? table->items[index].value : NULL;
}

bool ht_delete(hashtable *table, const char *key) {
    if (table == NULL || key == NULL) {
        return false;
    }

    bool   found;
    size_t index = find_slot(table, key, &found);

    if (!found) {
        return false;
    }

    free(table->items[index].key);

    table->items[index] = (hash_item){NULL, NULL, CELL_DEAD};

    --table->size;
    return true;
}