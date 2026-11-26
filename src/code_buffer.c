/**
 * @par         Implementace překladače imperativního jazyka IFJ26
 * @file        code_buffer.c
 * @authors     Michal Zednicek <xzednim00>
 * @brief       Implementace vyrovnávací paměti výstupného kódu
 */

// Prompt:
// napis kod pre bufferovanie generovaneho kodu pomocou prekladaca.
// ide o jeden buffer a pomocne funkcie pre pracu s nim.
// AI START
#include "code_buffer.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool buffer_init(CodeBuffer *buf) {
    if (buf == NULL)
        return false;

    buf->data = (char *)malloc(BUFFER_INIT_CAPACITY * sizeof(char));
    if (buf->data == NULL) {
        return false; // Chyba alokace
    }

    buf->data[0]  = '\0';
    buf->length   = 0;
    buf->capacity = BUFFER_INIT_CAPACITY;
    return true;
}

// Pomocná funkce pro zvětšení bufferu, pokud je to potřeba
static bool buffer_expand(CodeBuffer *buf, size_t required_length) {
    if (required_length < buf->capacity) {
        return true; // Kapacita je dostatečná
    }

    // Zdvojnásobujeme kapacitu, dokud není dostatečně velká
    size_t new_capacity = buf->capacity * 2;
    while (new_capacity <= required_length) {
        new_capacity *= 2;
    }

    char *new_data = (char *)realloc(buf->data, new_capacity * sizeof(char));
    if (new_data == NULL) {
        return false; // Chyba realokace
    }

    buf->data     = new_data;
    buf->capacity = new_capacity;
    return true;
}

bool buffer_append_char(CodeBuffer *buf, char c) {
    if (buf == NULL)
        return false;

    if (!buffer_expand(buf, buf->length + 1)) {
        return false;
    }

    buf->data[buf->length] = c;
    buf->length++;
    buf->data[buf->length] = '\0';

    return true;
}

bool buffer_append_string(CodeBuffer *buf, const char *str) {
    if (buf == NULL || str == NULL)
        return false;

    size_t str_len = strlen(str);
    if (!buffer_expand(buf, buf->length + str_len)) {
        return false;
    }

    // Zkopírujeme řetězec na konec bufferu
    memcpy(buf->data + buf->length, str, str_len + 1); // +1 pre '\0'
    buf->length += str_len;

    return true;
}

bool buffer_append_format(CodeBuffer *buf, const char *format, ...) {
    if (buf == NULL || format == NULL)
        return false;

    va_list args;
    va_start(args, format);

    // Zjistíme potřebnou délku pro formátovaný řetězec
    va_list args_copy;
    va_copy(args_copy, args);
    int required_len = vsnprintf(NULL, 0, format, args_copy);
    va_end(args_copy);

    if (required_len < 0) {
        va_end(args);
        return false; // Chyba formátování
    }

    if (!buffer_expand(buf, buf->length + required_len)) {
        va_end(args);
        return false;
    }

    // Zapíšeme formátovaný řetězec do bufferu
    vsnprintf(buf->data + buf->length, required_len + 1, format, args);
    buf->length += required_len;

    va_end(args);
    return true;
}

void buffer_free(CodeBuffer *buf) {
    if (buf != NULL) {
        free(buf->data);
        buf->data     = NULL;
        buf->length   = 0;
        buf->capacity = 0;
    }
}

void buffer_clear(CodeBuffer *buf) {
    if (buf != NULL && buf->data != NULL) {
        buf->length  = 0;
        buf->data[0] = '\0';
    }
}

void buffer_flush(CodeBuffer *buf) {
    if (buf != NULL && buf->data != NULL && buf->length > 0) {
        fwrite(buf->data, sizeof(char), buf->length, stdout);
        buffer_clear(buf);
    }
}
// AI END