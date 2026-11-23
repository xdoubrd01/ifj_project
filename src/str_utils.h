#ifndef STR_UTILS_H
#define STR_UTILS_H

#include <stddef.h>

/* The caller owns the result and releases it with free(). */
char *string_duplicate(const char *text);

/* Copies exactly length bytes and appends a terminating zero. */
char *string_duplicate_n(const char *text, size_t length);

#endif