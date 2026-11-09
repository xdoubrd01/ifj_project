#ifndef HASHTABLE_H
#define HASHTABLE_H

#include <stdbool.h>
#include <stddef.h>

/* Open addressing, djb2, linear probing. Representation is private. */
typedef struct hashtable hashtable;
typedef void (*ht_destructor)(void *value);

/* Zero capacity selects a default. Returns NULL on allocation failure. */
hashtable *ht_create(size_t initial_capacity);

/* Destroys keys and table; optionally calls destroy_value on each value. */
void ht_destroy(hashtable *table, ht_destructor destroy_value);

size_t ht_size(const hashtable *table);

/*
 * Copies key. Values must be non-NULL.
 * Replaces an existing value without freeing it.
 * The caller must retain/free the previous value as appropriate.
 * Returns false on invalid input or allocation failure.
 */
bool ht_insert(hashtable *table, const char *key, void *value);

/* Returned value is borrowed; NULL means absent or invalid input. */
void *ht_search(const hashtable *table, const char *key);

/* Frees the copied key, not the value. Save the value before deleting it. */
bool ht_delete(hashtable *table, const char *key);

#endif