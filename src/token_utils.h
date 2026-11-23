#ifndef TOKEN_UTILS_H
#define TOKEN_UTILS_H

#include "scanner.h"

/* Releases an owned string attribute and resets the token to EOF. */
void token_destroy(Token *token);

#endif