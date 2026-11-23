// Project:    IFJ26
// Filename:   code_label_counter.h
// Author:     Michal Zednicek <xzednim00>

// AI - generated from c file
#ifndef CODE_LABEL_COUNTER_H
#define CODE_LABEL_COUNTER_H

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

typedef struct {
    size_t cnt;        // Total number of generated labels
    size_t *stack;     // Dynamic array storing active nested label IDs
    size_t stack_top;  // Current number of items on the stack
    size_t capacity;   // Allocated capacity of the stack array
} LabelCounter;

/**
 * Generates a new unique label ID, pushes it onto the stack, and returns it.
 */
size_t label_push(LabelCounter *counter);

/**
 * Returns the label ID at the top of the stack without removing it.
 */
size_t label_top(LabelCounter *counter);

/**
 * Removes and returns the label ID at the top of the stack.
 */
size_t label_pop(LabelCounter *counter);

/**
 * Frees the internal stack array and resets the counter's stack state.
 */
void label_free(LabelCounter *counter);

#endif // CODE_LABEL_COUNTER_H