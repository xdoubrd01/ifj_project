/**
 * @par Projekt
 * Implementace překladače imperativního jazyka IFJ26
 * @file        code_buffer.h
 * @authors     Michal Zednicek <xzednim00>
 * @brief       Implementace vyrovnávací paměti výstupního kódu
 */

#ifndef CODE_BUFFER_H
#define CODE_BUFFER_H

#include <stdbool.h>
#include <stddef.h>

// Počáteční velikost bufferu
#define BUFFER_INIT_CAPACITY 512

typedef struct {
    char  *data;     // Ukazatel na samotný vygenerovaný kód
    size_t length;   // Aktuální délka kódu (bez koncového nulového znaku \0)
    size_t capacity; // Celková alokovaná kapacita bufferu
} CodeBuffer;

// Inicializuje buffer
bool buffer_init(CodeBuffer *buf);

// Přidá jeden znak do bufferu
bool buffer_append_char(CodeBuffer *buf, char c);

// Přidá řetězec do bufferu
bool buffer_append_string(CodeBuffer *buf, const char *str);

// Přidá formátovaný řetězec (podobně jako printf), velmi užitečné pro instrukce
bool buffer_append_format(CodeBuffer *buf, const char *format, ...);

// Uvolní paměť bufferu
void buffer_free(CodeBuffer *buf);

// Vymaže obsah bufferu bez uvolnění alokované paměti (pro opětovné použití)
void buffer_clear(CodeBuffer *buf);

// Vypíše obsah bufferu na stdout a hned ho vymaže
void buffer_flush(CodeBuffer *buf);

#endif // CODE_BUFFER_H