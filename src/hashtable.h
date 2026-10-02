#ifndef HASHTABLE_H
#define HASHTABLE_H

#include <stdbool.h>
#include <stdlib.h>

typedef enum { empty, full, dead } cell_state;

typedef struct {
    char      *key;
    void      *data;
    cell_state state;
} hash_item;

typedef struct {
    hash_item *items;
    size_t     size;
    size_t     capacity;
    size_t     used;
} hashtable;

unsigned long hash_djb2(const char *str);

void  ht_init(hashtable *table, size_t initial_capacity);
bool  ht_insert(hashtable *table, const char *key, void *data);
void *ht_search(hashtable *table, const char *key);
void  ht_free(hashtable *table);

#endif // HASHTABLE_H