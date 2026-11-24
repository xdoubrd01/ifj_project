#ifndef CODE_BUFFER_H
#define CODE_BUFFER_H

#include <stdbool.h>
#include <stddef.h>

// Počiatočná veľkosť buffera
#define BUFFER_INIT_CAPACITY 512

typedef struct {
    char  *data;     // Ukazovateľ na samotný vygenerovaný kód
    size_t length;   // Aktuálna dĺžka kódu (bez \0)
    size_t capacity; // Celková alokovaná kapacita buffera
} CodeBuffer;

// Inicializuje buffer
bool buffer_init(CodeBuffer *buf);

// Pridá jeden znak do buffera
bool buffer_append_char(CodeBuffer *buf, char c);

// Pridá reťazec do buffera
bool buffer_append_string(CodeBuffer *buf, const char *str);

// Pridá formátovaný reťazec (podobne ako printf), veľmi užitočné pre inštrukcie
bool buffer_append_format(CodeBuffer *buf, const char *format, ...);

// Uvoľní pamäť buffera
void buffer_free(CodeBuffer *buf);

// Vymaže obsah buffera bez uvoľnenia alokovanej pamäte (pre opätovné použitie)
void buffer_clear(CodeBuffer *buf);

// Vypíše obsah buffera na stdout a hneď ho vymaže
void buffer_flush(CodeBuffer *buf);

#endif // CODE_BUFFER_H