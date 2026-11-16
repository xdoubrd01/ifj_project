/**
 *@author Denis Doubrava xdoubrd01
 * Použitý model: GPT-6.1 Sol
 * Tvorba komentářů byla za pomoci umělé inteligence: překlad, formátování a lepší formulace vět.
 */

/**
 * @file scanner.c
 * @brief Stateful lexical analyzer for the base IFJ26 language.
 */

#include "scanner.h"

#include <errno.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief Resizable buffer used while reading a lexeme.
 *
 * The buffer always contains a terminating zero after its contents.
 * Its allocated storage can be reused for subsequent tokens.
 */
typedef struct {
    char  *text;
    size_t length;
    size_t capacity;
} DynamicString;

/* The stream belongs to the caller; the scanner never closes it. */
static FILE         *source = NULL;
static DynamicString buffer;

/* Only one scanner instance is active at a time. */
static size_t *indent_stack    = NULL;
static size_t  indent_capacity = 0;
// Index of the active block level; level zero is the unindented root.
static size_t indent_top = 0;
// Number of already determined block endings still waiting to be returned.
static size_t pending_dedents = 0;
static bool   at_line_start   = true;
// Tracks whether the current logical line needs an EOL separator.
static bool   line_has_token = false;
static size_t paren_depth    = 0;
// Indentation of the first physical line of the current logical line.
static size_t logical_line_indent = 0;
static bool   continuation_line   = false;
// LF skipped between real lexemes, retained across virtual layout tokens.
static bool skipped_newline = false;

/* Two slots avoid relying on multiple successful calls to ungetc(). */
static int    pushed_chars[2];
static size_t pushed_count = 0;

/**
 * @brief Tests an ASCII digit independently of the active character locale.
 */
static bool is_digit(int c) { return c >= '0' && c <= '9'; }

/**
 * @brief Tests an ASCII letter independently of the active character locale.
 */
static bool is_letter(int c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }

/**
 * @brief Tests a character permitted after the first identifier character.
 */
static bool is_name_char(int c) { return is_letter(c) || is_digit(c) || c == '_'; }

/**
 * @brief Reads buffered lookahead first, then the caller-owned stream.
 */
static int read_char(void) {
    // Use the most recently returned character before reading new input.
    if (pushed_count > 0) {
        return pushed_chars[--pushed_count];
    }

    // Keep the result as int so every byte remains distinguishable from EOF.
    return fgetc(source);
}

/**
 * @brief Initializes an empty dynamic string without allocating storage.
 */
static void ds_init(DynamicString *s) {
    // A newly initialized buffer owns no storage and may be freed safely.
    s->text     = NULL;
    s->length   = 0;
    s->capacity = 0;
}

/**
 * @brief Empties a dynamic string while retaining its allocated capacity.
 */
static void ds_clear(DynamicString *s) {
    // Forget the previous lexeme while retaining storage for the next one.
    s->length = 0;

    if (s->text != NULL) {
        // Keep the empty buffer usable as a C string when storage already exists.
        s->text[0] = '\0';
    }
}

/**
 * @brief Appends one character and preserves the terminating zero.
 *
 * @return 0 on success or 99 on allocation failure or size overflow.
 */
static int ds_append(DynamicString *s, char c) {
    /* Reserve the character and its terminating zero without size overflow. */
    if (s->length > SIZE_MAX - 2) {
        return 99;
    }

    // length excludes the terminating zero; this append needs two more bytes.
    size_t required = s->length + 2;

    if (required > s->capacity) {
        // Grow geometrically to avoid reallocating for every character.
        size_t capacity = s->capacity == 0 ? 32 : s->capacity;

        while (capacity < required) {
            if (capacity > SIZE_MAX / 2) {
                // If doubling would overflow, request only the exact required size.
                capacity = required;
                break;
            }

            capacity *= 2;
        }

        /* Keep the old allocation valid if realloc() fails. */
        char *text = realloc(s->text, capacity);

        if (text == NULL) {
            return 99;
        }

        // Publish the replacement pointer only after successful reallocation.
        s->text     = text;
        s->capacity = capacity;
    }

    // Append after existing contents and keep length independent of capacity.
    s->text[s->length++] = c;
    s->text[s->length]   = '\0';

    return 0;
}

/**
 * @brief Releases a dynamic string and clears its fields.
 */
static void ds_free(DynamicString *s) {
    free(s->text);

    // Clear ownership fields so repeated cleanup does not free the same pointer.
    s->text     = NULL;
    s->length   = 0;
    s->capacity = 0;
}

/**
 * @brief Copies the lexeme into a caller-owned token attribute.
 *
 * @return 0 on success or 99 if allocation fails.
 *
 * @note The parser must free this copy.
 * @note The returned attribute survives reuse of the shared scanner buffer.
 */
static int set_string_attribute(Token *token) {
    // Empty literals may have no shared buffer allocation yet.
    const char *src = buffer.text != NULL ? buffer.text : "";

    // Allocate a separate copy so the token survives reading another lexeme.
    token->attribute.s_val = malloc(buffer.length + 1);

    if (token->attribute.s_val == NULL) {
        return 99;
    }

    // Copy the contents and terminating zero; ownership passes to the caller.
    memcpy(token->attribute.s_val, src, buffer.length + 1);

    return 0;
}

/**
 * @brief Stores lookahead locally; EOF requires no pushback.
 *
 * @return 0 on success or 99 if the internal pushback buffer is full.
 */
static int push_char(int c) {
    // EOF is a stream condition, not a byte that needs to be stored.
    if (c == EOF) {
        return 0;
    }

    if (pushed_count == sizeof(pushed_chars) / sizeof(pushed_chars[0])) {
        return 99;
    }

    // Store in reverse reading order because read_char() uses this as a stack.
    pushed_chars[pushed_count++] = c;

    return 0;
}

/**
 * @brief Ensures that the indentation stack can hold the requested levels.
 *
 * @return 0 on success or 99 on allocation failure or size overflow.
 */
/**
 * Začátek AI části
 * Použitý model: GPT-6.1 Sol
 * Rozsah: Nová pomocná funkce pro dynamický zásobník odsazení.
 * Otestuj zpracování odsazení, zejména více vnořených bloků, návraty na předchozí úrovně,
 * prázdné řádky, komentáře a pokračování uvnitř závorek. Prověř omezení pevně velkého
 * zásobníku odsazení. Pokud omezuje zpracování platného vstupu, převeď jej na dynamicky
 * zvětšovaný zásobník. Ošetři selhání alokace, přetečení velikostí a uvolnění paměti.
 */
static int reserve_indents(size_t required) {
    // required counts levels, including the root level with zero indentation.
    if (required <= indent_capacity) {
        return 0;
    }

    // Validate the byte count before multiplying a level count by its size.
    if (required > SIZE_MAX / sizeof(*indent_stack)) {
        return 99;
    }

    // The first allocation reserves several levels; later allocations grow it.
    size_t capacity = indent_capacity == 0 ? 16 : indent_capacity;

    while (capacity < required) {
        if (capacity > SIZE_MAX / sizeof(*indent_stack) / 2) {
            // Avoid overflow while still satisfying this particular request.
            capacity = required;
            break;
        }

        capacity *= 2;
    }

    // Reallocation retains existing indentation levels and preserves them on failure.
    size_t *stack = realloc(indent_stack, capacity * sizeof(*stack));

    if (stack == NULL) {
        return 99;
    }

    // Update the global pointer and capacity together after successful growth.
    indent_stack    = stack;
    indent_capacity = capacity;

    return 0;
}
/**
 * Konec AI části: reserve_indents()
 */

/**
 * @brief Emits remaining block endings before the final EOF token.
 *
 * Only one token can be returned per call. Additional DEDENT tokens are
 * therefore queued and delivered by subsequent calls.
 */
static int emit_eof(Token *token) {
    if (indent_top > 0) {
        // Return one DEDENT now and remember the remaining block endings.
        pending_dedents = indent_top - 1;
        // The root level remains; the queued tokens represent levels already removed.
        indent_top  = 0;
        token->type = TOKEN_DEDENT;
    } else {
        // Once all open indentation levels are handled, EOF can reach the parser.
        token->type = TOKEN_EOF;
    }

    return 0;
}

/**
 * @brief Recognizes base-language keywords.
 *
 * Other names, including extension-only words, remain identifiers here.
 */
static TokenType keyword_or_identifier(const char *text) {
    // Compare the complete name, so a keyword prefix never splits an identifier.
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

    // Type names are case-sensitive: Int and int are different names.
    if (strcmp(text, "Int") == 0)
        return TOKEN_TYPE_INT;

    if (strcmp(text, "Double") == 0)
        return TOKEN_TYPE_DOUBLE;

    if (strcmp(text, "String") == 0)
        return TOKEN_TYPE_STRING;

    if (strcmp(text, "Unit") == 0)
        return TOKEN_TYPE_UNIT;

    // An unmatched name is an identifier rather than an unknown keyword error.
    return TOKEN_IDENTIFIER;
}

/**
 * @brief Removes digit separators and converts an Int literal.
 *
 * @return 0 on success or 99 on temporary-buffer allocation failure.
 *
 * @note Integer overflow is not checked, as permitted by the assignment.
 */
static int convert_int(const char *text, int *out) {
    // Build a conversion-only copy without changing the shared lexeme buffer.
    DynamicString tmp;
    ds_init(&tmp);

    /* strtol() does not accept the source-language underscore separators. */
    for (size_t i = 0; text[i] != '\0'; ++i) {
        if (text[i] != '_') {
            int rc = ds_append(&tmp, text[i]);

            if (rc != 0) {
                // Release the partially built temporary string before propagating allocation
                // failure.
                ds_free(&tmp);
                return rc;
            }
        }
    }

    // Base ten prevents a leading zero from selecting octal conversion.
    *out = (int)strtol(tmp.text != NULL ? tmp.text : "0", NULL, 10);

    ds_free(&tmp);

    return 0;
}

/**
 * @brief Removes separators and suffix before converting a Double literal.
 *
 * @return 0 on success, 1 on a rejected conversion, or 99 on allocation failure.
 *
 * @note Representable nonzero subnormal values are accepted.
 * @note Conversion assumes the standard C numeric locale.
 */
static int convert_double(const char *text, double *out) {
    // Keep syntax recognition separate from conversion to the numeric value.
    DynamicString tmp;
    ds_init(&tmp);

    for (size_t i = 0; text[i] != '\0'; ++i) {
        char c = text[i];

        // Separators affect source spelling but not the numerical value.
        if (c == '_') {
            continue;
        }

        /* The optional source-language suffix is not passed to strtod(). */
        if ((c == 'd' || c == 'D') && text[i + 1] == '\0') {
            continue;
        }

        int rc = ds_append(&tmp, c);

        if (rc != 0) {
            ds_free(&tmp);
            return rc;
        }
    }

    // Reset errno so an earlier library failure cannot affect this conversion.
    errno     = 0;
    char *end = NULL;
    // end identifies the first character not consumed by the C conversion.
    double value = strtod(tmp.text != NULL ? tmp.text : "0", &end);

    /* Reject overflow and complete underflow of a nonzero literal to zero. */
    if (!isfinite(value) || end == NULL || *end != '\0' || (errno == ERANGE && value == 0.0)) {
        ds_free(&tmp);
        return 1;
    }

    // Store the result only after conversion and range checks have succeeded.
    *out = value;

    ds_free(&tmp);

    return 0;
}

/**
 * @brief Consumes a block comment after its opening delimiter was read.
 *
 * @param had_newline Receives true if the comment contained LF.
 * @return 0 on success, 1 for an unterminated comment, or 99 on counter overflow.
 */
static int consume_block_comment(bool *had_newline) {
    // The caller has already consumed the first opening delimiter.
    size_t depth = 1;
    *had_newline = false;

    while (depth > 0) {
        int c = read_char();

        // Every opened comment must close before the input ends.
        if (c == EOF) {
            return 1;
        }

        if (c == '\n') {
            // Preserve LF for parser metadata as well as the comment-specific result.
            skipped_newline = true;
            *had_newline    = true;
            continue;
        }

        // A slash inside a comment may begin another nested comment.
        if (c == '/') {
            int next = read_char();

            if (next == '*') {
                /* Every nested opening requires its own closing delimiter. */
                if (depth == SIZE_MAX) {
                    return 99;
                }

                ++depth;
            } else {
                // A failed delimiter attempt must not discard the lookahead character.
                int rc = push_char(next);

                if (rc != 0)
                    return rc;
            }
        } else if (c == '*') {
            int next = read_char();

            if (next == '/') {
                // Only the outermost matching closing delimiter finishes this function.
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

/**
 * @brief Finishes a block comment that contained a physical newline.
 *
 * @return 0 if EOL was produced, -1 to continue scanning, or 1 on lexical error.
 *
 * @note Only whitespace may follow the closing delimiter before LF or EOF.
 */
static int finish_multiline_comment(Token *token) {
    int c;

    // Consume the permitted whitespace following a multiline comment.
    do {
        c = read_char();
    } while (c == ' ' || c == '\t' || c == '\r' || c == '\f' || c == '\v');

    // Another token on this closing physical line violates the comment rule.
    if (c != '\n' && c != EOF) {
        return 1;
    }

    // The following read must measure indentation from the next physical line.
    at_line_start = true;

    // Inside parentheses, a physical line break does not terminate the statement.
    if (paren_depth > 0) {
        continuation_line = true;

        /* The same logical line continues inside parentheses. */
        return -1;
    }

    continuation_line = false;

    if (!line_has_token) {
        /* A comment-only line does not produce a statement separator. */
        return -1;
    }

    // Outside parentheses, emit one separator for the preceding nonempty line.
    line_has_token = false;
    token->type    = TOKEN_EOL;

    return 0;
}

/**
 * @brief Measures indentation while ignoring blank and comment-only lines.
 *
 * @return 0 if a token was produced, -1 to continue scanning, or 1/99 on error.
 *
 * @note Continuation lines inside parentheses never create block markers.
 */
/**
 * Začátek AI části
 * Použitý model: GPT-6.1 Sol
 * Rozsah: Úpravy původní funkce pro práci s dynamickým zásobníkem.
 * Otestuj zpracování odsazení, zejména více vnořených bloků, návraty na předchozí úrovně,
 * prázdné řádky, komentáře a pokračování uvnitř závorek. Prověř omezení pevně velkého
 * zásobníku odsazení. Pokud omezuje zpracování platného vstupu, převeď jej na dynamicky
 * zvětšovaný zásobník. Ošetři selhání alokace, přetečení velikostí a uvolnění paměti.
 */
static int handle_line_start(Token *token) {
    // Measure this physical line separately from the stored block levels.
    size_t spaces             = 0;
    bool   indentation_locked = false;
    // Delay rejecting indentation whitespace until a real token is found.
    bool invalid_indent_whitespace = false;

    while (at_line_start) {
        int c;

        // A leading same-line block comment ends the measurable space prefix.
        if (!indentation_locked) {
            /* Only literal spaces contribute to the indentation width. */
            while (true) {
                c = read_char();

                if (c == ' ') {
                    if (spaces == SIZE_MAX) {
                        return 99;
                    }

                    ++spaces;
                    continue;
                }

                // Blank and comment-only lines may still be ignored despite this flag.
                if (c == '\t' || c == '\f' || c == '\v') {
                    invalid_indent_whitespace = true;
                    continue;
                }

                break;
            }
        } else {
            /*
             * A same-line block comment before the first token behaves as
             * whitespace. Spaces after it no longer belong to the initial
             * indentation prefix.
             */
            do {
                c = read_char();

                if (c == '\t' || c == '\f' || c == '\v') {
                    invalid_indent_whitespace = true;
                }
            } while (c == ' ' || c == '\t' || c == '\f' || c == '\v');
        }

        /* Accept CRLF input as a physical line ending. */
        if (c == '\r') {
            int next = read_char();

            if (next != '\n') {
                return 1;
            }

            c = '\n';
        }

        if (c == '\n') {
            skipped_newline = true;

            /* Blank lines do not change indentation. */
            spaces                    = 0;
            indentation_locked        = false;
            invalid_indent_whitespace = false;
            continuation_line         = paren_depth > 0;

            continue;
        }

        // No new token follows, but existing blocks may still need DEDENT tokens.
        if (c == EOF) {
            at_line_start     = false;
            continuation_line = false;

            return emit_eof(token);
        }

        /* Comment-only lines must not change indentation. */
        if (c == '/') {
            int next = read_char();

            // Discard this comment-only line before considering changes in block depth.
            if (next == '/') {
                while ((c = read_char()) != '\n' && c != EOF) {
                    ;
                }

                if (c == EOF) {
                    at_line_start     = false;
                    continuation_line = false;

                    return emit_eof(token);
                }

                skipped_newline           = true;
                spaces                    = 0;
                indentation_locked        = false;
                invalid_indent_whitespace = false;
                continuation_line         = paren_depth > 0;

                continue;
            }

            if (next == '*') {
                // Distinguish a same-line whitespace comment from a logical line ending.
                bool had_newline = false;
                int  rc          = consume_block_comment(&had_newline);

                if (rc != 0) {
                    return rc;
                }

                if (had_newline) {
                    rc = finish_multiline_comment(token);

                    // Zero means a token was emitted; a positive value is an error to
                    // propagate.
                    if (rc >= 0) {
                        return rc;
                    }

                    spaces                    = 0;
                    indentation_locked        = false;
                    invalid_indent_whitespace = false;

                    continue;
                }

                /* Preserve spaces measured before a same-line block comment. */
                indentation_locked = true;

                continue;
            }

            /* This slash is an operator rather than a comment opening. */
            if (invalid_indent_whitespace) {
                return 1;
            }

            // Push lookahead first so the slash is read first by token recognition.
            if (push_char(next) != 0 || push_char(c) != 0) {
                return 99;
            }
        } else {
            /* The physical line contains a real token. */
            if (invalid_indent_whitespace) {
                return 1;
            }

            if (push_char(c) != 0) {
                return 99;
            }
        }

        if (paren_depth > 0) {
            /*
             * A continuation must be indented beyond the beginning of its
             * logical line, without changing the block indentation stack.
             */
            // Compare against the first physical line of the logical line, not the last
            // continuation.
            if (continuation_line && spaces <= logical_line_indent) {
                return 1;
            }

            at_line_start     = false;
            continuation_line = false;

            // A continuation needs no layout token; the caller proceeds with the saved
            // character.
            return -1;
        }

        at_line_start     = false;
        continuation_line = false;
        // Remember the starting indentation before any parenthesized continuation.
        logical_line_indent = spaces;

        if (spaces > indent_stack[indent_top]) {
            /* Allocate another level instead of imposing a fixed depth limit. */
            if (indent_top > SIZE_MAX - 2 || reserve_indents(indent_top + 2) != 0) {
                return 99;
            }

            // Store the actual space count rather than assuming a fixed indentation width.
            indent_stack[++indent_top] = spaces;
            token->type                = TOKEN_INDENT;

            return 0;
        }

        if (spaces < indent_stack[indent_top]) {
            size_t count = 0;

            /* One physical line may close multiple nested blocks. */
            while (indent_top > 0 && spaces < indent_stack[indent_top]) {
                --indent_top;
                ++count;
            }

            /* A dedent must return to an existing indentation level. */
            if (spaces != indent_stack[indent_top]) {
                return 1;
            }

            // One DEDENT is returned now; the public API returns the rest on later calls.
            pending_dedents = count - 1;
            token->type     = TOKEN_DEDENT;

            return 0;
        }

        return -1;
    }

    return -1;
}
/**
 * Konec AI části: handle_line_start()
 */

/**
 * @brief Reads an ASCII name and classifies keywords.
 *
 * A lone underscore is not an identifier in the base language.
 */
static int read_identifier(Token *token, int first) {
    // Start a fresh name; the first character has already been consumed.
    ds_clear(&buffer);

    if (ds_append(&buffer, (char)first) != 0)
        return 99;

    int c;

    while ((c = read_char()) != EOF && is_name_char(c)) {
        if (ds_append(&buffer, (char)c) != 0)
            return 99;
    }

    // Preserve the first delimiter, such as a space, parenthesis, or operator.
    if (push_char(c) != 0)
        return 99;

    if (buffer.length == 1 && buffer.text[0] == '_') {
        return 1;
    }

    // Classify only after reading the whole name, including any keyword-like prefix.
    token->type = keyword_or_identifier(buffer.text);

    // Only identifiers need a copied name; keyword tokens carry their meaning in type.
    if (token->type == TOKEN_IDENTIFIER) {
        return set_string_attribute(token);
    }

    return 0;
}

/**
 * @brief Reads digits with optional runs of underscores between digits.
 *
 * @param first_digit First digit, or a negative value if already appended.
 * @param terminator Receives the first character outside the digit sequence.
 * @return 0 on success, 1 on invalid separators, or 99 on allocation failure.
 */
static int append_digit_sequence(int first_digit, int *terminator) {
    // A negative argument means the initial digit is already present in the buffer.
    if (first_digit >= 0) {
        if (!is_digit(first_digit))
            return 1;

        if (ds_append(&buffer, (char)first_digit) != 0)
            return 99;
    }

    while (true) {
        int c = read_char();

        if (is_digit(c)) {
            if (ds_append(&buffer, (char)c) != 0)
                return 99;

            continue;
        }

        // A separator run is valid only between digits, never at the end of a part.
        if (c == '_') {
            /* Multiple separators are valid, but a digit must follow the run. */
            do {
                if (ds_append(&buffer, '_') != 0)
                    return 99;

                c = read_char();
            } while (c == '_');

            // A separator run must end with a digit, including when EOF follows it.
            if (!is_digit(c))
                return 1;

            if (ds_append(&buffer, (char)c) != 0)
                return 99;

            continue;
        }

        // Return the consumed delimiter so read_number() can recognize dot, exponent, or
        // suffix.
        *terminator = c;

        return 0;
    }
}

/**
 * @brief Rejects an invalid numeric suffix and preserves the next delimiter.
 */
static int finish_number_delimiter(int c) {
    // Do not accept an unsupported letter suffix as a valid numeric spelling.
    if (is_letter(c) || c == '_') {
        return 1;
    }

    // Punctuation and whitespace belong to subsequent scanning, so preserve them.
    return push_char(c);
}

/**
 * @brief Reads an Int or Double literal.
 *
 * Supports fractional parts, exponents, digit separators, and the optional
 * d/D suffix on the supported Double forms.
 */

/**
 * Začátek AI části
 * Použitý model: GPT-6.1 Sol
 * Rozsah: Úpravy původního rozpoznávání číselných literálů.
 * Porovnej funkci read_number() s mým návrhem DKA, specifikací jazyka a poznatky z fóra.
 * Otestuj podtržítka mezi číslicemi, exponenty, přípony a úvodní nuly. Zápisy jako 01.5 musí být
 * odmítnuty, zatímco 0.5, .5 a 2e001 mají být přijaty. Nalezené chyby oprav a vysvětli změny
 * v implementaci.
 */
static int read_number(Token *token, int first) {
    ds_clear(&buffer);

    // These flags distinguish an Int from the supported Double forms.
    bool has_fraction = false;
    bool has_exponent = false;
    bool has_suffix   = false;
    int  term         = EOF;

    // The dispatcher already identified a digit after this initial dot.
    if (first == '.') {
        has_fraction = true;

        if (ds_append(&buffer, '.') != 0) {
            return 99;
        }

        int c = read_char();

        if (!is_digit(c)) {
            return 1;
        }

        int rc = append_digit_sequence(c, &term);

        if (rc != 0) {
            return rc;
        }
    } else {
        /* A leading zero must be the only digit of the whole-number part. */
        // Permit a single whole-part zero but reject 01, 00.5, and 0_1.5.
        if (first == '0') {
            if (ds_append(&buffer, '0') != 0) {
                return 99;
            }

            // The following character may start a fraction or exponent, or end the literal.
            term = read_char();

            /* Reject additional whole-part digits, including separated ones. */
            if (is_digit(term) || term == '_') {
                return 1;
            }
        } else {
            int rc = append_digit_sequence(first, &term);

            if (rc != 0) {
                return rc;
            }
        }

        // A decimal point after whole-part digits requires fractional digits.
        if (term == '.') {
            has_fraction = true;

            if (ds_append(&buffer, '.') != 0) {
                return 99;
            }

            int c = read_char();

            /* A fractional part is mandatory after the decimal point. */
            if (!is_digit(c)) {
                return 1;
            }

            int rc = append_digit_sequence(c, &term);

            if (rc != 0) {
                return rc;
            }
        }
    }

    // Exponent digits use the separator reader and may have leading zeros.
    if (term == 'e' || term == 'E') {
        has_exponent = true;

        if (ds_append(&buffer, (char)term) != 0)
            return 99;

        int c = read_char();

        if (c == '+' || c == '-') {
            if (ds_append(&buffer, (char)c) != 0)
                return 99;

            c = read_char();
        }

        /* An exponent sign must be followed by at least one digit. */
        if (!is_digit(c)) {
            return 1;
        }

        int rc = append_digit_sequence(c, &term);

        if (rc != 0)
            return rc;
    }

    // A suffix completes a Double spelling; it does not turn a plain Int into a Double.
    if (term == 'd' || term == 'D') {
        if (!has_fraction && !has_exponent) {
            /* For example, 1d is not one of the specified Double forms. */
            return 1;
        }

        has_suffix = true;

        if (ds_append(&buffer, (char)term) != 0)
            return 99;

        term = read_char();
    }

    // After the suffix, another name character would extend an invalid literal.
    if (has_suffix && is_name_char(term)) {
        return 1;
    }

    // Preserve the delimiter before converting the completed lexeme.
    int rc = finish_number_delimiter(term);

    if (rc != 0)
        return rc;

    // At least one of these parts is required for a supported Double literal.
    if (has_fraction || has_exponent) {
        token->type = TOKEN_DOUBLE_LITERAL;

        return convert_double(buffer.text, &token->attribute.d_val);
    }

    /* The leading-zero restriction applies to nonzero Int literals only. */
    // The earlier whole-part check already rejects additional digits after zero.
    if (buffer.text[0] == '0') {
        for (size_t i = 1; i < buffer.length; ++i) {
            if (buffer.text[i] >= '1' && buffer.text[i] <= '9') {
                return 1;
            }
        }
    }

    // Without fraction or exponent, return the integer attribute instead.
    token->type = TOKEN_INT_LITERAL;

    return convert_int(buffer.text, &token->attribute.i_val);
}
/**
 * Konec AI části: read_number()
 */

/**
 * @brief Decodes the five supported escapes in a single-line string.
 *
 * @return 0 on success, 1 on invalid contents, or 99 on allocation failure.
 */
static int read_normal_string(Token *token) {
    // Store the decoded value rather than the source delimiters.
    ds_clear(&buffer);

    while (true) {
        int c = read_char();

        // A normal string must close before a physical line ending or EOF.
        if (c == EOF || c == '\n' || c == '\r') {
            return 1;
        }

        // The closing quote is consumed but is not included in the value.
        if (c == '"') {
            token->type = TOKEN_STRING_LITERAL;

            return set_string_attribute(token);
        }

        /* Only the escapes explicitly listed in the assignment are accepted. */
        if (c == '\\') {
            int esc = read_char();

            // Replace each supported escape spelling with its actual character value.
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

            // An unsupported escape is a lexical error rather than a literal backslash
            // sequence.
            default:
                return 1;
            }
        }

        // Append either an ordinary character or the decoded escape result.
        if (ds_append(&buffer, (char)c) != 0)
            return 99;
    }
}

/**
 * @brief Reads raw contents and consumes the entire closing quote run.
 *
 * A run of at least three quotes closes the literal. Its final three quotes
 * are delimiters; any preceding quotes in that run belong to the value.
 */
/**
 * Začátek AI části
 * Použitý model: GPT-6.1 Sol
 * Rozsah: Přepracování zpracování uzavírací řady uvozovek.
 * Zkontroluj zpracování uvozovek ve víceřádkových řetězcích podle specifikace a upřesnění
 * z fóra. Otestuj především vstupy tvořené sedmi a osmi uvozovkami za sebou, jednotlivé
 * uvozovky uvnitř řetězce a neukončené řetězce. Oprav funkci tak, aby správně rozlišovala
 * obsah řetězce a jeho ukončovací uvozovky.
 */
static int read_multiline_string(Token *token) {
    // The opening triple quote has already been consumed by read_string().
    ds_clear(&buffer);

    while (true) {
        int c = read_char();

        if (c == '"') {
            // Count only this consecutive quote run, not quotes elsewhere in the literal.
            size_t quotes = 0;

            /* Read the complete consecutive run before deciding its contents. */
            do {
                // Temporarily retain every quote; delimiter quotes are removed after counting.
                if (ds_append(&buffer, '"') != 0) {
                    return 99;
                }

                ++quotes;
                c = read_char();
            } while (c == '"');

            if (quotes >= 3) {
                /* Remove only the final three delimiter quotes. */
                // Any earlier quotes in the same closing run remain part of the string value.
                buffer.length -= 3;
                buffer.text[buffer.length] = '\0';

                // The first character after the quote run belongs to the following input.
                if (push_char(c) != 0) {
                    return 99;
                }

                token->type = TOKEN_STRING_LITERAL;

                return set_string_attribute(token);
            }

            /* One or two quotes were already appended as ordinary contents. */
        }

        // Fewer than three closing quotes cannot finish the multiline literal.
        if (c == EOF) {
            return 1;
        }

        /* Backslashes and physical newlines are stored without decoding. */
        if (ds_append(&buffer, (char)c) != 0) {
            return 99;
        }
    }
}
/**
 * Konec AI části: read_multiline_string()
 */

/**
 * @brief Distinguishes ordinary, empty, and triple-quoted string literals.
 *
 * The first opening quote has already been consumed by the caller.
 */
static int read_string(Token *token) {
    // One quote is already consumed; inspect the next two to choose the string form.
    int second = read_char();

    if (second != '"') {
        // For an ordinary string, this character is still part of its contents.
        if (push_char(second) != 0)
            return 99;

        return read_normal_string(token);
    }

    int third = read_char();

    // Three opening quotes select raw multiline handling.
    if (third == '"') {
        return read_multiline_string(token);
    }

    /* Two quotes form an empty ordinary string. */
    // After an empty ordinary string, preserve the start of the next token.
    if (push_char(third) != 0)
        return 99;

    ds_clear(&buffer);
    token->type = TOKEN_STRING_LITERAL;

    return set_string_attribute(token);
}

/**
 * @brief Recognizes the indivisible @main annotation.
 *
 * Rejects whitespace after @ and identifier continuations such as @mainName.
 */
static int read_at_main(Token *token) {
    // The dispatcher consumed @; no intervening whitespace is permitted.
    static const char expected[] = "main";

    for (size_t i = 0; i < sizeof(expected) - 1; ++i) {
        if (read_char() != expected[i]) {
            return 1;
        }
    }

    int next = read_char();

    // Reject a longer annotation name instead of splitting it at the main prefix.
    if (is_name_char(next)) {
        return 1;
    }

    // Preserve punctuation or whitespace following the complete annotation.
    if (push_char(next) != 0)
        return 99;

    token->type = TOKEN_AT_MAIN;

    return 0;
}

/**
 * @brief Recognizes the next token while handling layout and comments.
 *
 * @return 0 on success, 1 on lexical error, or 99 on internal failure.
 */
/**
 * Začátek AI části
 * Použitý model: GPT-6.1 Sol
 * Rozsah: Vyčlenění původního čtení tokenů a doplnění sledování nových řádků.
 * Zkontroluj předávání tokenů ze scanneru parseru a návratové kódy při lexikální, interní
 * a vstupní chybě. Ověř, zda parser dostává dostatek informací pro kontrolu koncové čárky
 * před uzavírací závorkou na dalším řádku. Podle potřeby doplň informaci o přeskočeném
 * novém řádku a odděl samotné rozpoznávání tokenů od obsluhy veřejného rozhraní. Vysvětli
 * také vlastnictví paměti identifikátorů a řetězců.
 */
static int scan_next(Token *token) {
    if (source == NULL || token == NULL) {
        return 99;
    }

    /* Deliver queued block endings before reading further input. */
    if (pending_dedents > 0) {
        --pending_dedents;
        token->type = TOKEN_DEDENT;

        return 0;
    }

    while (true) {
        // Handle indentation before reading the first token of a physical line.
        if (at_line_start) {
            int rc = handle_line_start(token);

            // Return an emitted layout token or an error; -1 means continue scanning.
            if (rc >= 0) {
                return rc;
            }
        }

        int c = read_char();

        // Drain the indentation stack before reporting final end of input.
        if (c == EOF) {
            return emit_eof(token);
        }

        /* Horizontal whitespace between tokens has no syntactic meaning. */
        if (c == ' ' || c == '\t' || c == '\f' || c == '\v') {
            continue;
        }

        if (c == '\r') {
            int next = read_char();

            if (next != '\n')
                return 1;

            c = '\n';
        }

        // Remember physical LF even when parentheses suppress an EOL token.
        if (c == '\n') {
            skipped_newline = true;
            at_line_start   = true;

            if (paren_depth > 0) {
                continuation_line = true;
                continue;
            }

            // An empty logical line must not create another statement separator.
            if (!line_has_token) {
                continue;
            }

            line_has_token = false;
            token->type    = TOKEN_EOL;

            return 0;
        }

        // Recognize the entire name before deciding whether it is a keyword.
        if (is_letter(c) || c == '_') {
            int rc = read_identifier(token, c);

            if (rc == 0)
                line_has_token = true;

            return rc;
        }

        // A digit starts numeric recognition; plus and minus remain separate operators.
        if (is_digit(c)) {
            int rc = read_number(token, c);

            if (rc == 0)
                line_has_token = true;

            return rc;
        }

        if (c == '.') {
            int next = read_char();

            if (is_digit(next)) {
                /* A dot followed by a digit starts a Double such as .5. */
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

        // The string reader chooses ordinary or multiline form from further quotes.
        if (c == '"') {
            int rc = read_string(token);

            if (rc == 0)
                line_has_token = true;

            return rc;
        }

        if (c == '/') {
            int next = read_char();

            // A line comment consumes its final LF here instead of pushing it back.
            if (next == '/') {
                while ((c = read_char()) != '\n' && c != EOF) {
                    ;
                }

                if (c == EOF) {
                    return emit_eof(token);
                }

                skipped_newline = true;
                at_line_start   = true;

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

                // A block comment containing LF also imposes a restriction after its closing
                // delimiter.
                if (had_newline) {
                    rc = finish_multiline_comment(token);

                    if (rc >= 0)
                        return rc;

                    continue;
                }

                continue;
            }

            /* Neither comment form matched, so this is division. */
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

        // Inspect one extra character to distinguish assignment from equality.
        if (c == '=') {
            int next = read_char();

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

        // The base language supports != but does not support a standalone negation operator.
        if (c == '!') {
            int next = read_char();

            if (next != '=')
                return 1;

            token->type    = TOKEN_NEQ;
            line_has_token = true;

            return 0;
        }

        // Use the longer relational operator when the next character is an equals sign.
        if (c == '<' || c == '>') {
            int  next  = read_char();
            bool equal = next == '=';

            if (!equal && push_char(next) != 0)
                return 99;

            token->type =
                c == '<' ? (equal ? TOKEN_LEQ : TOKEN_LT) : (equal ? TOKEN_GEQ : TOKEN_GT);

            line_has_token = true;

            return 0;
        }

        // The remaining supported punctuation is decided from one character.
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
            // The outermost opening parenthesis anchors continuation indentation.
            if (paren_depth == 0)
                logical_line_indent = indent_stack[indent_top];

            if (paren_depth == SIZE_MAX) {
                return 99;
            }

            // Nested parentheses suppress logical EOL until the matching depth returns to zero.
            ++paren_depth;
            token->type = TOKEN_LPAREN;
            break;

        case ')':
            /*
             * Parenthesis matching is checked by the parser. Avoid underflow
             * here even if the input contains an unmatched closing parenthesis.
             */
            if (paren_depth > 0)
                --paren_depth;

            token->type = TOKEN_RPAREN;
            break;

        // A character not handled by any lexical category produces error code 1.
        default:
            return 1;
        }

        line_has_token = true;

        return 0;
    }
}
/**
 * Konec AI části: scan_next()
 */

/**
 * @brief Selects a caller-owned stream and resets the scanner.
 */
void scanner_init(FILE *source_file) {
    /* Internal allocations from a previous input must not be leaked. */
    scanner_free();

    // Borrow the new stream at its current position; initialization does not rewind it.
    source = source_file;
}

/**
 * @brief Returns one token and records skipped physical line breaks.
 *
 * @return 0 on success, 1 on lexical error, or 99 on internal/API/I/O failure.
 */
/**
 * Začátek AI části
 * Použitý model: GPT-6.1 Sol
 * Rozsah: Nová obsluha rozhraní, chyb čtení a informace newline_before.
 * Zkontroluj předávání tokenů ze scanneru parseru a návratové kódy při lexikální, interní
 * a vstupní chybě. Ověř, zda parser dostává dostatek informací pro kontrolu koncové čárky
 * před uzavírací závorkou na dalším řádku. Podle potřeby doplň informaci o přeskočeném
 * novém řádku a odděl samotné rozpoznávání tokenů od obsluhy veřejného rozhraní. Vysvětli
 * také vlastnictví paměti identifikátorů a řetězců.
 */
int get_next_token(Token *token) {
    // Invalid API arguments are internal errors, not source-language lexical errors.
    if (source == NULL || token == NULL) {
        return 99;
    }

    // Virtual layout tokens and EOF keep the default false value.
    token->newline_before = false;

    /* Lazy allocation allows allocation failures to use the usual error code. */
    if (indent_stack == NULL) {
        if (reserve_indents(1) != 0) {
            return 99;
        }

        // The root indentation level exists even when the source has no blocks.
        indent_stack[0] = 0;
    }

    // All source recognition happens internally; this wrapper finishes API metadata.
    int rc = scan_next(token);

    /* A failed stream read must not be mistaken for a normal EOF. */
    if (ferror(source)) {
        if (rc == 0 && (token->type == TOKEN_IDENTIFIER || token->type == TOKEN_STRING_LITERAL)) {
            // If I/O failed after creating an attribute, release the unreturned allocation.
            free(token->attribute.s_val);
            token->attribute.s_val = NULL;
        }

        return 99;
    }

    /*
     * Layout tokens do not consume the line-break information.
     * Preserve it until the next actual source lexeme is returned.
     */
    if (rc == 0 && token->type != TOKEN_EOL && token->type != TOKEN_INDENT &&
        token->type != TOKEN_DEDENT && token->type != TOKEN_EOF) {
        // The parser can now distinguish a same-line closing parenthesis after a comma.
        token->newline_before = skipped_newline;
        // Consume the metadata only for a real lexeme, never for INDENT, DEDENT, or EOL.
        skipped_newline = false;
    }

    return rc;
}
/**
 * Konec AI části: get_next_token()
 */

/**
 * @brief Frees internal buffers and resets all scanner state.
 *
 * Previously returned token attributes remain owned by the caller.
 */
void scanner_free(void) {
    // Release the shared lexeme storage; emitted string attributes are separate allocations.
    ds_free(&buffer);
    // Release the dynamically grown block-level storage independently of the stream.
    free(indent_stack);

    // Reset all counters and flags so repeated cleanup or reinitialization is safe.
    indent_stack        = NULL;
    indent_capacity     = 0;
    indent_top          = 0;
    pending_dedents     = 0;
    at_line_start       = true;
    line_has_token      = false;
    paren_depth         = 0;
    logical_line_indent = 0;
    continuation_line   = false;
    skipped_newline     = false;
    pushed_count        = 0;
    // Forget the borrowed stream without closing the caller-owned FILE.
    source = NULL;
}