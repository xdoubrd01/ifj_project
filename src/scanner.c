/**
 * @author Denis Doubrava xdoubrd01
 * Použitý model: GPT-6.1 Sol
 * Tvorba komentářů byla za pomoci umělé inteligence: překlad, formátování a
 * lepší formulace vět.
 */

/**
 * @file scanner.c
 * @brief Stateful lexical analyzer for the base IFJ26 language.
 */

#include "array_utils.h"
#include "dynamic_buffer.h"
#include <errno.h>
#include <math.h>
#include "scanner.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "str_utils.h"
#include <string.h>
#include "token_utils.h"

/**
 * @brief Resizable buffer used while reading a lexeme.
 *
 * The buffer always contains a terminating zero after its contents.
 * Its allocated storage can be reused for subsequent tokens.
 */
typedef DynamicBuffer DynamicString;

/* The stream belongs to the caller; the scanner never closes it. */
static FILE         *source = NULL;
// AI ASSISTED START
// AI PROMPT: Podporuj pametovy vstup bez fmemopen a zachovej FILE vstup.
static const unsigned char *memory_source = NULL;
static size_t memory_length = 0;
static size_t memory_position = 0;
// AI ASSISTED END
static DynamicString buffer;

/* Only one scanner instance is active at a time. */
static size_t *indent_stack        = NULL;
static size_t  indent_capacity     = 0;
static size_t  indent_top          = 0;
static size_t  pending_dedents     = 0;
static bool    at_line_start       = true;
static bool    line_has_token      = false;
static size_t  paren_depth         = 0;
static size_t  logical_line_indent = 0;
static bool    continuation_line   = false;
static bool    skipped_newline     = false;

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
    if (pushed_count > 0) {
        return pushed_chars[--pushed_count];
    }

    if (memory_source != NULL) {
        if (memory_position >= memory_length) return EOF;
        return memory_source[memory_position++];
    }

    return fgetc(source);
}

/**
 * @brief Appends one character and preserves the terminating zero.
 *
 * @return 0 on success or 99 on allocation failure or size overflow.
 */
static int ds_append(DynamicString *s, char c) { return dynamic_buffer_append_char(s, c) ? 0 : 99; }

/**
 * @brief Copies the lexeme into a caller-owned token attribute.
 *
 * @return 0 on success or 99 if allocation fails.
 *
 * @note The parser must free this copy.
 * @note The returned attribute survives reuse of the shared scanner buffer.
 */
static int set_string_attribute(Token *token) {
    const char *src        = buffer.data != NULL ? buffer.data : "";
    token->attribute.s_val = string_duplicate_n(src, buffer.length);
    return token->attribute.s_val != NULL ? 0 : 99;
}

/**
 * @brief Stores lookahead locally; EOF requires no pushback.
 *
 * @return 0 on success or 99 if the internal pushback buffer is full.
 */
static int push_char(int c) {
    if (c == EOF) {
        return 0;
    }

    if (pushed_count == sizeof(pushed_chars) / sizeof(pushed_chars[0])) {
        return 99;
    }

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
 * Otestuj zpracování odsazení, zejména více vnořených bloků, návraty na
 * předchozí úrovně, prázdné řádky, komentáře a pokračování uvnitř závorek.
 * Prověř omezení pevně velkého zásobníku odsazení. Pokud omezuje zpracování
 * platného vstupu, převeď jej na dynamicky zvětšovaný zásobník. Ošetři selhání
 * alokace, přetečení velikostí a uvolnění paměti.
 */
static int reserve_indents(size_t required) {
    if (required <= indent_capacity) {
        return 0;
    }

    size_t capacity;
    if (!array_next_capacity(indent_capacity, required, sizeof(*indent_stack), 16, &capacity)) {
        return 99;
    }

    size_t *stack = realloc(indent_stack, capacity * sizeof(*stack));

    if (stack == NULL) {
        return 99;
    }

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
        pending_dedents = indent_top - 1;
        indent_top      = 0;
        token->type     = TOKEN_DEDENT;
    } else {
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

/**
 * @brief Removes digit separators and converts an Int literal.
 *
 * @return 0 on success or 99 on temporary-buffer allocation failure.
 *
 * @note Integer overflow is not checked, as permitted by the assignment.
 */
static int convert_int(const char *text, int *out) {
    DynamicString tmp;
    dynamic_buffer_init(&tmp);

    /* strtol() does not accept the source-language underscore separators. */
    for (size_t i = 0; text[i] != '\0'; ++i) {
        if (text[i] != '_') {
            int rc = ds_append(&tmp, text[i]);

            if (rc != 0) {
                dynamic_buffer_free(&tmp);
                return rc;
            }
        }
    }

    *out = (int)strtol(tmp.data != NULL ? tmp.data : "0", NULL, 10);

    dynamic_buffer_free(&tmp);

    return 0;
}

/**
 * @brief Removes separators and suffix before converting a Double literal.
 *
 * @return 0 on success, 1 on a rejected conversion, or 99 on allocation
 * failure.
 *
 * @note Representable nonzero subnormal values are accepted.
 * @note Conversion assumes the standard C numeric locale.
 */
static int convert_double(const char *text, double *out) {
    DynamicString tmp;
    dynamic_buffer_init(&tmp);

    for (size_t i = 0; text[i] != '\0'; ++i) {
        char c = text[i];

        if (c == '_') {
            continue;
        }

        /* The optional source-language suffix is not passed to strtod(). */
        if ((c == 'd' || c == 'D') && text[i + 1] == '\0') {
            continue;
        }

        int rc = ds_append(&tmp, c);

        if (rc != 0) {
            dynamic_buffer_free(&tmp);
            return rc;
        }
    }

    errno        = 0;
    char  *end   = NULL;
    double value = strtod(tmp.data != NULL ? tmp.data : "0", &end);

    /* Reject overflow and complete underflow of a nonzero literal to zero. */
    if (!isfinite(value) || end == NULL || *end != '\0' || (errno == ERANGE && value == 0.0)) {
        dynamic_buffer_free(&tmp);
        return 1;
    }

    *out = value;

    dynamic_buffer_free(&tmp);

    return 0;
}

/**
 * @brief Consumes a block comment after its opening delimiter was read.
 *
 * @param had_newline Receives true if the comment contained LF.
 * @return 0 on success, 1 for an unterminated comment, or 99 on counter
 * overflow.
 */
static int consume_block_comment(bool *had_newline) {
    size_t depth = 1;
    *had_newline = false;

    while (depth > 0) {
        int c = read_char();

        if (c == EOF) {
            return 1;
        }

        if (c == '\n') {
            skipped_newline = true;
            *had_newline    = true;
            continue;
        }

        if (c == '/') {
            int next = read_char();

            if (next == '*') {
                /* Every nested opening requires its own closing delimiter. */
                if (depth == SIZE_MAX) {
                    return 99;
                }

                ++depth;
            } else {
                int rc = push_char(next);

                if (rc != 0)
                    return rc;
            }
        } else if (c == '*') {
            int next = read_char();

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

/**
 * @brief Finishes a block comment that contained a physical newline.
 *
 * @return 0 if EOL was produced, -1 to continue scanning, or 1 on lexical
 * error.
 *
 * @note Only whitespace may follow the closing delimiter before LF or EOF.
 */
static int finish_multiline_comment(Token *token) {
    int c;

    do {
        c = read_char();
    } while (c == ' ' || c == '\t' || c == '\r' || c == '\f' || c == '\v');

    if (c != '\n' && c != EOF) {
        return 1;
    }

    at_line_start = true;

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
 * Otestuj zpracování odsazení, zejména více vnořených bloků, návraty na
 * předchozí úrovně, prázdné řádky, komentáře a pokračování uvnitř závorek.
 * Prověř omezení pevně velkého zásobníku odsazení. Pokud omezuje zpracování
 * platného vstupu, převeď jej na dynamicky zvětšovaný zásobník. Ošetři selhání
 * alokace, přetečení velikostí a uvolnění paměti.
 */
static int handle_line_start(Token *token) {
    size_t spaces                    = 0;
    bool   indentation_locked        = false;
    bool   invalid_indent_whitespace = false;

    while (at_line_start) {
        int c;

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

        if (c == EOF) {
            at_line_start     = false;
            continuation_line = false;

            return emit_eof(token);
        }

        /* Comment-only lines must not change indentation. */
        if (c == '/') {
            int next = read_char();

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
                bool had_newline = false;
                int  rc          = consume_block_comment(&had_newline);

                if (rc != 0) {
                    return rc;
                }

                if (had_newline) {
                    rc = finish_multiline_comment(token);

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
            /* Allocate another level instead of imposing a fixed depth limit. */
            if (indent_top > SIZE_MAX - 2 || reserve_indents(indent_top + 2) != 0) {
                return 99;
            }

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
    dynamic_buffer_clear(&buffer);

    if (ds_append(&buffer, (char)first) != 0)
        return 99;

    int c;

    while ((c = read_char()) != EOF && is_name_char(c)) {
        if (ds_append(&buffer, (char)c) != 0)
            return 99;
    }

    if (push_char(c) != 0)
        return 99;

    if (buffer.length == 1 && buffer.data[0] == '_') {
        return 1;
    }

    token->type = keyword_or_identifier(buffer.data);

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

        if (c == '_') {
            /* Multiple separators are valid, but a digit must follow the run. */
            do {
                if (ds_append(&buffer, '_') != 0)
                    return 99;

                c = read_char();
            } while (c == '_');

            if (!is_digit(c))
                return 1;

            if (ds_append(&buffer, (char)c) != 0)
                return 99;

            continue;
        }

        *terminator = c;

        return 0;
    }
}

/**
 * @brief Rejects an invalid numeric suffix and preserves the next delimiter.
 */
static int finish_number_delimiter(int c) {
    if (is_letter(c) || c == '_') {
        return 1;
    }

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
 * Porovnej funkci read_number() s mým návrhem DKA, specifikací jazyka a
 * poznatky z fóra. Otestuj podtržítka mezi číslicemi, exponenty, přípony a
 * úvodní nuly. Zápisy jako 01.5 musí být odmítnuty, zatímco 0.5, .5 a 2e001
 * mají být přijaty. Nalezené chyby oprav a vysvětli změny v implementaci.
 */
static int read_number(Token *token, int first) {
    dynamic_buffer_clear(&buffer);

    bool has_fraction = false;
    bool has_exponent = false;
    bool has_suffix   = false;
    int  term         = EOF;

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
        if (first == '0') {
            if (ds_append(&buffer, '0') != 0) {
                return 99;
            }

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

        if (term == '.') {
            int c = read_char();
            if (is_letter(c) || c == '_') {
                /* VALMEM: keep the member dot and the first name character. */
                if (push_char(c) != 0) return 99;
            } else {
                has_fraction = true;
                if (ds_append(&buffer, '.') != 0) return 99;
                if (!is_digit(c)) return 1;
                int rc = append_digit_sequence(c, &term);
                if (rc != 0) return rc;
            }
        }
    }

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

    if (has_suffix && is_name_char(term)) {
        return 1;
    }

    int rc = finish_number_delimiter(term);

    if (rc != 0)
        return rc;

    if (has_fraction || has_exponent) {
        token->type = TOKEN_DOUBLE_LITERAL;

        return convert_double(buffer.data, &token->attribute.d_val);
    }

    /* The leading-zero restriction applies to nonzero Int literals only. */
    if (buffer.data[0] == '0') {
        for (size_t i = 1; i < buffer.length; ++i) {
            if (buffer.data[i] >= '1' && buffer.data[i] <= '9') {
                return 1;
            }
        }
    }

    token->type = TOKEN_INT_LITERAL;

    return convert_int(buffer.data, &token->attribute.i_val);
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
    dynamic_buffer_clear(&buffer);

    while (true) {
        int c = read_char();

        if (c == EOF || c == '\n' || c == '\r') {
            return 1;
        }

        if (c == '"') {
            token->type = TOKEN_STRING_LITERAL;

            return set_string_attribute(token);
        }

        /* Only the escapes explicitly listed in the assignment are accepted. */
        if (c == '\\') {
            int esc = read_char();

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
 * Zkontroluj zpracování uvozovek ve víceřádkových řetězcích podle specifikace a
 * upřesnění z fóra. Otestuj především vstupy tvořené sedmi a osmi uvozovkami za
 * sebou, jednotlivé uvozovky uvnitř řetězce a neukončené řetězce. Oprav funkci
 * tak, aby správně rozlišovala obsah řetězce a jeho ukončovací uvozovky.
 */
static int read_multiline_string(Token *token) {
    dynamic_buffer_clear(&buffer);

    while (true) {
        int c = read_char();

        if (c == '"') {
            size_t quotes = 0;

            /* Read the complete consecutive run before deciding its contents. */
            do {
                if (ds_append(&buffer, '"') != 0) {
                    return 99;
                }

                ++quotes;
                c = read_char();
            } while (c == '"');

            if (quotes >= 3) {
                /* Remove only the final three delimiter quotes. */
                buffer.length -= 3;
                buffer.data[buffer.length] = '\0';

                if (push_char(c) != 0) {
                    return 99;
                }

                token->type = TOKEN_STRING_LITERAL;

                return set_string_attribute(token);
            }

            /* One or two quotes were already appended as ordinary contents. */
        }

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
    int second = read_char();

    if (second != '"') {
        if (push_char(second) != 0)
            return 99;

        return read_normal_string(token);
    }

    int third = read_char();

    if (third == '"') {
        return read_multiline_string(token);
    }

    /* Two quotes form an empty ordinary string. */
    if (push_char(third) != 0)
        return 99;

    dynamic_buffer_clear(&buffer);
    token->type = TOKEN_STRING_LITERAL;

    return set_string_attribute(token);
}

/**
 * @brief Recognizes the indivisible @main annotation.
 *
 * Rejects whitespace after @ and identifier continuations such as @mainName.
 */
static int read_at_main(Token *token) {
    static const char expected[] = "main";

    for (size_t i = 0; i < sizeof(expected) - 1; ++i) {
        if (read_char() != expected[i]) {
            return 1;
        }
    }

    int next = read_char();

    if (is_name_char(next)) {
        return 1;
    }

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
 * Zkontroluj předávání tokenů ze scanneru parseru a návratové kódy při
 * lexikální, interní a vstupní chybě. Ověř, zda parser dostává dostatek
 * informací pro kontrolu koncové čárky před uzavírací závorkou na dalším řádku.
 * Podle potřeby doplň informaci o přeskočeném novém řádku a odděl samotné
 * rozpoznávání tokenů od obsluhy veřejného rozhraní. Vysvětli také vlastnictví
 * paměti identifikátorů a řetězců.
 */
static int scan_next(Token *token) {
    if ((source == NULL && memory_source == NULL) || token == NULL) {
        return 99;
    }

    /* Deliver queued block endings before reading further input. */
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

        int c = read_char();

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

        if (c == '\n') {
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

        if (is_letter(c) || c == '_') {
            int rc = read_identifier(token, c);

            if (rc == 0)
                line_has_token = true;

            return rc;
        }

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

        if (c == '"') {
            int rc = read_string(token);

            if (rc == 0)
                line_has_token = true;

            return rc;
        }

        if (c == '/') {
            int next = read_char();

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

        if (c == '!') {
            int next = read_char();

            if (next != '=')
                return 1;

            token->type    = TOKEN_NEQ;
            line_has_token = true;

            return 0;
        }

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

            if (paren_depth == SIZE_MAX) {
                return 99;
            }

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

    source = source_file;
}

// AI ASSISTED START
// AI PROMPT: Inicializuj scanner nad polem bajtu pro opakovane pruchody.
void scanner_init_buffer(const char *input, size_t length) {
    scanner_free();
    memory_source = (const unsigned char *)input;
    memory_length = length;
    memory_position = 0;
}
// AI ASSISTED END

/**
 * @brief Returns one token and records skipped physical line breaks.
 *
 * @return 0 on success, 1 on lexical error, or 99 on internal/API/I/O failure.
 */
/**
 * Začátek AI části
 * Použitý model: GPT-6.1 Sol
 * Rozsah: Nová obsluha rozhraní, chyb čtení a informace newline_before.
 * Zkontroluj předávání tokenů ze scanneru parseru a návratové kódy při
 * lexikální, interní a vstupní chybě. Ověř, zda parser dostává dostatek
 * informací pro kontrolu koncové čárky před uzavírací závorkou na dalším řádku.
 * Podle potřeby doplň informaci o přeskočeném novém řádku a odděl samotné
 * rozpoznávání tokenů od obsluhy veřejného rozhraní. Vysvětli také vlastnictví
 * paměti identifikátorů a řetězců.
 */
int get_next_token(Token *token) {
    if ((source == NULL && memory_source == NULL) || token == NULL) {
        return 99;
    }

    token->newline_before = false;

    /* Lazy allocation allows allocation failures to use the usual error code. */
    if (indent_stack == NULL) {
        if (reserve_indents(1) != 0) {
            return 99;
        }

        indent_stack[0] = 0;
    }

    int rc = scan_next(token);

    /* A failed stream read must not be mistaken for a normal EOF. */
    if (source != NULL && ferror(source)) {
        if (rc == 0 && (token->type == TOKEN_IDENTIFIER || token->type == TOKEN_STRING_LITERAL)) {
            token_destroy(token);
        }

        return 99;
    }

    /*
     * Layout tokens do not consume the line-break information.
     * Preserve it until the next actual source lexeme is returned.
     */
    if (rc == 0 && token->type != TOKEN_EOL && token->type != TOKEN_INDENT &&
        token->type != TOKEN_DEDENT && token->type != TOKEN_EOF) {
        token->newline_before = skipped_newline;
        skipped_newline       = false;
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
    dynamic_buffer_free(&buffer);
    free(indent_stack);

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
    source              = NULL;
    memory_source       = NULL;
    memory_length       = 0;
    memory_position     = 0;
}