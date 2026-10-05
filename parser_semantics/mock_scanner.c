#include "mock_scanner.h" 

// Mock scanner implementation for testing the precedence parser
Token mock_tokens[] = {
    { .type = TOKEN_INT_LITERAL, .attribute.i_val = 5 },
    { .type = TOKEN_PLUS },
    { .type = TOKEN_IDENTIFIER, .attribute.s_val = "x" },
    { .type = TOKEN_EOF }
};
int current_mock_index = 0;

// Function to get the next token from the mock scanner
int get_next_token(Token *token) {
    *token = mock_tokens[current_mock_index++];
    return 0; // 0 indicates success
}