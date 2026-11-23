#include "array_utils.h"
#include <stdint.h>

bool array_next_capacity(size_t current, size_t required, size_t element_size,
                         size_t initial, size_t *result) {
  if (result == NULL || element_size == 0) {
    return false;
  }

  size_t limit = SIZE_MAX / element_size;
  if (required > limit || current > limit) {
    return false;
  }

  if (required <= current) {
    *result = current;
    return true;
  }

  size_t capacity = current;
  if (capacity == 0) {
    capacity = initial == 0 ? 1 : initial;
    if (capacity > limit) {
      capacity = required;
    }
  }

  while (capacity < required) {
    if (capacity > limit / 2) {
      capacity = required;
      break;
    }
    capacity *= 2;
  }

  *result = capacity;
  return true;
}