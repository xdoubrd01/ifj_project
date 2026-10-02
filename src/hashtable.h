#ifndef HASHTABLE_H
#define HASHTABLE_H

#include <stdbool.h>
#include <stdlib.h>

// Cell states for open addressing collision resolution
typedef enum { empty, full, dead } cell_state;

// Structure of a single item in the hash table
typedef struct {
    char      *key;
    void      *data;
    cell_state state;
} hash_item;

// Hash table structure
typedef struct {
    hash_item *items;
    size_t     size;
    size_t     capacity;
    size_t     used;
} hashtable;

// Hash function
unsigned long hash_djb2(const char *str);

// Table interface
void  ht_init(hashtable *table, size_t initial_capacity);
bool  ht_insert(hashtable *table, const char *key, void *data);
void *ht_search(hashtable *table, const char *key);
void  ht_free(hashtable *table);

#endif // HASHTABLE_H