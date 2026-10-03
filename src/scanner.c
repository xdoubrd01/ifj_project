#include "scanner.h"
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_INDENT 100

typedef struct {
    char  *text;
    size_t length;
    size_t capacity;
} DynamicString;

static FILE         *source = NULL;
static DynamicString buffer;

static int  indent_stack[MAX_INDENT];
static int  indent_top          = 0;
static int  pending_dedents     = 0;
static bool at_line_start       = true;
static bool line_has_token      = false;
static int  paren_depth         = 0;
static int  logical_line_indent = 0;
static bool continuation_line   = false;

static void ds_init(DynamicString *s) {
    s->text     = NULL;
    s->length   = 0;
    s->capacity = 0;
}

static void ds_clear(DynamicString *s) {
    s->length = 0;
    if (s->text != NULL) {
        s->text[0] = '\0';
    }
}

static int ds_append(DynamicString *s, char c) {
    if (s->length + 1 >= s->capacity) {
        size_t new_capacity = s->capacity == 0 ? 32 : s->capacity * 2;
        char  *new_text     = (char *)realloc(s->text, new_capacity);
        if (new_text == NULL) {
            return 99;
        }
        s->text     = new_text;
        s->capacity = new_capacity;
    }

    s->text[s->length++] = c;
    s->text[s->length]   = '\0';
    return 0;
}

static void ds_free(DynamicString *s) {
    free(s->text);
    s->text     = NULL;
    s->length   = 0;
    s->capacity = 0;
}

static int set_string_attribute(Token *token) {
    const char *src = buffer.text != NULL ? buffer.text : "";

    token->attribute.s_val = (char *)malloc(buffer.length + 1);
    if (token->attribute.s_val == NULL) {
        return 99;
    }
    memcpy(token->attribute.s_val, src, buffer.length + 1);
    return 0;
}

static int push_char(int c) {
    if (c == EOF) {
        return 0;
    }
    return ungetc(c, source) == EOF ? 99 : 0;
}

static TokenType keyword_or_identifier(const char *text) {
    if (strcmp(text, "def") == 0)
        return TOKEN_KW_DEF;
    if (strcmp(text, "do") == 0)
        return TOKEN_KW_DO;
    if (strcmp(text, "else") == 0)
        return TOKEN_KW_ELSE;
    if (strcmp(text, "if") == 0)
        return TOKEN_KW_IF;
    if (strcmp(text, "import") == 0)
        return TOKEN_KW_IMPORT;
    if (strcmp(text, "return") == 0)
        return TOKEN_KW_RETURN;
    if (strcmp(text, "then") == 0)
        return TOKEN_KW_THEN;
    if (strcmp(text, "val") == 0)
        return TOKEN_KW_VAL;
    if (strcmp(text, "var") == 0)
        return TOKEN_KW_VAR;
    if (strcmp(text, "while") == 0)
        return TOKEN_KW_WHILE;
    if (strcmp(text, "Int") == 0)
        return TOKEN_TYPE_INT;
    if (strcmp(text, "Double") == 0)
        return TOKEN_TYPE_DOUBLE;
    if (strcmp(text, "String") == 0)
        return TOKEN_TYPE_STRING;
    if (strcmp(text, "Unit") == 0)
        return TOKEN_TYPE_UNIT;
    return TOKEN_IDENTIFIER;
}

static int convert_int(const char *text, int *out) {
    DynamicString tmp;
    ds_init(&tmp);

    for (size_t i = 0; text[i] != '\0'; ++i) {
        if (text[i] != '_') {
            int rc = ds_append(&tmp, text[i]);
            if (rc != 0) {
                ds_free(&tmp);
                return rc;
            }
        }
    }

    *out = (int)strtol(tmp.text != NULL ? tmp.text : "0", NULL, 10);
    ds_free(&tmp);
    return 0;
}

static int convert_double(const char *text, double *out) {
    DynamicString tmp;
    ds_init(&tmp);

    for (size_t i = 0; text[i] != '\0'; ++i) {
        char c = text[i];
        if (c == '_') {
            continue;
        }
        if ((c == 'd' || c == 'D') && text[i + 1] == '\0') {
            continue;
        }
        int rc = ds_append(&tmp, c);
        if (rc != 0) {
            ds_free(&tmp);
            return rc;
        }
    }

    errno        = 0;
    char  *end   = NULL;
    double value = strtod(tmp.text != NULL ? tmp.text : "0", &end);

    if (errno == ERANGE || !isfinite(value) || end == NULL || *end != '\0') {
        ds_free(&tmp);
        return 1;
    }

    *out = value;
    ds_free(&tmp);
    return 0;
}

/* Consumes a nested block comment after the opening slash-star was already
 * read. */
static int consume_block_comment(bool *had_newline) {
    int depth    = 1;
    *had_newline = false;

    while (depth > 0) {
        int c = fgetc(source);
        if (c == EOF) {
            return 1;
        }

        if (c == '\n') {
            *had_newline = true;
            continue;
        }

        if (c == '/') {
            int next = fgetc(source);
            if (next == '*') {
                ++depth;
            } else {
                int rc = push_char(next);
                if (rc != 0)
                    return rc;
            }
        } else if (c == '*') {
            int next = fgetc(source);
            if (next == '/') {
                --depth;
            } else {
                int rc = push_char(next);
                if (rc != 0)
                    return rc;
            }
        }
    }

    return 0;
}

/*
 * A multiline block comment behaves as one EOL. After its closing delimiter
 * only whitespace may occur before the physical LF/EOF.
 */
static int finish_multiline_comment(Token *token) {
    int c;
    do {
        c = fgetc(source);
    } while (c == ' ' || c == '\t' || c == '\r' || c == '\f' || c == '\v');

    if (c != '\n' && c != EOF) {
        return 1;
    }

    at_line_start     = true;
    continuation_line = false;

    if (paren_depth > 0) {
        line_has_token = false;
        return -1; /* continue scanning; LF inside parentheses is whitespace */
    }

    if (!line_has_token) {
        line_has_token = false;
        return -1; /* comment-only physical/logical line is ignored */
    }

    line_has_token = false;
    token->type    = TOKEN_EOL;
    return 0;
}

/*
 * Handles beginning-of-line whitespace and indentation. Returns:
 *   0  token has been produced,
 *  -1  no token produced; caller should continue scanning,
 *   1/99 error.
 */
static int handle_line_start(Token *token) {
    int  spaces             = 0;
    bool indentation_locked = false;

    while (at_line_start) {
        int c;

        if (!indentation_locked) {
            while ((c = fgetc(source)) == ' ') {
                ++spaces;
            }

            if (c == '\t') {
                return 1;
            }
        } else {

            do {
                c = fgetc(source);
            } while (c == ' ' || c == '\t' || c == '\f' || c == '\v');
        }

        if (c == '\r') {
            int next = fgetc(source);
            if (next != '\n') {
                return 1;
            }
            c = '\n';
        }

        if (c == '\n') {
            /* Empty/whitespace/comment-only physical lines are ignored. */
            spaces             = 0;
            indentation_locked = false;
            line_has_token     = false;
            continuation_line  = paren_depth > 0;
            continue;
        }

        if (c == EOF) {
            at_line_start     = false;
            continuation_line = false;
            if (indent_top > 0) {
                pending_dedents = indent_top;
                indent_top      = 0;
                --pending_dedents;
                token->type = TOKEN_DEDENT;
                return 0;
            }
            token->type = TOKEN_EOF;
            return 0;
        }

        /* Comment-only lines must not change indentation. */
        if (c == '/') {
            int next = fgetc(source);
            if (next == '/') {
                while ((c = fgetc(source)) != '\n' && c != EOF) {
                    ;
                }
                if (c == EOF) {
                    at_line_start = false;
                    if (indent_top > 0) {
                        pending_dedents = indent_top;
                        indent_top      = 0;
                        --pending_dedents;
                        token->type = TOKEN_DEDENT;
                        return 0;
                    }
                    token->type = TOKEN_EOF;
                    return 0;
                }
                spaces             = 0;
                indentation_locked = false;
                line_has_token     = false;
                continue;
            }

            if (next == '*') {
                bool had_newline = false;
                int  rc          = consume_block_comment(&had_newline);
                if (rc != 0)
                    return rc;

                if (had_newline) {
                    rc = finish_multiline_comment(token);
                    if (rc >= 0)
                        return rc;
                    spaces             = 0;
                    indentation_locked = false;
                    continue;
                }

                /*
                 * A same-line block comment at BOL behaves like whitespace.
                 * Preserve the indentation measured before the comment.
                 */
                indentation_locked = true;
                continue;
            }

            if (push_char(next) != 0 || push_char(c) != 0) {
                return 99;
            }
        } else {
            if (push_char(c) != 0) {
                return 99;
            }
        }

        if (paren_depth > 0) {
            /* Continuation lines inside parentheses do not create INDENT/DEDENT. */
            if (continuation_line && spaces <= logical_line_indent) {
                return 1;
            }
            at_line_start     = false;
            continuation_line = false;
            return -1;
        }

        at_line_start       = false;
        continuation_line   = false;
        logical_line_indent = spaces;

        if (spaces > indent_stack[indent_top]) {
            if (indent_top + 1 >= MAX_INDENT) {
                return 99;
            }
            indent_stack[++indent_top] = spaces;
            token->type                = TOKEN_INDENT;
            return 0;
        }

        if (spaces < indent_stack[indent_top]) {
            int count = 0;
            while (indent_top > 0 && spaces < indent_stack[indent_top]) {
                --indent_top;
                ++count;
            }
            if (spaces != indent_stack[indent_top]) {
                return 1;
            }
            pending_dedents = count - 1;
            token->type     = TOKEN_DEDENT;
            return 0;
        }

        return -1;
    }

    return -1;
}

static int read_identifier(Token *token, int first) {
    ds_clear(&buffer);
    if (ds_append(&buffer, (char)first) != 0)
        return 99;

    int c;
    while ((c = fgetc(source)) != EOF && (isalnum((unsigned char)c) || c == '_')) {
        if (ds_append(&buffer, (char)c) != 0)
            return 99;
    }
    if (push_char(c) != 0)
        return 99;

    if (buffer.length == 1 && buffer.text[0] == '_') {
        return 1;
    }

    token->type = keyword_or_identifier(buffer.text);
    if (token->type == TOKEN_IDENTIFIER) {
        return set_string_attribute(token);
    }
    return 0;
}

static int append_digit_sequence(int first_digit, int *terminator) {
    bool previous_was_digit = false;

    if (first_digit >= 0) {
        if (!isdigit((unsigned char)first_digit))
            return 1;
        if (ds_append(&buffer, (char)first_digit) != 0)
            return 99;
        previous_was_digit = true;
    }

    while (true) {
        int c = fgetc(source);
        if (isdigit((unsigned char)c)) {
            if (ds_append(&buffer, (char)c) != 0)
                return 99;
            previous_was_digit = true;
            continue;
        }

        if (c == '_') {
            if (!previous_was_digit)
                return 1;
            int next = fgetc(source);
            if (!isdigit((unsigned char)next))
                return 1;
            if (ds_append(&buffer, '_') != 0 || ds_append(&buffer, (char)next) != 0)
                return 99;
            previous_was_digit = true;
            continue;
        }

        *terminator = c;
        return 0;
    }
}

static int finish_number_delimiter(int c) {
    if (isalpha((unsigned char)c) || c == '_') {
        return 1;
    }
    return push_char(c);
}

static int read_number(Token *token, int first) {
    ds_clear(&buffer);

    bool has_fraction = false;
    bool has_exponent = false;
    bool has_suffix   = false;
    int  term         = EOF;

    if (first == '.') {
        has_fraction = true;
        if (ds_append(&buffer, '.') != 0)
            return 99;
        int c = fgetc(source);
        if (!isdigit((unsigned char)c)) {
            return 1;
        }
        int rc = append_digit_sequence(c, &term);
        if (rc != 0)
            return rc;
    } else {
        if (first == '0') {
            if (ds_append(&buffer, '0') != 0)
                return 99;
            term = fgetc(source);
            if (isdigit((unsigned char)term) || term == '_') {
                return 1;
            }
        } else {
            int rc = append_digit_sequence(first, &term);
            if (rc != 0)
                return rc;
        }

        if (term == '.') {
            has_fraction = true;
            if (ds_append(&buffer, '.') != 0)
                return 99;
            int c = fgetc(source);
            if (!isdigit((unsigned char)c)) {
                return 1; /* 1. is invalid; fractional part is mandatory */
            }
            int rc = append_digit_sequence(c, &term);
            if (rc != 0)
                return rc;
        }
    }

    if (term == 'e' || term == 'E') {
        has_exponent = true;
        if (ds_append(&buffer, (char)term) != 0)
            return 99;

        int c = fgetc(source);
        if (c == '+' || c == '-') {
            if (ds_append(&buffer, (char)c) != 0)
                return 99;
            c = fgetc(source);
        }
        if (!isdigit((unsigned char)c)) {
            return 1;
        }
        int rc = append_digit_sequence(c, &term);
        if (rc != 0)
            return rc;
    }

    if (term == 'd' || term == 'D') {
        if (!has_fraction && !has_exponent) {
            return 1; /* e.g. 1d is not a Double in IFJ26 */
        }
        has_suffix = true;
        if (ds_append(&buffer, (char)term) != 0)
            return 99;
        term = fgetc(source);
    }

    if (has_suffix && (isalnum((unsigned char)term) || term == '_')) {
        return 1;
    }

    int rc = finish_number_delimiter(term);
    if (rc != 0)
        return rc;

    if (has_fraction || has_exponent) {
        token->type = TOKEN_DOUBLE_LITERAL;
        return convert_double(buffer.text, &token->attribute.d_val);
    }

    token->type = TOKEN_INT_LITERAL;
    return convert_int(buffer.text, &token->attribute.i_val);
}

static int read_normal_string(Token *token) {
    ds_clear(&buffer);

    while (true) {
        int c = fgetc(source);
        if (c == EOF || c == '\n' || c == '\r') {
            return 1;
        }
        if (c == '"') {
            token->type = TOKEN_STRING_LITERAL;
            return set_string_attribute(token);
        }
        if (c == '\\') {
            int esc = fgetc(source);
            switch (esc) {
            case '\\':
                c = '\\';
                break;
            case '"':
                c = '"';
                break;
            case 'n':
                c = '\n';
                break;
            case 'r':
                c = '\r';
                break;
            case 't':
                c = '\t';
                break;
            default:
                return 1;
            }
        }
        if (ds_append(&buffer, (char)c) != 0)
            return 99;
    }
}

static int read_multiline_string(Token *token) {
    ds_clear(&buffer);
    int quote_run = 0;

    while (true) {
        int c = fgetc(source);
        if (c == EOF) {
            return 1;
        }

        if (c == '"') {
            ++quote_run;
            if (quote_run == 3) {
                token->type = TOKEN_STRING_LITERAL;
                return set_string_attribute(token);
            }
            continue;
        }

        while (quote_run > 0) {
            if (ds_append(&buffer, '"') != 0)
                return 99;
            --quote_run;
        }

        if (ds_append(&buffer, (char)c) != 0)
            return 99;
    }
}

static int read_string(Token *token) {
    int second = fgetc(source);
    if (second != '"') {
        if (push_char(second) != 0)
            return 99;
        return read_normal_string(token);
    }

    int third = fgetc(source);
    if (third == '"') {
        return read_multiline_string(token);
    }

    /* "" is an empty ordinary string; third character belongs to the next token.
     */
    if (push_char(third) != 0)
        return 99;
    ds_clear(&buffer);
    token->type = TOKEN_STRING_LITERAL;
    return set_string_attribute(token);
}

static int read_at_main(Token *token) {
    static const char expected[] = "main";
    for (size_t i = 0; i < sizeof(expected) - 1; ++i) {
        if (fgetc(source) != expected[i]) {
            return 1;
        }
    }

    int next = fgetc(source);
    if (isalnum((unsigned char)next) || next == '_') {
        return 1;
    }
    if (push_char(next) != 0)
        return 99;

    token->type = TOKEN_AT_MAIN;
    return 0;
}

void scanner_init(FILE *source_file) {
    source = source_file;
    ds_init(&buffer);
    indent_stack[0]     = 0;
    indent_top          = 0;
    pending_dedents     = 0;
    at_line_start       = true;
    line_has_token      = false;
    paren_depth         = 0;
    logical_line_indent = 0;
    continuation_line   = false;
}

int get_next_token(Token *token) {
    if (source == NULL || token == NULL) {
        return 99;
    }

    if (pending_dedents > 0) {
        --pending_dedents;
        token->type = TOKEN_DEDENT;
        return 0;
    }

    while (true) {
        if (at_line_start) {
            int rc = handle_line_start(token);
            if (rc >= 0) {
                return rc;
            }
        }

        int c = fgetc(source);

        if (c == EOF) {
            if (indent_top > 0) {
                pending_dedents = indent_top;
                indent_top      = 0;
                --pending_dedents;
                token->type = TOKEN_DEDENT;
                return 0;
            }
            token->type = TOKEN_EOF;
            return 0;
        }

        if (c == ' ' || c == '\t' || c == '\f' || c == '\v') {
            continue;
        }

        if (c == '\r') {
            int next = fgetc(source);
            if (next != '\n')
                return 1;
            c = '\n';
        }

        if (c == '\n') {
            at_line_start = true;
            if (paren_depth > 0) {
                continuation_line = true;
                continue;
            }
            if (!line_has_token) {
                continue;
            }
            line_has_token = false;
            token->type    = TOKEN_EOL;
            return 0;
        }

        if (isalpha((unsigned char)c) || c == '_') {
            int rc = read_identifier(token, c);
            if (rc == 0)
                line_has_token = true;
            return rc;
        }

        if (isdigit((unsigned char)c)) {
            int rc = read_number(token, c);
            if (rc == 0)
                line_has_token = true;
            return rc;
        }

        if (c == '.') {
            int next = fgetc(source);
            if (isdigit((unsigned char)next)) {
                if (push_char(next) != 0)
                    return 99;
                int rc = read_number(token, '.');
                if (rc == 0)
                    line_has_token = true;
                return rc;
            }
            if (push_char(next) != 0)
                return 99;
            token->type    = TOKEN_DOT;
            line_has_token = true;
            return 0;
        }

        if (c == '"') {
            int rc = read_string(token);
            if (rc == 0)
                line_has_token = true;
            return rc;
        }

        if (c == '/') {
            int next = fgetc(source);
            if (next == '/') {
                while ((c = fgetc(source)) != '\n' && c != EOF) {
                    ;
                }

                if (c == EOF) {
                    if (indent_top > 0) {
                        pending_dedents = indent_top;
                        indent_top      = 0;
                        --pending_dedents;
                        token->type = TOKEN_DEDENT;
                        return 0;
                    }
                    token->type = TOKEN_EOF;
                    return 0;
                }

                at_line_start = true;
                if (paren_depth > 0) {
                    continuation_line = true;
                    continue;
                }
                if (!line_has_token) {
                    continue;
                }
                line_has_token = false;
                token->type    = TOKEN_EOL;
                return 0;
            }

            if (next == '*') {
                bool had_newline = false;
                int  rc          = consume_block_comment(&had_newline);
                if (rc != 0)
                    return rc;
                if (had_newline) {
                    rc = finish_multiline_comment(token);
                    if (rc >= 0)
                        return rc;
                    continue;
                }
                continue;
            }

            if (push_char(next) != 0)
                return 99;
            token->type    = TOKEN_DIV;
            line_has_token = true;
            return 0;
        }

        if (c == '@') {
            int rc = read_at_main(token);
            if (rc == 0)
                line_has_token = true;
            return rc;
        }

        if (c == '=') {
            int next = fgetc(source);
            if (next == '=') {
                token->type = TOKEN_EQ;
            } else {
                if (push_char(next) != 0)
                    return 99;
                token->type = TOKEN_ASSIGN;
            }
            line_has_token = true;
            return 0;
        }

        if (c == '!') {
            int next = fgetc(source);
            if (next != '=')
                return 1;
            token->type    = TOKEN_NEQ;
            line_has_token = true;
            return 0;
        }

        if (c == '<' || c == '>') {
            int  next  = fgetc(source);
            bool equal = next == '=';
            if (!equal && push_char(next) != 0)
                return 99;
            token->type =
                c == '<' ? (equal ? TOKEN_LEQ : TOKEN_LT) : (equal ? TOKEN_GEQ : TOKEN_GT);
            line_has_token = true;
            return 0;
        }

        switch (c) {
        case '+':
            token->type = TOKEN_PLUS;
            break;
        case '-':
            token->type = TOKEN_MINUS;
            break;
        case '*':
            token->type = TOKEN_MUL;
            break;
        case ':':
            token->type = TOKEN_COLON;
            break;
        case ',':
            token->type = TOKEN_COMMA;
            break;
        case ';':
            token->type = TOKEN_SEMICOLON;
            break;
        case '(':
            if (paren_depth == 0)
                logical_line_indent = indent_stack[indent_top];
            ++paren_depth;
            token->type = TOKEN_LPAREN;
            break;
        case ')':
            if (paren_depth > 0)
                --paren_depth;
            token->type = TOKEN_RPAREN;
            break;
        default:
            return 1;
        }

        line_has_token = true;
        return 0;
    }
}

void scanner_free(void) {
    ds_free(&buffer);
    source = NULL;
}