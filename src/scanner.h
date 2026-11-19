/**
 *@author Denis Doubrava xdoubrd01
 * Použitý model: GPT-6.1 Sol
 * Tvorba komentářů byla za pomoci umělé inteligence: překlad, formátování a lepší formulace vět
 */

/**
 * @file scanner.h
 * @brief Token types and a stateful scanner interface for base IFJ26.
 *
 * The caller owns the input stream and all successfully returned identifier
 * and string attributes. Only one scanner instance is active at a time.
 */

#ifndef SCANNER_H
#define SCANNER_H

#include <stdbool.h>
#include <stdio.h>

/**
 * @brief Tokens recognized by this scanner.
 *
 * @note Unit literals are represented by LPAREN followed by RPAREN.
 * @note Physical line breaks inside parentheses do not produce EOL tokens.
 */

/**
 * Začátek AI části
 * Použitý model: GPT-6.1 Sol
 * Prompt: Ze zadaných fotek DKA mi vytvoř enum na seznam tokenů, ke každému tokenu přidej komentář
 * popisující token a rozděl je do smysluplných skupin
 */

typedef enum {
    TOKEN_EOF, // End of input
    TOKEN_EOL, // End of a nonempty logical line

    // Identifiers and literals
    TOKEN_IDENTIFIER,
    TOKEN_INT_LITERAL,
    TOKEN_DOUBLE_LITERAL,
    TOKEN_STRING_LITERAL,

    // Base-language keywords
    TOKEN_KW_DEF,
    TOKEN_KW_DO,
    TOKEN_KW_ELSE,
    TOKEN_KW_IF,
    TOKEN_KW_RETURN,
    TOKEN_KW_THEN,
    TOKEN_KW_VAL,
    TOKEN_KW_VAR,
    TOKEN_KW_WHILE,
    TOKEN_KW_IMPORT,

    // Base-language data types
    TOKEN_TYPE_INT,
    TOKEN_TYPE_DOUBLE,
    TOKEN_TYPE_STRING,
    TOKEN_TYPE_UNIT,

    // Operators and separators
    TOKEN_ASSIGN,  // =
    TOKEN_PLUS,    // +
    TOKEN_MINUS,   // -
    TOKEN_MUL,     // *
    TOKEN_DIV,     // /
    TOKEN_EQ,      // ==
    TOKEN_NEQ,     // !=
    TOKEN_LT,      // <
    TOKEN_LEQ,     // <=
    TOKEN_GT,      // >
    TOKEN_GEQ,     // >=
    TOKEN_LPAREN,  // (
    TOKEN_RPAREN,  // )
    TOKEN_COLON,   // :
    TOKEN_COMMA,   // ,

    // Additional punctuation and the entry-point annotation
    TOKEN_DOT,       // .
    TOKEN_SEMICOLON, // ;
    TOKEN_AT_MAIN,   // @main

    // Block boundaries derived from indentation
    TOKEN_INDENT,
    TOKEN_DEDENT
} TokenType;

/**
 * @brief Token and its type-dependent attribute.
 *
 * @note Read i_val only for TOKEN_INT_LITERAL.
 * @note Read d_val only for TOKEN_DOUBLE_LITERAL.
 * @note TOKEN_IDENTIFIER and TOKEN_STRING_LITERAL own an allocated s_val.
 *       The caller must free it before discarding or overwriting the token.
 * @note Other token types have no defined attribute.
 */
typedef struct {
    TokenType type;

    /**
     * True if skipped whitespace or comments before this lexeme contained LF.
     *
     * Layout tokens and EOF always use false. Newlines inside a string literal
     * do not set this flag. The parser uses it to validate a trailing comma.
     */
    bool newline_before;

    union {
        int    i_val;
        double d_val;
        char  *s_val;
    } attribute;
} Token;

/**
 * @brief Resets scanner state for a new input stream.
 *
 * @param source_file Caller-owned stream from which tokens will be read.
 *
 * @note Reinitialization also releases previous internal scanner storage.
 * @note Does not close the input stream or free previously returned attributes.
 */
void scanner_init(FILE *source_file);

/**
 * @brief Reads the next token, including indentation markers when needed.
 *
 * @param token Destination for the next token; must not be NULL.
 * @return 0 on success, 1 on lexical error, or 99 on internal/API/I/O failure.
 *
 * @note Call scanner_init() with a valid stream before requesting tokens.
 * @note After an error, call scanner_free() or scanner_init() before continuing.
 * @note Attributes are valid only on success.
 * @note Previous token attributes are not automatically freed.
 */
int get_next_token(Token *token);

/**
 * @brief Releases the scanner's lexeme buffer and indentation stack.
 *
 * @note Does not free returned token attributes or close the input stream.
 * @note May be called after successful scanning or after an error.
 * @note Repeated calls are safe.
 */
void scanner_free(void);

#endif // SCANNER_H