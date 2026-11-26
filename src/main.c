#include "parser.h"
#include "scanner.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

// AI ASSISTED START
// AI PROMPT: Nacti stdin do pameti bez fmemopen, se zachovanim presne delky.
static char *prepare_input(size_t *length_out) {
    size_t capacity = 4096;
    size_t length   = 0;
    char  *buffer   = malloc(capacity);
    if (buffer == NULL)
        return NULL;

    int c;
    while ((c = fgetc(stdin)) != EOF) {
        if (length == capacity) {
            if (capacity > SIZE_MAX / 2) {
                free(buffer);
                return NULL;
            }
            capacity *= 2;
            char *next = realloc(buffer, capacity);
            if (next == NULL) {
                free(buffer);
                return NULL;
            }
            buffer = next;
        }
        buffer[length++] = (char)c;
    }
    if (ferror(stdin)) {
        free(buffer);
        return NULL;
    }
    *length_out = length;
    return buffer;
}

int main(void) {
    size_t length = 0;
    char  *buffer = prepare_input(&length);
    if (buffer == NULL) {
        fprintf(stderr, "Internal error: cannot prepare input\n");
        return 99;
    }
    parse(buffer, length);
    free(buffer);
    return 0;
}
// AI ASSISTED END