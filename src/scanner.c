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
    STATE_NUMBER_ZERO,
    STATE_NUMBER_FRACTION,
    STATE_NUMBER_EXP,
    STATE_NUMBER_EXP_SIGN,
    STATE_NUMBER_EXP_DIGIT,
    STATE_NUMBER_SUFFIX,
    STATE_STRING,
    STATE_STRING_ESCAPE,
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

// Helper function to convert a number with underscores to double
static double convert_double_literal(const char *text) {
    char temp[256];
    int  j = 0;
    for (int i = 0; text[i] != '\0' && j < 255; i++) {
        // Skip underscores and optional d/D suffix
        if (text[i] != '_' && text[i] != 'd' && text[i] != 'D') {
            temp[j++] = text[i];
        }
    }
    temp[j] = '\0';
    return strtod(temp, NULL);
}

// Helper function to convert a number with underscores to int
static int convert_int_literal(const char *text) {
    char temp[256];
    int  j = 0;
    for (int i = 0; text[i] != '\0' && j < 255; i++) {
        if (text[i] != '_') {
            temp[j++] = text[i];
        }
    }
    temp[j] = '\0';
    return (int)strtol(temp, NULL, 10);
}

void scanner_init(FILE *source_file) {
    source = source_file;
    dstring_init(&buffer);
    // TODO: Initialize stack for tracking indentation levels
}

// Helper function to distinguish keywords and regular identifiers
static TokenType resolve_identifier_or_keyword(const char *text) {
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

    if (!strcmp(text, "Int"))
        return TOKEN_TYPE_INT;
    if (!strcmp(text, "Double"))
        return TOKEN_TYPE_DOUBLE;
    if (!strcmp(text, "String"))
        return TOKEN_TYPE_STRING;
    if (!strcmp(text, "Unit"))
        return TOKEN_TYPE_UNIT;

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
                continue;
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
            if (c == '0') {
                dstring_append(&buffer, (char)c);
                state = STATE_NUMBER_ZERO;
                break;
            }
            if (isdigit(c)) { // 1-9
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

            return 1;

        case STATE_ASSIGN:
            if (c == '=') {
                token->type = TOKEN_EQ;
            } else {
                ungetc(c, source);
                token->type = TOKEN_ASSIGN;
            }
            return 0;

        case STATE_SLASH:
            if (c == '/') {
                state = STATE_LINE_COMMENT;
                break;
            } else if (c == '*') {
                state            = STATE_BLOCK_COMMENT;
                comment_depth    = 1;
                contains_newline = false;
                break;
            } else {
                ungetc(c, source);
                token->type = TOKEN_DIV;
                return 0;
            }

        case STATE_LINE_COMMENT:
            if (c == '\n') {
                token->type = TOKEN_EOL;
                return 0;
            } else if (c == EOF) {
                token->type = TOKEN_EOF;
                return 0;
            }
            break;

        case STATE_BLOCK_COMMENT:
            if (c == EOF) {
                return 1;
            } else if (c == '\n') {
                contains_newline = true;
            } else if (c == '*') {
                int next_c = fgetc(source);
                if (next_c == '/') {
                    comment_depth--;
                    if (comment_depth == 0) {
                        if (contains_newline) {
                            token->type = TOKEN_EOL;
                            return 0;
                        } else {
                            state = STATE_START;
                            break;
                        }
                    }
                } else {
                    ungetc(next_c, source);
                }
            } else if (c == '/') {
                int next_c = fgetc(source);
                if (next_c == '*') {
                    comment_depth++;
                } else {
                    ungetc(next_c, source);
                }
            }
            break;

        case STATE_IDENTIFIER:
            if (isalnum(c) || c == '_') {
                dstring_append(&buffer, (char)c);
            } else {
                ungetc(c, source);

                token->type = resolve_identifier_or_keyword(buffer.text);

                if (token->type == TOKEN_IDENTIFIER) {
                    token->attribute.s_val = (char *)malloc(buffer.length + 1);
                    if (token->attribute.s_val != NULL) {
                        strcpy(token->attribute.s_val, buffer.text);
                    } else {
                        return 1;
                    }
                }
                return 0;
            }
            break;

        // --- NUMBER STATES ---
        case STATE_NUMBER_ZERO:
            if (c == '.') {
                dstring_append(&buffer, (char)c);
                state = STATE_NUMBER_FRACTION;
            } else if (c == 'e' || c == 'E') {
                dstring_append(&buffer, (char)c);
                state = STATE_NUMBER_EXP;
            } else if (c == 'd' || c == 'D') {
                dstring_append(&buffer, (char)c);
                state = STATE_NUMBER_SUFFIX;
            } else if (isdigit(c)) {
                return 1; // Error: Non-zero number cannot start with zero
            } else if (isalpha(c) || c == '_') {
                return 1; // Invalid character attached to a number
            } else {
                ungetc(c, source);
                token->type            = TOKEN_INT_LITERAL;
                token->attribute.i_val = 0;
                return 0;
            }
            break;

        case STATE_NUMBER:
            if (isdigit(c) || c == '_') {
                dstring_append(&buffer, (char)c);
            } else if (c == '.') {
                dstring_append(&buffer, (char)c);
                state = STATE_NUMBER_FRACTION;
            } else if (c == 'e' || c == 'E') {
                dstring_append(&buffer, (char)c);
                state = STATE_NUMBER_EXP;
            } else if (c == 'd' || c == 'D') {
                dstring_append(&buffer, (char)c);
                state = STATE_NUMBER_SUFFIX;
            } else if (isalpha(c)) {
                return 1;
            } else {
                ungetc(c, source);
                token->type            = TOKEN_INT_LITERAL;
                token->attribute.i_val = convert_int_literal(buffer.text);
                return 0;
            }
            break;

        case STATE_NUMBER_FRACTION:
            if (isdigit(c) || c == '_') {
                dstring_append(&buffer, (char)c);
            } else if (c == 'e' || c == 'E') {
                dstring_append(&buffer, (char)c);
                state = STATE_NUMBER_EXP;
            } else if (c == 'd' || c == 'D') {
                dstring_append(&buffer, (char)c);
                state = STATE_NUMBER_SUFFIX;
            } else if (isalpha(c)) {
                return 1;
            } else {
                ungetc(c, source);
                token->type            = TOKEN_DOUBLE_LITERAL;
                token->attribute.d_val = convert_double_literal(buffer.text);
                return 0;
            }
            break;

        case STATE_NUMBER_EXP:
            if (c == '+' || c == '-') {
                dstring_append(&buffer, (char)c);
                state = STATE_NUMBER_EXP_SIGN;
            } else if (isdigit(c)) {
                dstring_append(&buffer, (char)c);
                state = STATE_NUMBER_EXP_DIGIT;
            } else {
                return 1;
            }
            break;

        case STATE_NUMBER_EXP_SIGN:
            if (isdigit(c)) {
                dstring_append(&buffer, (char)c);
                state = STATE_NUMBER_EXP_DIGIT;
            } else {
                return 1;
            }
            break;

        case STATE_NUMBER_EXP_DIGIT:
            if (isdigit(c) || c == '_') {
                dstring_append(&buffer, (char)c);
            } else if (c == 'd' || c == 'D') {
                dstring_append(&buffer, (char)c);
                state = STATE_NUMBER_SUFFIX;
            } else if (isalpha(c)) {
                return 1;
            } else {
                ungetc(c, source);
                token->type            = TOKEN_DOUBLE_LITERAL;
                token->attribute.d_val = convert_double_literal(buffer.text);
                return 0;
            }
            break;

        case STATE_NUMBER_SUFFIX:
            if (isalpha(c) || isdigit(c) || c == '_') {
                return 1;
            } else {
                ungetc(c, source);
                token->type            = TOKEN_DOUBLE_LITERAL;
                token->attribute.d_val = convert_double_literal(buffer.text);
                return 0;
            }

        // --- STRING STATES ---
        case STATE_STRING:
            if (c == '"') {
                token->type            = TOKEN_STRING_LITERAL;
                token->attribute.s_val = (char *)malloc(buffer.length + 1);
                if (token->attribute.s_val != NULL) {
                    strcpy(token->attribute.s_val, buffer.text);
                    return 0;
                } else {
                    return 1;
                }
            } else if (c == '\\') {
                state = STATE_STRING_ESCAPE;
            } else if (c == '\n' || c == EOF) {
                return 1;
            } else {
                dstring_append(&buffer, (char)c);
            }
            break;

        case STATE_STRING_ESCAPE:
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
                return 1;
            }
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