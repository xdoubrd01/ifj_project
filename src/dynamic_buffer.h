#ifndef DYNAMIC_BUFFER_H
#define DYNAMIC_BUFFER_H

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct {
  char *data;
  size_t length;
  size_t capacity;
} DynamicBuffer;

/* Init does not allocate memory. */
/* reserve() takes the content length, excluding the terminating zero. */
void dynamic_buffer_init(DynamicBuffer *buffer);
bool dynamic_buffer_reserve(DynamicBuffer *buffer, size_t length);
bool dynamic_buffer_append_char(DynamicBuffer *buffer, char c);
bool dynamic_buffer_append_string(DynamicBuffer *buffer, const char *text);
bool dynamic_buffer_append_vformat(DynamicBuffer *buffer, const char *format,
                                   va_list args);
void dynamic_buffer_clear(DynamicBuffer *buffer);
void dynamic_buffer_free(DynamicBuffer *buffer);

#endif
