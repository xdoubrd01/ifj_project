#ifndef SCANNER_H
#define SCANNER_H

#include <stdbool.h>
#include <stdio.h>

// Enumeration of all possible token types for the IFJ26 language.
// TODO (Colleague 2 - Parser): You will use this enumeration in your recursive descent
// for expected syntax checking (e.g., if (token.type == TOKEN_KW_DEF) ...).
typedef enum {
    TOKEN_EOF, // End of file (Parser stops analysis here)
    TOKEN_EOL, // End of logical line (statement separator, if not a semicolon)

    // Identifiers and literals
    TOKEN_IDENTIFIER,     // e.g., variable_name, function_name
    TOKEN_INT_LITERAL,    // Integer literal (e.g., 42)
    TOKEN_DOUBLE_LITERAL, // Decimal literal (e.g., 3.14, 1e-5)
    TOKEN_STRING_LITERAL, // String literal (incl. multiline and escape sequences)

    // IFJ26 language keywords
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

    // Data types
    TOKEN_TYPE_INT,
    TOKEN_TYPE_DOUBLE,
    TOKEN_TYPE_STRING,
    TOKEN_TYPE_UNIT,

    // Operators and separators
    TOKEN_ASSIGN, // =
    TOKEN_PLUS,   // +
    TOKEN_MINUS,  // -
    TOKEN_MUL,    // *
    TOKEN_DIV,    // /
    TOKEN_EQ,     // ==
    TOKEN_NEQ,    // !=
    TOKEN_LT,     // <
    TOKEN_LTE,    // <=
    TOKEN_GT,     // >
    TOKEN_GTE,    // >=
    TOKEN_LPAREN, // (
    TOKEN_RPAREN, // )
    TOKEN_COLON,  // :
    TOKEN_COMMA,  // ,

    // Special tokens for block control using space indentation.
    // TODO (Colleague 1 - Lexer): You must generate these tokens automatically when the number
    // of spaces changes at the beginning of a new logical line, because IFJ26 does not use curly braces.
    TOKEN_INDENT, // Increase indentation level (block open)
    TOKEN_DEDENT  // Decrease indentation level (block close)
} TokenType;

// Structure representing a lexical unit (token).
// TODO (Colleague 1 - Lexer): Ensure you correctly fill the `attribute` when returning a token
// if it's a literal or identifier. Do not fill the attribute for operators and keywords.
typedef struct {
    TokenType type; // Token type

    // Token attribute (additional value carrying semantic information)
    union {
        int    i_val; // Fill for TOKEN_INT_LITERAL
        double d_val; // Fill for TOKEN_DOUBLE_LITERAL
        char  *s_val; // Fill for TOKEN_STRING_LITERAL and TOKEN_IDENTIFIER (must be dynamically
                      // allocated!)
    } attribute;
} Token;

/**
 * @brief Initializes the lexical analyzer.
 * @param source_file Pointer to the open IFJ26 source code file.
 */
void scanner_init(FILE *source_file);

/**
 * @brief Main communication function for the Parser (Colleague 2).
 * Reads the next lexeme from the input and fills the provided Token structure with it.
 *
 * TODO (Colleague 2 - Parser): You will call this function in a loop or at the beginning of
 * every recursive descent function to determine the next element.
 *
 * @param token Pointer to the structure where the result will be written.
 * @return Value 0 on success, error code 1 on lexical error (e.g., unknown character).
 */
int get_next_token(Token *token);

/**
 * @brief Frees all dynamically allocated memory of the scanner (e.g., indentation stack).
 */
void scanner_free(void);

#endif // SCANNER_H