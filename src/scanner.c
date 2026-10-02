#include "scanner.h"
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Definice stavů konečného automatu (DKA)
typedef enum {
    STATE_START,
    STATE_IDENTIFIER,
    STATE_NUMBER,
    STATE_STRING,
    STATE_ASSIGN,
    STATE_SLASH,
    STATE_LINE_COMMENT,
    STATE_BLOCK_COMMENT
} ScannerState;

// Základní struktura pro dynamický řetězec (buffer pro ukládání textu lexémů)
typedef struct {
    char  *text;
    size_t length;
    size_t capacity;
} dynamic_string;

static dynamic_string buffer;
static FILE          *source = NULL;

// Pomocné funkce pro dynamický buffer (implementaci doplníme pro řetězce/čísla)
void dstring_init(dynamic_string *b) {
    b->text = NULL;
    b->length = 0;
    b->capacity = 0;
}

void dstring_clear(dynamic_string *b) {
    b->length = 0;
    if (b->text != NULL) {
        b->text[0] = '\0';
    }
}

void dstring_append(dynamic_string *b, char c) {
    (void)b;
    (void)c;
    // TODO: Dynamická realokace paměti a připojení znaku
}

void dstring_free(dynamic_string *b) {
    if (b->text != NULL) {
        free(b->text);
        b->text = NULL;
    }
    b->length = 0;
    b->capacity = 0;
}

void scanner_init(FILE *source_file) {
    source = source_file;
    dstring_init(&buffer);
    // TODO: Inicializace zásobníku pro sledování úrovní odsazení (indentace)
}

int get_next_token(Token *token) {
    if (!source || !token) {
        return 1; // Interní chyba
    }

    int  state = STATE_START;
    int  comment_depth = 0;
    bool contains_newline = false;

    dstring_clear(&buffer);

    while (true) {
        int c = fgetc(source);

        switch (state) {
            case STATE_START:
                if (c == EOF) {
                    token->type = TOKEN_EOF;
                    return 0;
                }
                if (c == ' ') {
                    continue; // Běžné mezery uvnitř řádku přeskakujeme
                }
                if (c == '\n') {
                    token->type = TOKEN_EOL;
                    return 0;
                }

                // Identifikátory a klíčová slova
                if (isalpha(c) || c == '_') {
                    dstring_append(&buffer, (char)c);
                    state = STATE_IDENTIFIER;
                    break;
                }

                // Celočíselné a desetinné literály
                if (isdigit(c)) {
                    dstring_append(&buffer, (char)c);
                    state = STATE_NUMBER;
                    break;
                }

                // Složené operátory a komentáře
                if (c == '=') {
                    state = STATE_ASSIGN;
                    break;
                }
                if (c == '/') {
                    state = STATE_SLASH;
                    break;
                }

                // Jednoznakové operátory
                if (c == '+') {
                    token->type = TOKEN_PLUS;
                    return 0;
                }
                if (c == '-') {
                    token->type = TOKEN_MINUS;
                    return 0;
                }

                return 1; // Neznámý znak -> lexikální chyba 1 podle specifikace

            case STATE_ASSIGN:
                // Předchozí znak byl '='
                if (c == '=') {
                    token->type = TOKEN_EQ; // '=='
                } else {
                    ungetc(c, source);      // Vracíme nepoužitý znak
                    token->type = TOKEN_ASSIGN; // Pouze '='
                }
                return 0;

            case STATE_SLASH:
                // Předchozí znak byl '/'
                if (c == '/') {
                    state = STATE_LINE_COMMENT; // Řádkový komentář '//'
                    break;
                } else if (c == '*') {
                    state = STATE_BLOCK_COMMENT; // Blokový komentář '/*'
                    comment_depth = 1;
                    contains_newline = false;
                    break;
                } else {
                    ungetc(c, source);
                    token->type = TOKEN_DIV; // Obyčejné dělení '/'
                    return 0;
                }

            case STATE_LINE_COMMENT:
                // Řádkový komentář končí koncem řádku (LF) nebo EOF
                if (c == '\n') {
                    token->type = TOKEN_EOL; // Komentář se chová jako konec řádku
                    return 0;
                } else if (c == EOF) {
                    token->type = TOKEN_EOF;
                    return 0;
                }
                break; // Všechny ostatní znaky v komentáři zahazujeme

            case STATE_BLOCK_COMMENT:
                if (c == EOF) {
                    return 1; // Neukončený komentář -> lexikální chyba 1
                } else if (c == '\n') {
                    contains_newline = true; // Zaznamenáme nový řádek uvnitř komentáře
                } else if (c == '*') {
                    int next_c = fgetc(source);
                    if (next_c == '/') {
                        comment_depth--;
                        if (comment_depth == 0) {
                            // Komentář zcela uzavřen
                            if (contains_newline) {
                                token->type = TOKEN_EOL;
                                return 0;
                            } else {
                                state = STATE_START; // Funguje jako jeden bílý znak
                                break;
                            }
                        }
                    } else {
                        ungetc(next_c, source);
                    }
                } else if (c == '/') {
                    int next_c = fgetc(source);
                    if (next_c == '*') {
                        comment_depth++; // Vnořený komentář '/*'
                    } else {
                        ungetc(next_c, source);
                    }
                }
                break;

            case STATE_IDENTIFIER:
                // TODO: Přechody pro akumulaci zbytku identifikátoru a test na klíčová slova
                break;

            case STATE_NUMBER:
                // TODO: Přechody pro celá čísla, tečku a exponenty
                break;

            case STATE_STRING:
                // TODO: Zpracování řetězcových literálů
                break;
        }
    }
}

void scanner_free(void) {
    dstring_free(&buffer);
    // TODO: Dealokace paměti pro zásobník odsazení
}