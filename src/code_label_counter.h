#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

typedef struct {
    size_t cnt;
    size_t *stack;
    size_t stack_top;
    size_t capacity;
} LabelCounter;

