#ifndef ARRAY_UTILS_H
#define ARRAY_UTILS_H

#include <stdbool.h>
#include <stddef.h>

bool array_next_capacity(size_t current, size_t required, size_t element_size,
                         size_t initial, size_t *result);

#endif // ARRAY_UTILS_H
