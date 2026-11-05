#include "hashtable.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define HT_DEFAULT_CAPACITY 16

unsigned long hash_djb2(const char *str) {
    unsigned long hash = 5381;
    int           c;
    while ((c = (unsigned char)*str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}

void ht_init(hashtable *table, size_t initial_capacity) {
    if (table == NULL) {
        return;
    }
    if (initial_capacity == 0) {
        initial_capacity = HT_DEFAULT_CAPACITY;
    }

    if (initial_capacity > SIZE_MAX / sizeof(hash_item)) {
        table->capacity = 0;
        table->size     = 0;
        table->used     = 0;
        table->items    = NULL;
        return;
    }

    table->items = malloc(initial_capacity * sizeof(hash_item));
    if (table->items == NULL) {
        table->capacity = 0;
        table->size     = 0;
        table->used     = 0;
        return;
    }

    for (size_t i = 0; i < initial_capacity; i++) {
        table->items[i].key   = NULL;
        table->items[i].data  = NULL;
        table->items[i].state = empty;
    }
    table->capacity = initial_capacity;
    table->size     = 0;
    table->used     = 0;
}

static void ht_insert_raw(hash_item *items, size_t capacity, char *key, void *data) {
    size_t idx = hash_djb2(key) % capacity;
    while (items[idx].state == full) {
        idx = (idx + 1) % capacity;
    }
    items[idx].key   = key;
    items[idx].data  = data;
    items[idx].state = full;
}

static bool ht_resize(hashtable *table) {
    if (table->capacity > SIZE_MAX / 2 / sizeof(hash_item)) {
        return false;
    }
    size_t     new_capacity = table->capacity * 2;
    hash_item *new_items    = malloc(new_capacity * sizeof(hash_item));
    if (new_items == NULL) {
        return false;
    }

    for (size_t i = 0; i < new_capacity; i++) {
        new_items[i].key   = NULL;
        new_items[i].data  = NULL;
        new_items[i].state = empty;
    }

    for (size_t i = 0; i < table->capacity; i++) {
        if (table->items[i].state == full) {
            ht_insert_raw(new_items, new_capacity, table->items[i].key, table->items[i].data);
        }
    }

    free(table->items);
    table->items    = new_items;
    table->capacity = new_capacity;
    table->used     = table->size; // dead buňky zmizely
    return true;
}

bool ht_insert(hashtable *table, const char *key, void *data) {
    if (table == NULL || table->items == NULL || table->capacity == 0 || key == NULL || data == NULL) {
        return false;
    }

    size_t idx        = hash_djb2(key) % table->capacity;
    size_t first_dead = table->capacity;

    for (size_t i = 0; i < table->capacity; i++) {
        hash_item *item = &table->items[idx];

        if (item->state == empty) {
            break;
        }
        if (item->state == dead) {
            if (first_dead == table->capacity) {
                first_dead = idx;
            }
        } else if (strcmp(item->key, key) == 0) {
            item->data = data;
            return true;
        }
        idx = (idx + 1) % table->capacity;
    }


    bool reuse_dead = (first_dead != table->capacity);
    if (!reuse_dead && (table->used + 1) * 4 > table->capacity * 3) {
        if (!ht_resize(table)) {
            return false;
        }

        idx = hash_djb2(key) % table->capacity;
        while (table->items[idx].state == full) {
            idx = (idx + 1) % table->capacity;
        }
    } else if (reuse_dead) {
        idx = first_dead;
    } else if (table->items[idx].state != empty) {
        return false;
    }

    char *key_copy = malloc(strlen(key) + 1);
    if (key_copy == NULL) {
        return false;
    }
    strcpy(key_copy, key);

    table->items[idx].key   = key_copy;
    table->items[idx].data  = data;
    table->items[idx].state = full;
    table->size++;
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
    for (size_t i = 0; i < table->capacity; i++) {
        hash_item *item = &table->items[idx];

        if (item->state == empty) {
            return NULL;
        }
        if (item->state == full && strcmp(item->key, key) == 0) {
            return item->data;
        }
        idx = (idx + 1) % table->capacity;
    }
    return NULL;
}

bool ht_delete(hashtable *table, const char *key) {
    if (table == NULL || table->items == NULL || table->capacity == 0 || key == NULL) {
        return false;
    }

    size_t idx = hash_djb2(key) % table->capacity;

    for (size_t i = 0; i < table->capacity; i++) {
        hash_item *item = &table->items[idx];

        if (item->state == empty) {
            return false;
        }

        if (item->state == full && strcmp(item->key, key) == 0) {
            free(item->key);

            item->key   = NULL;
            item->data  = NULL;
            item->state = dead;

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
    if (table->items != NULL) {
        for (size_t i = 0; i < table->capacity; i++) {
            if (table->items[i].state == full) {
                free(table->items[i].key);
            }
        }
        free(table->items);
    }
    table->items    = NULL;
    table->size     = 0;
    table->capacity = 0;
    table->used     = 0;
}