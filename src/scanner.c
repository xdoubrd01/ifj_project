/**
 * @project Implementace překladače imperativního jazyka IFJ26
 * @file scanner.c
 * @author Denis Doubrava (xdoubrd01)
 * @brief Lexikální analýza vstupu, rozpoznávání tokenů a zpracování odsazení.
 * Čte znaky z paměťového bufferu nebo vstupního proudu a vrací tokeny
 * volajícímu analyzátoru. Zpracovává komentáře, řetězce a logické řádky
 * a používá dynamický zásobník úrovní odsazení.
 * @note AI byla využita k lepšímu znění a obsahu komentářů.
 */

#include "scanner.h"
#include "array_utils.h"
#include "dynamic_buffer.h"
#include "str_utils.h"
#include "token_utils.h"
#include <errno.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef DynamicBuffer DynamicString;

// Vstupní proud vlastní volající; scanner jej nikdy nezavírá.
static FILE *source = NULL;
// ZAČÁTEK ČÁSTI S ASISTENCÍ AI
// Zadání AI: Podporuj pametovy vstup bez fmemopen a zachovej FILE vstup.
static const unsigned char *memory_source   = NULL; ///< Nevlastněný ukazatel na paměťový vstup.
static size_t               memory_length   = 0;    ///< Přesný počet dostupných vstupních bajtů.
static size_t               memory_position = 0;    ///< Pozice příštího bajtu při paměťovém čtení.
// KONEC ČÁSTI S ASISTENCÍ AI
static DynamicString buffer; ///< Opakovaně používaný pracovní buffer aktuálního lexému.

// Modul udržuje jediný aktivní stav scanneru; není určen pro souběžné vstupy.
static size_t *indent_stack    = NULL;  ///< Dynamické pole úrovní odsazení se základní hodnotou 0.
static size_t  indent_capacity = 0;     ///< Kapacita pole úrovní.
static size_t  indent_top      = 0;     ///< Index aktuálního vrcholu zásobníku, nikoli počet prvků.
static size_t  pending_dedents = 0;     ///< Počet naplánovaných DEDENTů dosud nepředaných parseru.
static bool    at_line_start   = true;  ///< Zda má další krok zpracovat začátek fyzického řádku.
static bool    line_has_token  = false; ///< Zda aktuální logický řádek obsahuje skutečný token.
static size_t  paren_depth     = 0;     ///< Počet dosud otevřených kulatých závorek.
static size_t  logical_line_indent =
    0; ///< Odsazení výchozího logického řádku pro kontrolu pokračování.
static bool continuation_line = false; ///< Příznak fyzického pokračování uvnitř závorek.
static bool skipped_newline   = false; ///< LF přeskočené před příštím skutečným lexémem.

// Dvě pozice pro vrácené znaky nahrazují závislost na opakovaném ungetc().
static int    pushed_chars[2];
static size_t pushed_count = 0; ///< Počet aktuálně vrácených znaků.

/**
 * @brief Ověří ASCII číslici nezávisle na nastavení locale.
 * @param c Načtený bajt nebo EOF.
 * @return True pouze pro znaky 0 až 9.
 */
static bool is_digit(int c) { return c >= '0' && c <= '9'; }

/**
 * @brief Ověří ASCII písmeno nezávisle na nastavení locale.
 * @param c Načtený bajt nebo EOF.
 * @return True pouze pro A až Z a a až z.
 */
static bool is_letter(int c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }

/**
 * @brief Ověří znak přípustný po prvním znaku identifikátoru.
 * @param c Načtený bajt nebo EOF.
 * @return True pro ASCII písmeno, číslici nebo podtržítko.
 */
static bool is_name_char(int c) { return is_letter(c) || is_digit(c) || c == '_'; }

/**
 * @brief Načte znak nejprve z vrácených znaků a poté ze zvoleného vstupu.
 *
 * Vlastní dvouprvkový zásobník má přednost. Paměťový vstup čte podle pozice a délky, jinak
 * použije fgetc() nad proudem volajícího.
 * @return Další bajt jako int, nebo EOF při vyčerpání vstupu.
 * @note Chybu proudového čtení dodatečně rozliší get_next_token().
 */
static int read_char(void) {
    // Nejprve zpracujeme znaky načtené předčasně při rozpoznávání předchozího tokenu.
    if (pushed_count > 0) {
        return pushed_chars[--pushed_count];
    }

    // Paměťový vstup nepotřebuje terminátor; jeho konec určuje počet dostupných bajtů.
    if (memory_source != NULL) {
        if (memory_position >= memory_length)
            return EOF;
        return memory_source[memory_position++];
    }

    // Pokud není nastaven paměťový vstup, čteme z proudu vlastněného volajícím.
    return fgetc(source);
}

/**
 * @brief Připojí znak do dynamického řetězce a zachová jeho terminátor.
 * @param s Inicializovaný dynamický buffer.
 * @param c Připojovaný znak.
 * @return 0 při úspěchu, jinak 99 při selhání rezervace nebo přetečení velikosti.
 */
static int ds_append(DynamicString *s, char c) { return dynamic_buffer_append_char(s, c) ? 0 : 99; }

/**
 * @brief Zkopíruje obsah lexému do vlastněného atributu tokenu.
 *
 * Kopie přežije další použití pracovního bufferu scanneru. Funkce nepřepisuje typ tokenu a
 * neuvolňuje případný předchozí atribut.
 * @param token Token s již nastaveným typem, jehož s_val se má vyplnit.
 * @return 0 při úspěchu, jinak 99 při selhání alokace.
 * @note Vrácenou kopii musí volající později uvolnit, například pomocí token_destroy().
 */
static int set_string_attribute(Token *token) {
    const char *src        = buffer.data != NULL ? buffer.data : "";
    token->attribute.s_val = string_duplicate_n(src, buffer.length);
    return token->attribute.s_val != NULL ? 0 : 99;
}

/**
 * @brief Vrátí znak do interního zásobníku pro příští čtení.
 *
 * Znaky se vybírají v opačném pořadí vložení. EOF se neukládá, protože neoznačuje skutečný
 * bajt vstupu.
 * @param c Znak určený k opětovnému zpracování, případně EOF.
 * @return 0 při úspěchu nebo EOF, jinak 99 při zaplnění zásobníku.
 */
static int push_char(int c) {
    // EOF není skutečný znak a do zásobníku vrácených bajtů nepatří.
    if (c == EOF) {
        return 0;
    }

    if (pushed_count == sizeof(pushed_chars) / sizeof(pushed_chars[0])) {
        return 99;
    }

    // Vracení více znaků funguje jako zásobník: poslední vložený se načte jako první.
    pushed_chars[pushed_count++] = c;

    return 0;
}

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
/**
 * @brief Zajistí kapacitu pro požadovaný počet úrovní odsazení.
 *
 * Kapacita roste pomocí array_next_capacity(); hodnoty uložených úrovní zůstávají zachovány.
 * @param required Požadovaný počet prvků zásobníku včetně základní úrovně.
 * @return 0 při úspěchu, jinak 99 při přetečení nebo selhání alokace.
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
 * @brief Před konečným EOF vrátí zbývající ukončení bloků.
 *
 * Pokud zásobník obsahuje lokální úrovně, vyprázdní jej na základní úroveň, vrátí první
 * DEDENT a zbytek naplánuje do pending_dedents. Dodatečný EOL nevytváří.
 * @param token Cílový token pro DEDENT nebo EOF.
 * @return Vždy 0; další DEDENTy se vracejí při následujících voláních.
 */
static int emit_eof(Token *token) {
    // Před koncovým EOF musíme parseru oznámit uzavření zbývajících bloků.
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
 * @brief Rozliší klíčové slovo či název typu od běžného identifikátoru.
 *
 * Porovnává celé jméno s podporovanými slovy základního jazyka. Nepodporovaná slova
 * rozšíření zůstávají identifikátory.
 * @param text Úplné jméno ukončené nulovým znakem.
 * @return Odpovídající TokenType; při neshodě TOKEN_IDENTIFIER.
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
 * @brief Odstraní oddělovače číslic a převede literál na Int.
 * @param text Lexikálně ověřený zápis celého čísla.
 * @param [out] out Místo pro výslednou hodnotu.
 * @return 0 při úspěchu, jinak 99 při selhání pomocného bufferu.
 * @note Přetečení rozsahu Int se v této funkci nekontroluje.
 */
static int convert_int(const char *text, int *out) {
    DynamicString tmp;
    dynamic_buffer_init(&tmp);

    // strtol() nepřijímá podtržítka zdrojového zápisu, proto je před převodem vynecháme.
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
 * @brief Odstraní podtržítka a příponu a převede zápis na Double.
 *
 * Převod odmítá nekonečný výsledek, neúplné zpracování zápisu a úplné podtečení na nulu s
 * ERANGE. Nenulové reprezentovatelné subnormální výsledky přijímá.
 * @param text Lexikálně ověřený zápis desetinného literálu.
 * @param [out] out Místo pro výslednou hodnotu.
 * @return 0 při úspěchu, 1 při neplatném výsledku převodu, 99 při selhání bufferu.
 * @note Předpokládá standardní číselné locale jazyka C.
 */
static int convert_double(const char *text, double *out) {
    DynamicString tmp;
    dynamic_buffer_init(&tmp);

    for (size_t i = 0; text[i] != '\0'; ++i) {
        char c = text[i];

        if (c == '_') {
            continue;
        }

        // Volitelnou koncovou příponu d/D nepředáváme funkci strtod().
        if ((c == 'd' || c == 'D') && text[i + 1] == '\0') {
            continue;
        }

        int rc = ds_append(&tmp, c);

        if (rc != 0) {
            dynamic_buffer_free(&tmp);
            return rc;
        }
    }

    // Stav errno nulujeme před převodem, aby nezůstala chyba starší knihovní operace.
    errno        = 0;
    char  *end   = NULL;
    double value = strtod(tmp.data != NULL ? tmp.data : "0", &end);

    // Odmítneme nekonečný výsledek a úplné podtečení nenulové hodnoty na nulu.
    if (!isfinite(value) || end == NULL || *end != '\0' || (errno == ERANGE && value == 0.0)) {
        dynamic_buffer_free(&tmp);
        return 1;
    }

    *out = value;

    dynamic_buffer_free(&tmp);

    return 0;
}

/**
 * @brief Přeskočí blokový komentář po již načteném otevření.
 *
 * Čítač depth začíná na 1 a umožňuje vnořené komentáře. Každý výskyt LF zaznamená i do
 * příznaku skipped_newline.
 * @param [out] had_newline Příznak, zda obsah komentáře zahrnoval LF; musí být platným ukazatelem.
 * @return 0 při ukončení, 1 při EOF uvnitř komentáře, 99 při přetečení čítače či chybě vrácení
 * znaku.
 */
static int consume_block_comment(bool *had_newline) {
    // Otevírací dvojice již byla načtena; počáteční hloubka proto není nulová.
    size_t depth = 1;
    *had_newline = false;

    while (depth > 0) {
        int c = read_char();

        if (c == EOF) {
            return 1;
        }

        // Nový řádek v komentáři ovlivňuje příznak tokenu i dokončení logického řádku.
        if (c == '\n') {
            skipped_newline = true;
            *had_newline    = true;
            continue;
        }

        if (c == '/') {
            int next = read_char();

            if (next == '*') {
                // Každé vnořené otevření vyžaduje vlastní odpovídající ukončení.
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
 * @brief Dokončí obsluhu blokového komentáře obsahujícího LF.
 *
 * Za uzavřením smí do dalšího LF nebo EOF následovat jen bílé znaky. Podle závorek a obsahu
 * logického řádku buď vytvoří EOL, nebo připraví pokračování scanneru.
 * @param token Výstupní token pro případný EOL.
 * @return 0 při vytvoření EOL, -1 pro pokračování čtení, 1 při nepřípustném znaku za komentářem.
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

        // Uvnitř závorek pokračuje stejný logický řádek bez EOL.
        return -1;
    }

    continuation_line = false;

    if (!line_has_token) {
        // Samostatný komentář bez předchozího tokenu nevytváří oddělovač příkazů.
        return -1;
    }

    line_has_token = false;
    token->type    = TOKEN_EOL;

    return 0;
}

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
/**
 * @brief Změří odsazení a přeskočí prázdné a komentářové řádky.
 *
 * Na tokenovém řádku odmítá nepovolené bílé znaky v prefixu a ověřuje návrat na uloženou
 * úroveň. Uvnitř závorek kontroluje pokračovací odsazení bez změn zásobníku bloků.
 * @param token Výstupní místo pro INDENT, DEDENT nebo EOF.
 * @return 0 při vytvoření tokenu, -1 pro pokračování čtení lexému, 1 při lexikální chybě nebo 99
 * při interním selhání.
 */
static int handle_line_start(Token *token) {
    size_t spaces                    = 0;
    bool   indentation_locked        = false;
    bool   invalid_indent_whitespace = false;

    while (at_line_start) {
        int c;

        if (!indentation_locked) {
            // Do šířky odsazení započítáváme pouze obyčejné mezery.
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
            /* Jednořádkový blokový komentář před prvním tokenem nahrazuje bílé místo.
             * Mezery za ním již nezvyšují původně naměřený prefix odsazení. */
            do {
                c = read_char();

                if (c == '\t' || c == '\f' || c == '\v') {
                    invalid_indent_whitespace = true;
                }
            } while (c == ' ' || c == '\t' || c == '\f' || c == '\v');
        }

        // CR přijímáme jako fyzický konec řádku pouze v dvojici CRLF.
        if (c == '\r') {
            int next = read_char();

            if (next != '\n') {
                return 1;
            }

            c = '\n';
        }

        if (c == '\n') {
            skipped_newline = true;

            // Prázdný řádek nevytváří novou úroveň odsazení.
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

        // Ani řádky obsahující pouze komentáře nemění blokové odsazení.
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

                // Zachováme počet mezer naměřený před jednořádkovým blokovým komentářem.
                indentation_locked = true;

                continue;
            }

            // Nejde o začátek komentáře; vrátíme lomítko a další znak pro rozpoznání dělení.
            if (invalid_indent_whitespace) {
                return 1;
            }

            if (push_char(next) != 0 || push_char(c) != 0) {
                return 99;
            }
        } else {
            // Na fyzickém řádku začíná skutečný lexém.
            if (invalid_indent_whitespace) {
                return 1;
            }

            if (push_char(c) != 0) {
                return 99;
            }
        }

        if (paren_depth > 0) {
            /* Pokračovací řádek musí být odsazen více než začátek logického řádku.
             * Zásobník blokového odsazení se přitom nemění. */
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
            // Přidáme novou úroveň do dynamického zásobníku bez pevného limitu hloubky.
            if (indent_top > SIZE_MAX - 2 || reserve_indents(indent_top + 2) != 0) {
                return 99;
            }

            indent_stack[++indent_top] = spaces;
            token->type                = TOKEN_INDENT;

            return 0;
        }

        if (spaces < indent_stack[indent_top]) {
            size_t count = 0;

            // Jediný fyzický řádek může současně uzavřít několik vnořených bloků.
            while (indent_top > 0 && spaces < indent_stack[indent_top]) {
                --indent_top;
                ++count;
            }

            // Menší odsazení se musí přesně shodovat s některou již uloženou úrovní.
            if (spaces != indent_stack[indent_top]) {
                return 1;
            }

            // Jeden DEDENT vrátíme nyní; ostatní čekají na další volání scanneru.
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
 * @brief Načte celé ASCII jméno a určí jeho typ tokenu.
 *
 * První znak již načetl volající. Samotné podtržítko odmítá; ostatní jména předá klasifikaci
 * klíčových slov. Identifikátor získá vlastní kopii textového atributu.
 * @param token Výstupní token.
 * @param first Již načtené počáteční písmeno nebo podtržítko.
 * @return 0 při úspěchu, 1 pro samotné podtržítko, 99 při selhání bufferu nebo alokace.
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

    // Podtržítko lze použít v delším jménu, ale samotné v základním jazyce neplatí.
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
 * @brief Připojí posloupnost číslic s volitelnými skupinami podtržítek.
 *
 * Více podtržítek za sebou je dovoleno pouze mezi číslicemi. První znak mimo posloupnost
 * předá volajícímu bez jeho vracení do zásobníku.
 * @param first_digit První číslice, nebo záporná hodnota, pokud již byla připojena.
 * @param [out] terminator První znak za posloupností, případně EOF.
 * @return 0 při úspěchu, 1 pro neplatné oddělovače, 99 při nedostatku paměti.
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
            // Po libovolně dlouhé skupině podtržítek musí následovat číslice.
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
 * @brief Ověří znak za číslem a vrátí přípustný oddělovač do vstupu.
 * @param c První dosud nezpracovaný znak za číselným literálem.
 * @return 1 pro písmeno nebo podtržítko; jinak výsledek push_char(), tedy 0 nebo 99.
 */
static int finish_number_delimiter(int c) {
    if (is_letter(c) || c == '_') {
        return 1;
    }

    return push_char(c);
}

/**
 * Začátek AI části
 * Použitý model: GPT-6.1 Sol
 * Rozsah: Úpravy původního rozpoznávání číselných literálů.
 * Porovnej funkci read_number() s mým návrhem DKA, specifikací jazyka a
 * poznatky z fóra. Otestuj podtržítka mezi číslicemi, exponenty, přípony a
 * úvodní nuly. Zápisy jako 01.5 musí být odmítnuty, zatímco 0.5, .5 a 2e001
 * mají být přijaty. Nalezené chyby oprav a vysvětli změny v implementaci.
 */

/**
 * @brief Rozpozná literál Int nebo Double a převede jeho hodnotu.
 *
 * Podporuje desetinnou část, exponent, podtržítka mezi číslicemi a příponu d/D u desetinných
 * forem. Tečku následovanou jménem ponechá pro členský přístup VALMEM.
 * @param token Výstupní token s číselným atributem.
 * @param first První načtená číslice, nebo tečka u zápisu typu .5.
 * @return 0 při úspěchu, 1 pro neplatný zápis či převod Double, 99 při interní chybě.
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
        // Počáteční nula musí být jedinou číslicí celé části literálu.
        if (first == '0') {
            if (ds_append(&buffer, '0') != 0) {
                return 99;
            }

            term = read_char();

            // Další číslice celé části nejsou povoleny ani při oddělení podtržítkem.
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
                // VALMEM: znak jména vrátíme nyní, tečku vrátí dokončení číselného tokenu.
                if (push_char(c) != 0)
                    return 99;
            } else {
                has_fraction = true;
                if (ds_append(&buffer, '.') != 0)
                    return 99;
                if (!is_digit(c))
                    return 1;
                int rc = append_digit_sequence(c, &term);
                if (rc != 0)
                    return rc;
            }
        }
    }

    // Exponent může následovat po celé i desetinné části a mění výsledný typ na Double.
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

        // Po e/E a případném znaménku musí být alespoň jedna číslice exponentu.
        if (!is_digit(c)) {
            return 1;
        }

        int rc = append_digit_sequence(c, &term);

        if (rc != 0)
            return rc;
    }

    // Přípona je povolena jen po desetinné části nebo exponentu.
    if (term == 'd' || term == 'D') {
        if (!has_fraction && !has_exponent) {
            // Samotná celá část s d/D, například 1d, není podporovanou formou Double.
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

    // Typ literálu je dán jeho zápisem; převod hodnoty následuje až po lexikální kontrole.
    if (has_fraction || has_exponent) {
        token->type = TOKEN_DOUBLE_LITERAL;

        return convert_double(buffer.data, &token->attribute.d_val);
    }

    // Doplňková kontrola zápisu Int začínajícího nulou; další číslice již odmítla předchozí větev.
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
 * @brief Načte běžný řetězec a dekóduje pět podporovaných escape sekvencí.
 *
 * Otevírací uvozovka již byla spotřebována. Doslovné LF, CR, EOF nebo nepodporovaný escape
 * odmítá jako lexikální chybu.
 * @param token Výstupní řetězcový token s vlastní kopií hodnoty.
 * @return 0 při úspěchu, 1 při neplatném obsahu či ukončení, 99 při nedostatku paměti.
 */
static int read_normal_string(Token *token) {
    dynamic_buffer_clear(&buffer);

    while (true) {
        int c = read_char();

        // Běžný řetězec nesmí pokračovat přes fyzický konec řádku.
        if (c == EOF || c == '\n' || c == '\r') {
            return 1;
        }

        if (c == '"') {
            token->type = TOKEN_STRING_LITERAL;

            return set_string_attribute(token);
        }

        // Přijímáme pouze escape sekvence výslovně uvedené ve specifikaci.
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
 * Začátek AI části
 * Použitý model: GPT-6.1 Sol
 * Rozsah: Přepracování zpracování uzavírací řady uvozovek.
 * Zkontroluj zpracování uvozovek ve víceřádkových řetězcích podle specifikace a
 * upřesnění z fóra. Otestuj především vstupy tvořené sedmi a osmi uvozovkami za
 * sebou, jednotlivé uvozovky uvnitř řetězce a neukončené řetězce. Oprav funkci
 * tak, aby správně rozlišovala obsah řetězce a jeho ukončovací uvozovky.
 */
/**
 * @brief Načte víceřádkový řetězec bez dekódování escape sekvencí.
 *
 * Načte vždy celou souvislou řadu uvozovek. Obsahuje-li alespoň tři, poslední tři tvoří
 * ukončení a předchozí uvozovky z této řady zůstanou součástí hodnoty.
 * @param token Výstupní řetězcový token.
 * @return 0 při úspěchu, 1 pro EOF před uzavřením, 99 při selhání bufferu či kopie atributu.
 */
static int read_multiline_string(Token *token) {
    dynamic_buffer_clear(&buffer);

    while (true) {
        int c = read_char();

        if (c == '"') {
            size_t quotes = 0;

            // Než rozhodneme o obsahu a ukončení, načteme celou řadu uvozovek.
            do {
                if (ds_append(&buffer, '"') != 0) {
                    return 99;
                }

                ++quotes;
                c = read_char();
            } while (c == '"');

            // Celá souvislá řada je načtena; teprve nyní lze oddělit obsah od posledních tří
            // uvozovek.
            if (quotes >= 3) {
                // Z hodnoty odebereme pouze poslední tři uvozovky tvořící uzavírací oddělovač.
                buffer.length -= 3;
                buffer.data[buffer.length] = '\0';

                if (push_char(c) != 0) {
                    return 99;
                }

                token->type = TOKEN_STRING_LITERAL;

                return set_string_attribute(token);
            }

            // Jedna či dvě uvozovky již byly připojeny jako běžný obsah řetězce.
        }

        if (c == EOF) {
            return 1;
        }

        // Zpětná lomítka i fyzické nové řádky ukládáme doslova, bez dekódování.
        if (ds_append(&buffer, (char)c) != 0) {
            return 99;
        }
    }
}
/**
 * Konec AI části: read_multiline_string()
 */

/**
 * @brief Rozliší běžný, prázdný a víceřádkový řetězec.
 *
 * Volající již spotřeboval první otevírací uvozovku. Druhá uvozovka může ukončit prázdný
 * řetězec, ale při třetí začne víceřádková forma.
 * @param token Výstupní token řetězcového literálu.
 * @return 0 při úspěchu, 1 pro neplatný řetězec, 99 při interním selhání.
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

    // Dvojice uvozovek bez třetí otevírací uvozovky tvoří prázdný běžný řetězec.
    if (push_char(third) != 0)
        return 99;

    dynamic_buffer_clear(&buffer);
    token->type = TOKEN_STRING_LITERAL;

    return set_string_attribute(token);
}

/**
 * @brief Rozpozná nedělitelnou anotaci \@main.
 *
 * Znak @ již načetl volající; za ním musí bez mezer následovat main. Další písmeno, číslice
 * nebo podtržítko za celou anotací znamená chybu.
 * @param token Výstupní token TOKEN_AT_MAIN.
 * @return 0 při úspěchu, 1 pro neplatnou anotaci, 99 při selhání vrácení následujícího znaku.
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
/**
 * @brief Rozpozná další token při obsluze komentářů a logických řádků.
 *
 * Před čtením vstupu vrací naplánované DEDENTy. Následně řeší počátek fyzického řádku a
 * vybírá rozpoznávač podle načteného znaku.
 * @param token Výstupní token; nesmí být NULL.
 * @return 0 při úspěchu, 1 při lexikální chybě, 99 při neplatném rozhraní nebo interním selhání.
 */
static int scan_next(Token *token) {
    if ((source == NULL && memory_source == NULL) || token == NULL) {
        return 99;
    }

    // Naplánovaná ukončení bloků vracíme před dalším čtením vstupu.
    // Při čekajících DEDENTech vstup neposouváme a nevytváříme další lexém.
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

        // Vodorovné bílé znaky mezi lexémy přeskakujeme; prefix odsazení se řeší zvlášť.
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
                // Tečka následovaná číslicí zahajuje Double, například .5; jinak je samostatným
                // tokenem.
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

            // Nebylo rozpoznáno otevření komentáře; lomítko tedy znamená dělení.
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
            /* Párování závorek kontroluje parser.
             * Přebytečná zavírací závorka zde nesmí způsobit podtečení čítače. */
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
 * @brief Nastaví proudový vstup a resetuje stav scanneru.
 * @param source_file Proud vlastněný volajícím; pro čtení musí být platný.
 * @note Předchozí interní prostředky uvolní, ale proud nezavírá ani neuvolňuje dříve vrácené
 * atributy.
 */
void scanner_init(FILE *source_file) {
    // Před změnou vstupu uvolníme předchozí interní alokace scanneru.
    scanner_free();

    source = source_file;
}

// ZAČÁTEK ČÁSTI S ASISTENCÍ AI
// Zadání AI: Inicializuj scanner nad polem bajtu pro opakovane pruchody.
/**
 * @brief Připraví scanner pro čtení z paměťového pole.
 *
 * Nejprve uvolní dosavadní interní stav a potom uloží ukazatel, délku a nulovou pozici.
 * Opětovná inicializace nad stejným polem umožňuje více průchodů parseru bez rewind() a
 * fmemopen().
 * @param input Pole bajtů vlastněné volajícím; pro aktivní vstup nesmí být NULL.
 * @param length Přesný počet čitelných bajtů; koncová nula není vyžadována.
 * @note Pole se nekopíruje; musí zůstat platné po celou dobu čtení.
 * @note Nevyžaduje nulový terminátor; konec vstupu určuje výhradně length.
 */
void scanner_init_buffer(const char *input, size_t length) {
    scanner_free();
    // Ukládáme jen vypůjčený ukazatel; délku a životnost původního pole zajišťuje volající.
    memory_source = (const unsigned char *)input;
    memory_length = length;
    // Nové zpracování stejného paměťového vstupu začíná od jeho prvního bajtu.
    memory_position = 0;
}
// KONEC ČÁSTI S ASISTENCÍ AI

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
/**
 * @brief Vrátí jeden token a přiřadí informaci o přeskočeném LF.
 *
 * Při prvním volání zajistí základní úroveň zásobníku odsazení. Po rozpoznání tokenu ověří
 * chyby proudového vstupu a pro skutečný lexém předá příznak newline_before.
 * @param token Platné místo pro výstupní token.
 * @return 0 při úspěchu, 1 při lexikální chybě, 99 při chybě API, alokace nebo čtení.
 * @note Atributy jsou platné pouze při úspěšném návratu; předchozí atributy volající uvolňuje sám.
 */
int get_next_token(Token *token) {
    if ((source == NULL && memory_source == NULL) || token == NULL) {
        return 99;
    }

    // Příznak předchozího LF vyplníme jen pro úspěšně rozpoznaný skutečný lexém.
    token->newline_before = false;

    // Odložená alokace umožní hlásit nedostatek paměti běžným návratovým kódem 99.
    if (indent_stack == NULL) {
        if (reserve_indents(1) != 0) {
            return 99;
        }

        indent_stack[0] = 0;
    }

    int rc = scan_next(token);

    // Chybu fgetc() nesmíme zaměnit s řádným koncem vstupu.
    // EOF způsobené chybou čtení nesmí být vydáváno za řádný konec programu.
    if (source != NULL && ferror(source)) {
        if (rc == 0 && (token->type == TOKEN_IDENTIFIER || token->type == TOKEN_STRING_LITERAL)) {
            token_destroy(token);
        }

        return 99;
    }

    /* Layoutové tokeny ani EOF nespotřebovávají informaci o přeskočeném LF.
     * Příznak zachováme až pro příští skutečný lexém zdrojového programu. */
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
 * @brief Uvolní interní prostředky a obnoví výchozí stav scanneru.
 *
 * Zruší pracovní buffer a zásobník odsazení a resetuje pozice, čítače i příznaky vstupu.
 * Dříve vrácené textové atributy zůstávají ve vlastnictví volajícího.
 * @note Nevlastní a neuvolňuje paměťový vstup ani nezavírá proud; opakované volání je bezpečné.
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