/**
 *@authors Denis Doubrava xdoubrd01
 *         Eliáš Oharek   xohare00
 * Tvorba komentářů byla za pomoci umělé inteligence: překlad, formátování a lepší formulace vět
 */

/**
 * @file hashtable.h
 * @brief Generic hash table with open addressing and string keys.
 *
 * The table owns copies of its keys. Stored values are non-NULL pointers
 * owned by the caller. The internal representation is hidden.
 */

#ifndef HASHTABLE_H
#define HASHTABLE_H

#include <stdbool.h>
#include <stddef.h>

struct hashtable;

/** @brief Opaque hash table type. */
typedef struct hashtable hashtable;

/**
 * @brief Callback type for processing one stored value.
 *
 * The callback receives a stored data pointer and returns no value.
 */
typedef void (*ht_value_callback)(void *);

/**
 * @brief Creates an empty hash table.
 *
 * @param initial_capacity Initial number of cells; 0 selects the default.
 * @return A new table, or NULL if creation fails.
 *
 * @note The caller must eventually destroy the table using ht_free().
 */
hashtable *ht_create(size_t initial_capacity);

/**
 * @brief Invokes a callback once for each occupied entry.
 *
 * @param ht Table to traverse. NULL is accepted and does nothing.
 * @param cb Callback to invoke. NULL is accepted and does nothing.
 *
 * @note Traversal order is unspecified. Empty and deleted cells are skipped.
 * @note A pointer shared by multiple entries is passed once per entry.
 *
 * @warning The callback must not insert, delete, or replace entries,
 * or destroy the table being traversed.
 * @warning If the callback frees values, it must avoid freeing shared values
 * more than once. When disposing of values, destroy the table afterwards
 * using ht_free(); the stored value pointers are no longer valid.
 */
void ht_foreach(hashtable *ht, ht_value_callback cb);

/**
 * @brief Inserts an entry or replaces the value of an existing key.
 *
 * @param table Target table; must not be NULL.
 * @param key Null-terminated key; must not be NULL.
 * @param data Value pointer; must not be NULL.
 * @return true on success; false on invalid input or allocation failure.
 *
 * @note A new key is copied. The value itself is not copied.
 * @warning Replacing a value does not free the previous value.
 * The caller remains responsible for its lifetime.
 */
bool ht_insert(hashtable *table, const char *key, void *data);

/**
 * @brief Finds a value by its key.
 *
 * @param table Table to search.
 * @param key Null-terminated key to find.
 * @return The stored pointer, or NULL if absent or an argument is NULL.
 *
 * @note No value is copied and no ownership is transferred.
 */
void *ht_search(hashtable *table, const char *key);

/**
 * @brief Removes an entry by its key.
 *
 * @param table Table from which to remove the entry.
 * @param key Null-terminated key to remove.
 * @return true if removed; false if absent or an argument is NULL.
 *
 * @note Frees the copied key, but does not free the stored value.
 * Retrieve the value before removal if its pointer is needed for cleanup.
 */
bool ht_delete(hashtable *table, const char *key);

/**
 * @brief Destroys the table, its copied keys, and its cell array.
 *
 * @param table Table created by ht_create(). NULL is accepted.
 *
 * @warning Stored values are not freed; the caller is responsible for them.
 * @warning The table pointer becomes invalid after destruction.
 * This function does not reset the caller's pointer to NULL.
 */
void ht_free(hashtable *table);

#endif // HASHTABLE_H