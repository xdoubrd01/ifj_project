#include "str_utils.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

char *string_duplicate_n(const char *text, size_t length) {
    if (text == NULL || length == SIZE_MAX) {
        return NULL;
    }

    char *copy = malloc(length + 1);
    if (copy != NULL) {
        memcpy(copy, text, length);
        copy[length] = '\0';
    }

    return copy;
}

char *string_duplicate(const char *text) {
    return text == NULL ? NULL : string_duplicate_n(text, strlen(text));
}