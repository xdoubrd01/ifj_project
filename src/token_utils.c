#include <stdlib.h>
#include "token_utils.h"

void token_destroy(Token *token) {
    if (token == NULL) {
        return;
    }

    if (token->type == TOKEN_IDENTIFIER || token->type == TOKEN_STRING_LITERAL) {
        free(token->attribute.s_val);
    }

    *token = (Token){.type = TOKEN_EOF};
}