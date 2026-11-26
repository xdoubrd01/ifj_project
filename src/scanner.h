/**
 * @project Implementace překladače imperativního jazyka IFJ26
 * @file scanner.h
 * @author Denis Doubrava (xdoubrd01)
 * @brief Definice tokenů a rozhraní scanneru pro proudový a paměťový vstup.
 * Definuje výčet TokenType a strukturu Token s atributem závislým na typu
 * tokenu. Deklaruje inicializaci vstupu, získání dalšího tokenu a uvolnění
 * interních prostředků scanneru.
 * @note AI byla využita k lepšímu znění a obsahu komentářů.
 */

#ifndef SCANNER_H
#define SCANNER_H

// Scanner vrací vždy jeden token; textové atributy po předání vlastní volající.
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

/**
 * Začátek AI části
 * Použitý model: GPT-6.1 Sol
 * Prompt: Ze zadaných fotek DKA mi vytvoř enum na seznam tokenů, ke každému tokenu přidej komentář
 * popisující token a rozděl je do smysluplných skupin
 */

typedef enum {
    TOKEN_EOF, ///< Konec vstupu po vrácení zbývajících značek DEDENT.
    TOKEN_EOL, ///< Konec neprázdného logického řádku mimo kulaté závorky.

    // Identifikátory a literály.
    TOKEN_IDENTIFIER, ///< Jméno, které není rozpoznaným klíčovým slovem ani názvem typu; atribut
                      ///< s_val.
    TOKEN_INT_LITERAL,    ///< Celočíselný literál s atributem i_val.
    TOKEN_DOUBLE_LITERAL, ///< Desetinný literál s atributem d_val.
    TOKEN_STRING_LITERAL, ///< Řetězcový literál s vlastněným atributem s_val.

    // Klíčová slova základního jazyka.
    TOKEN_KW_DEF,    ///< Klíčové slovo def pro definici funkce.
    TOKEN_KW_DO,     ///< Klíčové slovo do uvádějící tělo cyklu.
    TOKEN_KW_ELSE,   ///< Klíčové slovo else uvádějící alternativní větev.
    TOKEN_KW_IF,     ///< Klíčové slovo if pro podmíněný příkaz.
    TOKEN_KW_RETURN, ///< Klíčové slovo return pro návrat z funkce.
    TOKEN_KW_THEN,   ///< Klíčové slovo then uvádějící větev podmínky.
    TOKEN_KW_VAL,    ///< Klíčové slovo val pro neměnitelnou lokální hodnotu.
    TOKEN_KW_VAR,    ///< Klíčové slovo var pro měnitelnou proměnnou.
    TOKEN_KW_WHILE,  ///< Klíčové slovo while pro cyklus.
    TOKEN_KW_IMPORT, ///< Klíčové slovo import.

    // Datové typy základního jazyka.
    TOKEN_TYPE_INT,    ///< Název typu Int.
    TOKEN_TYPE_DOUBLE, ///< Název typu Double.
    TOKEN_TYPE_STRING, ///< Název typu String.
    TOKEN_TYPE_UNIT,   ///< Název typu Unit.

    // Operátory a oddělovače.
    TOKEN_ASSIGN, ///< Přiřazení nebo oddělovač definice (=).
    TOKEN_PLUS,   ///< Sčítání nebo unární plus (+).
    TOKEN_MINUS,  ///< Odčítání nebo unární minus (-).
    TOKEN_MUL,    ///< Násobení (*).
    TOKEN_DIV,    ///< Dělení (/); začátky komentářů se zpracují zvlášť.
    TOKEN_EQ,     ///< Porovnání rovnosti (==).
    TOKEN_NEQ,    ///< Porovnání nerovnosti (!=).
    TOKEN_LT,     ///< Porovnání menší než (<).
    TOKEN_LEQ,    ///< Porovnání menší nebo rovno (<=).
    TOKEN_GT,     ///< Porovnání větší než (>).
    TOKEN_GEQ,    ///< Porovnání větší nebo rovno (>=).
    TOKEN_LPAREN, ///< Otevírací kulatá závorka; zvyšuje paren_depth.
    TOKEN_RPAREN, ///< Zavírací kulatá závorka; párování ověřuje parser.
    TOKEN_COLON,  ///< Dvojtečka před typovou anotací (:).
    TOKEN_COMMA,  ///< Oddělovač parametrů nebo argumentů (,).

    // Další interpunkce a anotace vstupní funkce.
    TOKEN_DOT, ///< Samostatná tečka nebo členský přístup; . s následující číslicí patří číslu.
    TOKEN_SEMICOLON, ///< Středník jako oddělovač příkazů (;).
    TOKEN_AT_MAIN,   ///< Nedělitelná anotace \@main.

    // Hranice bloků odvozené z odsazení.
    TOKEN_INDENT, ///< Zahájení nové úrovně blokového odsazení.
    TOKEN_DEDENT  ///< Ukončení jedné úrovně blokového odsazení.
} TokenType;

/**
 * @brief Token a jeho atribut závislý na typu.
 *
 * Číselné literály ukládají hodnotu přímo do unie. Identifikátor a řetězcový literál
 * obsahují vlastněnou kopii textu, kterou musí volající uvolnit před přepsáním nebo
 * zahozením tokenu.
 * @note i_val čtěte jen pro TOKEN_INT_LITERAL a d_val jen pro TOKEN_DOUBLE_LITERAL.
 * @note s_val je platné pro TOKEN_IDENTIFIER a TOKEN_STRING_LITERAL; ostatní typy atribut nemají
 * definován.
 */
typedef struct {
    TokenType type; ///< Typ tokenu určující význam atributu.

    /**
     * @brief Označuje LF přeskočené před tímto lexémem.
     *
     * Layoutové tokeny a EOF mají hodnotu false a příznak nespotřebují. Nový řádek uvnitř
     * řetězcového literálu jej nenastavuje. Parser jej používá například při kontrole koncové
     * čárky.
     */
    bool newline_before;

    union {
        int    i_val; ///< Hodnota pro TOKEN_INT_LITERAL.
        double d_val; ///< Hodnota pro TOKEN_DOUBLE_LITERAL.
        char  *s_val; ///< Vlastněný text pro identifikátor nebo řetězcový literál.
    } attribute;
} Token;

/**
 * @brief Inicializuje scanner nad vstupním proudem.
 * @param source_file Platný proud vlastněný volajícím.
 * @note Uvolní předchozí interní stav, ale nezavírá proud ani neuvolňuje dříve vrácené textové
 * atributy.
 */
void scanner_init(FILE *source_file);

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
void scanner_init_buffer(const char *input, size_t length);

/**
 * @brief Načte další token včetně značek logických řádků a odsazení.
 *
 * Před voláním nastavte vstup pomocí scanner_init() nebo scanner_init_buffer(). Vrácené
 * textové atributy vlastní volající a musí je uvolnit před přepsáním tokenu.
 * @param token Platný výstupní ukazatel; nesmí být NULL.
 * @return 0 při úspěchu, 1 při lexikální chybě, 99 při interní chybě, neplatném API nebo chybě
 * vstupního proudu.
 * @note Atributy používejte pouze po úspěšném návratu.
 * @note Po chybě scanner uvolněte nebo znovu inicializujte, než budete pokračovat.
 */
int get_next_token(Token *token);

/**
 * @brief Uvolní pracovní buffer, zásobník odsazení a resetuje scanner.
 *
 * Ruší pouze interní prostředky scanneru. Proud, paměťový vstup i dříve vrácené textové
 * atributy nadále spravuje volající.
 * @note Lze volat po úspěchu i po chybě; opakované volání je bezpečné.
 */
void scanner_free(void);

#endif // SCANNER_H