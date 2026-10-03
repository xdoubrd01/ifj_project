#include scanner.h

int current_mock_index = 0; // Index of the current token in the mock token array
Token mock_tokens[] = {
    {TOKEN_ID, "a"}, 
    {TOKEN_PLUS, "+"}, 
    {TOKEN_INT_LITERAL, "5"}, 
    {TOKEN_EOF, ""}
};

Token get_next_token() {
    return mock_tokens[current_mock_index++]; }