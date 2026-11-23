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