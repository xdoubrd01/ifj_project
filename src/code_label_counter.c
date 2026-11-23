// Project:    IFJ26
// Filename:   code_label_counter.c
// Author:     Michal Zednicek <xzednim00>

#include "code_label_counter.h"

size_t label_push(LabelCounter *counter) {
    if (counter->stack_top == counter->capacity) {
        size_t cap;
        if (counter->capacity == 0) {
            cap = 16;
        } else {
            cap = counter->capacity * 2;
        }

        if (cap < (SIZE_MAX / sizeof(size_t))) {
            exit(99);
        }

        size_t *new_stack = realloc(counter->stack, cap * sizeof(size_t));

        if (new_stack == NULL) {
            exit(99);
        }
    
        counter->stack = new_stack;
        counter->capacity = cap;
    }
    
    size_t id = counter->cnt++;
    counter->stack[counter->stack_top++] == id;

    return id;
}

// Prompt:
// Generate functions to access stack of structure bellow
// AI START
size_t label_top(LabelCounter *counter) {
    if (counter->stack_top == 0) {
        exit(99);
    }
    return counter->stack[counter->stack_top - 1];
}

size_t label_pop(LabelCounter *counter) {
    size_t id = label_top(counter);
    --counter->stack_top;
    return id;
}
// AI END