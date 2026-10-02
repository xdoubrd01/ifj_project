#include "scanner.h"
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Definition of Finite State Machine (DFA) states
typedef enum {
    STATE_START,
    STATE_IDENTIFIER,
    STATE_NUMBER,
    STATE_STRING,
    STATE_ASSIGN,
    STATE_SLASH,
    STATE_LINE_COMMENT,
    STATE_BLOCK_COMMENT
} ScannerState;

// Basic structure for a dynamic string (buffer for storing lexeme text)
typedef struct {
    char  *text;
    size_t length;
    size_t capacity;
} dynamic_string;

static dynamic_string buffer;
static FILE          *source = NULL;

// Helper functions for dynamic buffer (implementation to be added for strings/numbers)
void dstring_init(dynamic_string *b) {
    b->text = NULL;
    b->length = 0;
    b->capacity = 0;
}

void dstring_clear(dynamic_string *b) {
    b->length = 0;
    if (b->text != NULL) {
        b->text[0] = '\0';
    }
}

void dstring_append(dynamic_string *b, char c) {
    (void)b;
    (void)c;
    // TODO: Dynamic memory reallocation and character appending
}

void dstring_free(dynamic_string *b) {
    if (b->text != NULL) {
        free(b->text);
        b->text = NULL;
    }
    b->length = 0;
    b->capacity = 0;
}

void scanner_init(FILE *source_file) {
    source = source_file;
    dstring_init(&buffer);
    // TODO: Initialize stack for tracking indentation levels
}

int get_next_token(Token *token) {
    if (!source || !token) {
        return 1; // Internal error
    }

    int  state = STATE_START;
    int  comment_depth = 0;
    bool contains_newline = false;

    dstring_clear(&buffer);

    while (true) {
        int c = fgetc(source);

        switch (state) {
            case STATE_START:
                if (c == EOF) {
                    token->type = TOKEN_EOF;
                    return 0;
                }
                if (c == ' ') {
                    continue; // Skip regular spaces within a line
                }
                if (c == '\n') {
                    token->type = TOKEN_EOL;
                    return 0;
                }

                // Identifiers and keywords
                if (isalpha(c) || c == '_') {
                    dstring_append(&buffer, (char)c);
                    state = STATE_IDENTIFIER;
                    break;
                }

                // Integer and floating-point literals
                if (isdigit(c)) {
                    dstring_append(&buffer, (char)c);
                    state = STATE_NUMBER;
                    break;
                }

                // Compound operators and comments
                if (c == '=') {
                    state = STATE_ASSIGN;
                    break;
                }
                if (c == '/') {
                    state = STATE_SLASH;
                    break;
                }

                // Single-character operators
                if (c == '+') {
                    token->type = TOKEN_PLUS;
                    return 0;
                }
                if (c == '-') {
                    token->type = TOKEN_MINUS;
                    return 0;
                }

                return 1; // Unknown character -> lexical error 1 according to specification

            case STATE_ASSIGN:
                // Previous character was '='
                if (c == '=') {
                    token->type = TOKEN_EQ; // '=='
                } else {
                    ungetc(c, source);      // Return unused character
                    token->type = TOKEN_ASSIGN; // Just '='
                }
                return 0;

            case STATE_SLASH:
                // Previous character was '/'
                if (c == '/') {
                    state = STATE_LINE_COMMENT; // Line comment '//'
                    break;
                } else if (c == '*') {
                    state = STATE_BLOCK_COMMENT; // Block comment '/*'
                    comment_depth = 1;
                    contains_newline = false;
                    break;
                } else {
                    ungetc(c, source);
                    token->type = TOKEN_DIV; // Standard division '/'
                    return 0;
                }

            case STATE_LINE_COMMENT:
                // Line comment ends with newline (LF) or EOF
                if (c == '\n') {
                    token->type = TOKEN_EOL; // Comment acts as a newline
                    return 0;
                } else if (c == EOF) {
                    token->type = TOKEN_EOF;
                    return 0;
                }
                break; // Discard all other characters in the comment

            case STATE_BLOCK_COMMENT:
                if (c == EOF) {
                    return 1; // Unterminated comment -> lexical error 1
                } else if (c == '\n') {
                    contains_newline = true; // Record newline inside comment
                } else if (c == '*') {
                    int next_c = fgetc(source);
                    if (next_c == '/') {
                        comment_depth--;
                        if (comment_depth == 0) {
                            // Comment fully closed
                            if (contains_newline) {
                                token->type = TOKEN_EOL;
                                return 0;
                            } else {
                                state = STATE_START; // Acts as a single white space
                                break;
                            }
                        }
                    } else {
                        ungetc(next_c, source);
                    }
                } else if (c == '/') {
                    int next_c = fgetc(source);
                    if (next_c == '*') {
                        comment_depth++; // Nested comment '/*'
                    } else {
                        ungetc(next_c, source);
                    }
                }
                break;

            case STATE_IDENTIFIER:
                // TODO: Transitions for accumulating identifier and keyword check
                break;

            case STATE_NUMBER:
                // TODO: Transitions for integers, dot, and exponents
                break;

            case STATE_STRING:
                // TODO: Processing of string literals
                break;
        }
    }
}

void scanner_free(void) {
    dstring_free(&buffer);
    // TODO: Deallocate memory for indentation stack
}