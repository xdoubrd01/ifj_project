#include "dynamic_buffer.h"
#include "array_utils.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void dynamic_buffer_init(DynamicBuffer *buffer) { *buffer = (DynamicBuffer){0}; }

bool dynamic_buffer_reserve(DynamicBuffer *buffer, size_t length) {
    if (buffer == NULL || length == SIZE_MAX) {
        return false;
    }

    if (length < buffer->capacity) {
        return true;
    }

    size_t capacity;
    if (!array_next_capacity(buffer->capacity, length + 1, sizeof(char), 32, &capacity)) {
        return false;
    }

    char *data = realloc(buffer->data, capacity);
    if (data == NULL) {
        return false;
    }

    buffer->data                 = data;
    buffer->capacity             = capacity;
    buffer->data[buffer->length] = '\0';
    return true;
}

bool dynamic_buffer_append_char(DynamicBuffer *buffer, char c) {
    if (buffer == NULL || buffer->length == SIZE_MAX ||
        !dynamic_buffer_reserve(buffer, buffer->length + 1)) {
        return false;
    }

    buffer->data[buffer->length++] = c;
    buffer->data[buffer->length]   = '\0';
    return true;
}

bool dynamic_buffer_append_string(DynamicBuffer *buffer, const char *text) {
    if (buffer == NULL || text == NULL) {
        return false;
    }

    size_t length = strlen(text);
    if (length > SIZE_MAX - buffer->length ||
        !dynamic_buffer_reserve(buffer, buffer->length + length)) {
        return false;
    }

    memcpy(buffer->data + buffer->length, text, length + 1);
    buffer->length += length;
    return true;
}

bool dynamic_buffer_append_vformat(DynamicBuffer *buffer, const char *format, va_list args) {
    if (buffer == NULL || format == NULL) {
        return false;
    }

    va_list copy;
    va_copy(copy, args);
    int length = vsnprintf(NULL, 0, format, copy);
    va_end(copy);

    if (length < 0 || (size_t)length > SIZE_MAX - buffer->length ||
        !dynamic_buffer_reserve(buffer, buffer->length + (size_t)length)) {
        return false;
    }

    va_copy(copy, args);
    int written = vsnprintf(buffer->data + buffer->length, (size_t)length + 1, format, copy);
    va_end(copy);

    if (written != length) {
        buffer->data[buffer->length] = '\0';
        return false;
    }

    buffer->length += (size_t)length;
    return true;
}

void dynamic_buffer_clear(DynamicBuffer *buffer) {
    if (buffer != NULL) {
        buffer->length = 0;
        if (buffer->data != NULL) {
            buffer->data[0] = '\0';
        }
    }
}

void dynamic_buffer_free(DynamicBuffer *buffer) {
    if (buffer != NULL) {
        free(buffer->data);
        *buffer = (DynamicBuffer){0};
    }
}