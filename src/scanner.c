#include "scanner.h"
#include <stdlib.h>
#include <string.h>

static FILE *source = NULL;
// TODO: Deklarovat globální/statický zásobník pro sledování hloubky odsazení mezerami.
// TODO: Deklarovat proměnnou pro sledování hloubky vnořených blokových komentářů.

// TODO: Pomocná funkce pro přeskočení bílých znaků, počítání nových řádků a generování TOKEN_INDENT
// / TOKEN_DEDENT.
static int process_indentation(Token *token) { return 0; }

// TODO: Pomocná funkce pro stavový automat zpracovávající celá a desetinná čísla (včetně
// exponentů).
static int process_number(int first_char, Token *token) { return 0; }

// TODO: Pomocná funkce pro zpracování řetězcových literálů (včetně tří uvozovek pro víceřádkové
// texty a escape sekvencí).
static int process_string(Token *token) { return 0; }

// TODO: Pomocná funkce pro identifikátory a ověření, zda se nejedná o klíčové slovo (pomocí tabulky
// symbolů nebo binárního vyhledávání).
static int process_identifier_or_keyword(int first_char, Token *token) { return 0; }

void scanner_init(FILE *source_file) {
    source = source_file;
    // TODO: Inicializovat zásobník odsazení (vložit výchozí úroveň 0) a vynulovat stavové proměnné.
}

int get_next_token(Token *token) {
    if (!source || !token) {
        return 1;
    }

    // TODO: Implementovat hlavní cyklus (DKA). Zde bude základní rozcestník, který znaky předá do
    // příslušných `process_*` funkcí. Příklad: int c = fgetc(source); if (isalpha(c) || c == '_')
    // return process_identifier_or_keyword(c, token); else if (isdigit(c)) return process_number(c,
    // token);
    // ...

    return 0;
}

void scanner_free(void) {
    // TODO: Uvolnit paměť vyhrazenou pro zásobník odsazení a další statické struktury.
}