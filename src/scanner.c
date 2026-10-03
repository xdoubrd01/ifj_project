#include "scanner.h"
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Maximum allowed nesting depth for block indentation */
#define MAX_INDENT 100

/*
 * Definition of all internal states for the Deterministic Finite Automaton (DFA).
 * The scanner uses these states to process individual characters character-by-character
 * and assemble meaningful lexical tokens based on the IFJ26 grammar specification.
 */
typedef enum {
    STATE_INDENTATION, // Evaluates leading spaces at the beginning of a new line
    STATE_START,       // Default resting state waiting for a new token
    STATE_IDENTIFIER,  // Processes alphanumeric variable/function names or keywords
    STATE_NUMBER,      // Processes integers starting with digits 1-9
    STATE_NUMBER_ZERO, // Processes numbers starting with a '0' (to prevent invalid octal formats)
    STATE_NUMBER_FRACTION,  // Processes digits immediately following a decimal point
    STATE_NUMBER_EXP,       // Processes the exponent identifier ('e' or 'E')
    STATE_NUMBER_EXP_SIGN,  // Processes an optional '+' or '-' sign in the exponent
    STATE_NUMBER_EXP_DIGIT, // Processes the mandatory digits of the exponent
    STATE_NUMBER_SUFFIX,    // Processes the optional 'd' or 'D' suffix for Double literals
    STATE_STRING,           // Reads characters inside a string literal
    STATE_STRING_ESCAPE,    // Translates escape sequences inside strings (e.g., '\n', '\"')
    STATE_ASSIGN,           // Resolves ambiguity between assignment '=' and equality '=='
    STATE_SLASH,            // Resolves division '/' vs. line '//' vs. block '/*' comments
    STATE_LINE_COMMENT,     // Discards all characters until a newline character is found
    STATE_BLOCK_COMMENT     // Handles multi-line comments with support for deep nesting
} ScannerState;

/*
 * Dynamic buffer structure used to hold character sequences of arbitrary length.
 * Reallocates memory automatically as the lexeme grows.
 */
typedef struct {
    char  *text;
    size_t length;
    size_t capacity;
} dynamic_string;

static dynamic_string buffer;
static FILE          *source = NULL;

/*
 * Global state variables required to track the block scope (indentation)
 * across multiple independent calls to the get_next_token() function.
 */
static int  indent_stack[MAX_INDENT];
static int  top_index   = 0;
static int  dedent_debt = 0; // Number of DEDENT tokens owed to the parser
static bool at_line_start =
    true; // Flag indicating whether we are processing the first characters of a line

/* Initializes the dynamic string with null values */
static void dstring_init(dynamic_string *b) {
    b->text     = NULL;
    b->length   = 0;
    b->capacity = 0;
}

/* Clears the content of the buffer without deallocating its memory footprint */
static void dstring_clear(dynamic_string *b) {
    b->length = 0;
    if (b->text != NULL) {
        b->text[0] = '\0';
    }
}

/*
 * Appends a single character to the dynamic buffer.
 * If the buffer's capacity is reached, it dynamically doubles the allocated memory.
 * Returns 0 on success, or 1 on memory allocation failure (Internal Error 99).
 */
static int dstring_append(dynamic_string *b, char c) {
    if (b->length + 1 >= b->capacity) {
        size_t new_capacity = (b->capacity == 0) ? 16 : b->capacity * 2;
        char  *new_text     = (char *)realloc(b->text, new_capacity);
        if (new_text == NULL) {
            return 1; // Heap memory exhausted
        }
        b->text     = new_text;
        b->capacity = new_capacity;
    }
    b->text[b->length++] = c;
    b->text[b->length]   = '\0';
    return 0;
}

/* Safely deallocates the dynamic buffer memory to prevent memory leaks */
static void dstring_free(dynamic_string *b) {
    if (b->text != NULL) {
        free(b->text);
        b->text = NULL;
    }
    b->length   = 0;
    b->capacity = 0;
}

/*
 * Parses a buffered double literal string, omitting any underscore '_' characters
 * and the optional 'd' or 'D' suffix, before invoking strtod() for numerical conversion.
 */
static double convert_double_literal(const char *text) {
    char temp[256];
    int  j = 0;
    for (int i = 0; text[i] != '\0' && j < 255; i++) {
        if (text[i] != '_' && text[i] != 'd' && text[i] != 'D') {
            temp[j++] = text[i];
        }
    }
    temp[j] = '\0';
    return strtod(temp, NULL);
}

/*
 * Parses a buffered integer literal string, omitting any underscore '_' characters
 * before invoking strtol() to resolve the final base-10 numerical integer value.
 */
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

/* Initializes scanner context, global variables, and the indentation tracking stack */
void scanner_init(FILE *source_file) {
    source = source_file;
    dstring_init(&buffer);
    indent_stack[0] = 0; // Level 0 implies the outermost scope (0 spaces)
    top_index       = 0;
    dedent_debt     = 0;
    at_line_start   = true;
}

/*
 * Matches an extracted alphanumeric string against the language's reserved keywords.
 * If no match is found, the string is classified as a standard user-defined identifier.
 */
static TokenType resolve_identifier_or_keyword(const char *text) {
    // Control structures and definitions
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

    // Built-in basic data types
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

/*
 * Core scanner function. Retrieves the next valid lexical token from the input stream.
 * Returns 0 on success. Returns 1 for lexical syntax errors. Returns 99 for internal faults.
 */
int get_next_token(Token *token) {
    if (!source || !token) {
        return 1; // Missing context pointers
    }

    /*
     * Block Dedentation Queue Resolution:
     * If the previous line's indentation level dropped by more than one step,
     * the scanner owes the parser multiple DEDENT tokens. We yield one per function call
     * until the debt reaches zero before reading new characters.
     */
    if (dedent_debt > 0) {
        dedent_debt--;
        token->type = TOKEN_DEDENT;
        return 0;
    }

    int  state            = STATE_START;
    int  comment_depth    = 0;
    bool contains_newline = false;
    int  space_count      = 0;

    dstring_clear(&buffer);

    // Immediately trigger the indentation checking mechanism if we are on a fresh line.
    if (at_line_start) {
        state = STATE_INDENTATION;
    }

    while (true) {
        int c = fgetc(source);

        switch (state) {

        /*
         * STATE_INDENTATION:
         * Calculates the indentation level (number of spaces) of the current physical line.
         * Validates indentation changes against the global stack to emit INDENT/DEDENT tokens.
         */
        case STATE_INDENTATION:
            if (c == ' ') {
                space_count++;
            } else if (c == '\t') {
                return 1; // Tab characters are strictly forbidden at line start
            } else if (c == '\n') {
                space_count = 0; // Completely empty lines do not trigger scope changes
            } else if (c == EOF) {
                at_line_start = false;
                ungetc(c, source);

                // End of File encountered: gracefully close all currently open block scopes
                while (top_index > 0) {
                    dedent_debt++;
                    top_index--;
                }
                if (dedent_debt > 0) {
                    dedent_debt--;
                    token->type = TOKEN_DEDENT;
                    return 0;
                }
                state = STATE_START;
            } else {
                // A printable character was found; indentation measurement is complete.
                ungetc(c, source);
                at_line_start = false;

                if (space_count > indent_stack[top_index]) {
                    // Increased indentation implies a new nested block scope.
                    top_index++;
                    if (top_index >= MAX_INDENT)
                        return 99; // Indentation stack overflow
                    indent_stack[top_index] = space_count;
                    token->type             = TOKEN_INDENT;
                    return 0;
                } else if (space_count < indent_stack[top_index]) {
                    // Decreased indentation implies closing of one or more nested scopes.
                    while (top_index > 0 && space_count < indent_stack[top_index]) {
                        dedent_debt++;
                        top_index--;
                    }
                    // The new space count must match an existing previous block scope exactly.
                    if (space_count != indent_stack[top_index]) {
                        return 1; // Invalid indentation layout detected
                    }
                    dedent_debt--;
                    token->type = TOKEN_DEDENT;
                    return 0;
                } else {
                    // Indentation remains identical to the current block scope; resume normal
                    // parsing.
                    state = STATE_START;
                }
            }
            break;

        /*
         * STATE_START:
         * Serves as the primary entry point for characterizing the next immediate token based
         * on the type of character encountered.
         */
        case STATE_START:
            if (c == EOF) {
                token->type = TOKEN_EOF;
                return 0;
            }
            if (c == ' ') {
                continue; // Ignore generic whitespaces separating tokens within the line
            }
            if (c == '\n') {
                at_line_start = true; // Signals the scanner to check indentation on the next pass
                token->type   = TOKEN_EOL;
                return 0;
            }

            if (c == '"') {
                state = STATE_STRING;
                break;
            }

            if (isalpha(c) || c == '_') {
                if (dstring_append(&buffer, (char)c))
                    return 99;
                state = STATE_IDENTIFIER;
                break;
            }

            // Distinguish an isolated zero from a sequence starting with 1-9
            if (c == '0') {
                if (dstring_append(&buffer, (char)c))
                    return 99;
                state = STATE_NUMBER_ZERO;
                break;
            }
            if (isdigit(c)) {
                if (dstring_append(&buffer, (char)c))
                    return 99;
                state = STATE_NUMBER;
                break;
            }

            // Check for multi-character logical/mathematical operator prefixes
            if (c == '=') {
                state = STATE_ASSIGN;
                break;
            }
            if (c == '/') {
                state = STATE_SLASH;
                break;
            }

            // Execute single-character mathematical operators
            if (c == '+') {
                token->type = TOKEN_PLUS;
                return 0;
            }
            if (c == '-') {
                token->type = TOKEN_MINUS;
                return 0;
            }
            if (c == '*') {
                token->type = TOKEN_MUL;
                return 0;
            }

            // Execute comparative and relational operators
            if (c == '<') {
                int next_c = fgetc(source);
                if (next_c == '=') {
                    token->type = TOKEN_LEQ; // <=
                } else {
                    ungetc(next_c, source);
                    token->type = TOKEN_LT; // <
                }
                return 0;
            }
            if (c == '>') {
                int next_c = fgetc(source);
                if (next_c == '=') {
                    token->type = TOKEN_GEQ; // >=
                } else {
                    ungetc(next_c, source);
                    token->type = TOKEN_GT; // >
                }
                return 0;
            }
            if (c == '!') {
                int next_c = fgetc(source);
                if (next_c == '=') {
                    token->type = TOKEN_NEQ; // !=
                } else {
                    return 1; // Standalone exclamation marks are unsupported by default
                }
                return 0;
            }

            // Miscellaneous single-character delimiters
            if (c == '(') {
                token->type = TOKEN_LPAREN;
                return 0;
            }
            if (c == ')') {
                token->type = TOKEN_RPAREN;
                return 0;
            }
            if (c == ',') {
                token->type = TOKEN_COMMA;
                return 0;
            }
            if (c == ':') {
                token->type = TOKEN_COLON;
                return 0;
            }

            // Dot operator (e.g., for method or attribute access like x.toString)
            if (c == '.') {
                token->type = TOKEN_DOT;
                return 0;
            }

            // Semicolon (command separator on the same logical line)
            if (c == ';') {
                token->type = TOKEN_SEMICOLON;
                return 0;
            }

            // @main annotation evaluation
            if (c == '@') {
                const char *expected_main = "main";
                for (int i = 0; i < 4; i++) {
                    int next_c = fgetc(source);
                    if (next_c != expected_main[i]) {
                        return 1; // Lexical error: Only @main is allowed according to spec
                    }
                }
                token->type = TOKEN_AT_MAIN;
                return 0;
            }

            return 1; // Illegal character encountered

        /* STATE_ASSIGN: Disambiguates assignment '=' from logical equality '==' */
        case STATE_ASSIGN:
            if (c == '=') {
                token->type = TOKEN_EQ;
            } else {
                ungetc(c, source);
                token->type = TOKEN_ASSIGN;
            }
            return 0;

        /* STATE_SLASH: Resolves single division operators from line or block comments */
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

        /* STATE_LINE_COMMENT: Ignores text uniformly until line completion */
        case STATE_LINE_COMMENT:
            if (c == '\n') {
                at_line_start = true;
                token->type   = TOKEN_EOL;
                return 0;
            } else if (c == EOF) {
                token->type = TOKEN_EOF;
                return 0;
            }
            break;

        /* STATE_BLOCK_COMMENT: Manages deep nesting structures. Generates EOL if internally
         * wrapped. */
        case STATE_BLOCK_COMMENT:
            if (c == EOF) {
                return 1; // Unterminated block comment yields lexical error 1
            } else if (c == '\n') {
                contains_newline = true;
            } else if (c == '*') {
                int next_c = fgetc(source);
                if (next_c == '/') {
                    comment_depth--;
                    if (comment_depth == 0) {
                        if (contains_newline) {
                            at_line_start = true;
                            token->type   = TOKEN_EOL;
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
                    comment_depth++; // Track nested depth to avoid premature closure
                } else {
                    ungetc(next_c, source);
                }
            }
            break;

        /* STATE_IDENTIFIER: Collects characters into a string and determines function vs keyword */
        case STATE_IDENTIFIER:
            if (isalnum(c) || c == '_') {
                if (dstring_append(&buffer, (char)c))
                    return 99;
            } else {
                ungetc(c, source);
                token->type = resolve_identifier_or_keyword(buffer.text);

                // Deeply copy text for user-defined identifiers to prevent memory overwrites
                if (token->type == TOKEN_IDENTIFIER) {
                    token->attribute.s_val = (char *)malloc(buffer.length + 1);
                    if (token->attribute.s_val != NULL) {
                        strcpy(token->attribute.s_val, buffer.text);
                    } else {
                        return 99; // Internal allocation error
                    }
                }
                return 0;
            }
            break;

        /* STATE_NUMBER_ZERO: Implements the strict restriction preventing integers from starting
         * with zero */
        case STATE_NUMBER_ZERO:
            if (c == '.') {
                if (dstring_append(&buffer, (char)c))
                    return 99;
                state = STATE_NUMBER_FRACTION;
            } else if (c == 'e' || c == 'E') {
                if (dstring_append(&buffer, (char)c))
                    return 99;
                state = STATE_NUMBER_EXP;
            } else if (c == 'd' || c == 'D') {
                if (dstring_append(&buffer, (char)c))
                    return 99;
                state = STATE_NUMBER_SUFFIX;
            } else if (isdigit(c)) {
                return 1; // E.g., '012' leads to lexical error
            } else if (isalpha(c) || c == '_') {
                return 1; // Invalid character attached immediately after 0
            } else {
                ungetc(c, source);
                token->type            = TOKEN_INT_LITERAL;
                token->attribute.i_val = 0;
                return 0;
            }
            break;

        /* STATE_NUMBER: Processes standard non-zero-leading integers, capturing dots and exponent
         * states */
        case STATE_NUMBER:
            if (isdigit(c) || c == '_') {
                if (dstring_append(&buffer, (char)c))
                    return 99;
            } else if (c == '.') {
                if (dstring_append(&buffer, (char)c))
                    return 99;
                state = STATE_NUMBER_FRACTION;
            } else if (c == 'e' || c == 'E') {
                if (dstring_append(&buffer, (char)c))
                    return 99;
                state = STATE_NUMBER_EXP;
            } else if (c == 'd' || c == 'D') {
                if (dstring_append(&buffer, (char)c))
                    return 99;
                state = STATE_NUMBER_SUFFIX;
            } else if (isalpha(c)) {
                return 1; // Appended text without delimiter raises error
            } else {
                ungetc(c, source);
                token->type            = TOKEN_INT_LITERAL;
                token->attribute.i_val = convert_int_literal(buffer.text);
                return 0;
            }
            break;

        /* Decimal points redirect to double-literal handling logic */
        case STATE_NUMBER_FRACTION:
            if (isdigit(c) || c == '_') {
                if (dstring_append(&buffer, (char)c))
                    return 99;
            } else if (c == 'e' || c == 'E') {
                if (dstring_append(&buffer, (char)c))
                    return 99;
                state = STATE_NUMBER_EXP;
            } else if (c == 'd' || c == 'D') {
                if (dstring_append(&buffer, (char)c))
                    return 99;
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
                if (dstring_append(&buffer, (char)c))
                    return 99;
                state = STATE_NUMBER_EXP_SIGN;
            } else if (isdigit(c)) {
                if (dstring_append(&buffer, (char)c))
                    return 99;
                state = STATE_NUMBER_EXP_DIGIT;
            } else {
                return 1;
            }
            break;

        case STATE_NUMBER_EXP_SIGN:
            if (isdigit(c)) {
                if (dstring_append(&buffer, (char)c))
                    return 99;
                state = STATE_NUMBER_EXP_DIGIT;
            } else {
                return 1;
            }
            break;

        case STATE_NUMBER_EXP_DIGIT:
            if (isdigit(c) || c == '_') {
                if (dstring_append(&buffer, (char)c))
                    return 99;
            } else if (c == 'd' || c == 'D') {
                if (dstring_append(&buffer, (char)c))
                    return 99;
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

        /*
         * STATE_STRING:
         * Iterates strictly bounded characters until the terminating quote is detected.
         */
        case STATE_STRING:
            if (c == '"') {
                token->type            = TOKEN_STRING_LITERAL;
                token->attribute.s_val = (char *)malloc(buffer.length + 1);
                if (token->attribute.s_val != NULL) {
                    strcpy(token->attribute.s_val, buffer.text);
                    return 0; // The string literal is complete and valid
                } else {
                    return 99;
                }
            } else if (c == '\\') {
                // Backslash triggers a state jump to process the escape sequence natively
                state = STATE_STRING_ESCAPE;
            } else if (c == '\n' || c == EOF) {
                return 1; // Unclosed string traversing multiple lines generates a lexical error
            } else {
                if (dstring_append(&buffer, (char)c))
                    return 99;
            }
            break;

        /*
         * STATE_STRING_ESCAPE:
         * Parses and translates escaped sequence combinations within string literals.
         * Illegal escape combinations immediately invoke a fatal lexical error code.
         */
        case STATE_STRING_ESCAPE:
            if (c == 'n') {
                if (dstring_append(&buffer, '\n'))
                    return 99;
            } else if (c == 'r') {
                if (dstring_append(&buffer, '\r'))
                    return 99;
            } else if (c == 't') {
                if (dstring_append(&buffer, '\t'))
                    return 99;
            } else if (c == '\\') {
                if (dstring_append(&buffer, '\\'))
                    return 99;
            } else if (c == '"') {
                if (dstring_append(&buffer, '"'))
                    return 99;
            } else {
                return 1; // Encountered invalid escape sequence format
            }
            // Transition back to main string context
            state = STATE_STRING;
            break;

        default:
            return 99; // Unexpected finite state transition
        }
    }
}

/* Performs final memory deallocation during compiler termination */
void scanner_free(void) { dstring_free(&buffer); }