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
    STATE_STRING_ESCAPE, // Added auxiliary state for escape sequences
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

// Helper functions for dynamic buffer
static void dstring_init(dynamic_string *b) {
    b->text     = NULL;
    b->length   = 0;
    b->capacity = 0;
}

static void dstring_clear(dynamic_string *b) {
    b->length = 0;
    if (b->text != NULL) {
        b->text[0] = '\0';
    }
}

static void dstring_append(dynamic_string *b, char c) {
    (void)b;
    (void)c;
    // TODO: Dynamic memory reallocation and character appending
}

static void dstring_free(dynamic_string *b) {
    if (b->text != NULL) {
        free(b->text);
        b->text = NULL;
    }
    b->length   = 0;
    b->capacity = 0;
}

void scanner_init(FILE *source_file) {
    source = source_file;
    dstring_init(&buffer);
    // TODO: Initialize stack for tracking indentation levels
}

// Helper function to distinguish keywords and regular identifiers
static TokenType resolve_identifier_or_keyword(const char *text) {
    // Keyword check
    if (!strcmp(text, "def"))
        return TOKEN_KW_DEF;
    if (!strcmp(text, "do"))
        return TOKEN_KW_DO;
    if (!strcmp(text, "else"))
        return TOKEN_KW_ELSE;
    if (!strcmp(text, "if"))
        return TOKEN_KW_IF;
    if (!strcmp(text, "import"))
        return TOKEN_KW_IMPORT;
    if (!strcmp(text, "return"))
        return TOKEN_KW_RETURN;
    if (!strcmp(text, "then"))
        return TOKEN_KW_THEN;
    if (!strcmp(text, "val"))
        return TOKEN_KW_VAL;
    if (!strcmp(text, "var"))
        return TOKEN_KW_VAR;
    if (!strcmp(text, "while"))
        return TOKEN_KW_WHILE;

    // Data types check
    if (!strcmp(text, "Int"))
        return TOKEN_TYPE_INT;
    if (!strcmp(text, "Double"))
        return TOKEN_TYPE_DOUBLE;
    if (!strcmp(text, "String"))
        return TOKEN_TYPE_STRING;
    if (!strcmp(text, "Unit"))
        return TOKEN_TYPE_UNIT;

    // If no match occurred, it is a regular identifier
    return TOKEN_IDENTIFIER;
}

int get_next_token(Token *token) {
    if (!source || !token) {
        return 1; // Internal error
    }

    int  state            = STATE_START;
    int  comment_depth    = 0;
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

            // String literals
            if (c == '"') {
                state = STATE_STRING;
                break;
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
                ungetc(c, source);          // Return unused character
                token->type = TOKEN_ASSIGN; // Just '='
            }
            return 0;

        case STATE_SLASH:
            // Previous character was '/'
            if (c == '/') {
                state = STATE_LINE_COMMENT; // Line comment '//'
                break;
            } else if (c == '*') {
                state            = STATE_BLOCK_COMMENT; // Block comment '/*'
                comment_depth    = 1;
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
            // While reading letters, numbers, or underscore, append to buffer
            if (isalnum(c) || c == '_') {
                dstring_append(&buffer, (char)c);
            } else {
                // Character no longer belongs to identifier, return it to input
                ungetc(c, source);

                // Determine if it is a keyword or an identifier
                token->type = resolve_identifier_or_keyword(buffer.text);

                // If it is a regular identifier, we must copy the text (dynamic allocation)
                if (token->type == TOKEN_IDENTIFIER) {
                    token->attribute.s_val = (char *)malloc(buffer.length + 1);
                    if (token->attribute.s_val != NULL) {
                        strcpy(token->attribute.s_val, buffer.text);
                    } else {
                        return 1; // Internal memory allocation error
                    }
                }
                return 0; // Token is successfully prepared
            }
            break;

        case STATE_NUMBER:
            // TODO: Transitions for integers, dot, and exponents (see previous design)
            break;

        case STATE_STRING:
            if (c == '"') {
                // String is successfully terminated, copy it to the token
                token->type            = TOKEN_STRING_LITERAL;
                token->attribute.s_val = (char *)malloc(buffer.length + 1);
                if (token->attribute.s_val != NULL) {
                    strcpy(token->attribute.s_val, buffer.text);
                    return 0; // Success
                } else {
                    return 1; // Internal allocation error
                }
            } else if (c == '\\') {
                // Start of an escape sequence, switch to auxiliary state
                state = STATE_STRING_ESCAPE;
            } else if (c == '\n' || c == EOF) {
                // Unterminated string is lexical error 1 according to specification
                return 1;
            } else {
                // Regular character inside the string
                dstring_append(&buffer, (char)c);
            }
            break;

        case STATE_STRING_ESCAPE:
            // Evaluate the character immediately following the backslash
            if (c == 'n') {
                dstring_append(&buffer, '\n');
            } else if (c == 'r') {
                dstring_append(&buffer, '\r');
            } else if (c == 't') {
                dstring_append(&buffer, '\t');
            } else if (c == '\\') {
                dstring_append(&buffer, '\\');
            } else if (c == '"') {
                dstring_append(&buffer, '"');
            } else {
                // Unknown escape sequence leads to lexical error 1
                return 1;
            }
            // After successful processing of escape sequence, return to string reading
            state = STATE_STRING;
            break;

        default:
            return 1; // Internal error (unknown DFA state)
        }
    }
}

void scanner_free(void) {
    dstring_free(&buffer);
    // TODO: Deallocate memory for indentation stack
}