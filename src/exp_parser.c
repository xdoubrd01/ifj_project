/**

@par         IFJ26
@file        exp_parser.c
@authors     Jakub Grásgruber <xgrasgj00>
@brief       Precedenční syntaktická analýza výrazů, která z tokenů staví typovaný strom výrazu.
*/

#include "exp_parser.h"
#include "array_utils.h"
#include "paramlists.h"
#include "scanner.h"
#include "semantic.h"
#include "token_utils.h"
#include "valmem.h"
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/** @brief Index tokenu do precedenční tabulky. */
typedef enum {
    PREC_IDX_ADD_SUB,  ///< 0: sčítání a odčítání (+ -)
    PREC_IDX_MUL_DIV,  ///< 1: násobení a dělení (* /)
    PREC_IDX_RELATION, ///< 2: <, <=, >, >=
    PREC_IDX_EQ,       ///< 3: ==, !=
    PREC_IDX_LPAREN,   ///< 4: (
    PREC_IDX_RPAREN,   ///< 5: )
    PREC_IDX_VALUE,    ///< 6: celé číslo, desetinné číslo, řetězec, identifikátor
    PREC_IDX_EOF,      ///< 7: konec výrazu
    PREC_IDX_UNARY,    ///< 8: unární + - (OPRAVENO: index musí být 8)
    PREC_IDX_NONE      ///< 9: nedefinovaný nebo nerozpoznaný token
} prec_idx_t;

// AI - UPDATED: static
/**
 * @brief Vrátí index do precedenční tabulky pro daný token.
 *
 * Plus a minus jsou unární, pokud před nimi nestojí operand.
 *
 * @param current_type typ aktuálního tokenu
 * @param prev_type    typ předchozího tokenu výrazu
 * @return index do precedenční tabulky
 */
static prec_idx_t get_precedence_index(TokenType current_type, TokenType prev_type) {
    switch (current_type) {
    case TOKEN_PLUS:
    case TOKEN_MINUS:
        if (prev_type == TOKEN_EOF || prev_type == TOKEN_LPAREN || prev_type == TOKEN_PLUS ||
            prev_type == TOKEN_MINUS || prev_type == TOKEN_MUL || prev_type == TOKEN_DIV ||
            prev_type == TOKEN_LT || prev_type == TOKEN_LEQ || prev_type == TOKEN_GT ||
            prev_type == TOKEN_GEQ || prev_type == TOKEN_EQ || prev_type == TOKEN_NEQ) {
            return PREC_IDX_UNARY; // unární operátor + nebo -
        } else {
            return PREC_IDX_ADD_SUB; // binární operátor + nebo -
        }
    case TOKEN_MUL:
    case TOKEN_DIV:
        return PREC_IDX_MUL_DIV;
    case TOKEN_LT:
    case TOKEN_LEQ:
    case TOKEN_GT:
    case TOKEN_GEQ:
        return PREC_IDX_RELATION;
    case TOKEN_EQ:
    case TOKEN_NEQ:
        return PREC_IDX_EQ;
    case TOKEN_LPAREN:
        return PREC_IDX_LPAREN;
    case TOKEN_RPAREN:
        return PREC_IDX_RPAREN;
    case TOKEN_INT_LITERAL:
    case TOKEN_DOUBLE_LITERAL:
    case TOKEN_STRING_LITERAL:
    case TOKEN_IDENTIFIER:
        return PREC_IDX_VALUE;
    case TOKEN_EOF: // AI - UPDATED: odstraněn duplicitní 'case TOKEN_EOF'
    case TOKEN_EOL:
    case TOKEN_SEMICOLON:
    case TOKEN_KW_DO:
    case TOKEN_KW_THEN:
    case TOKEN_COMMA:
        return PREC_IDX_EOF;
    default:
        return PREC_IDX_NONE; // nedefinovaný nebo nerozpoznaný token
    }
}

// prompt: "Neměň precedenční tabulku, pokud by bylo potřeba ji upravit, upozorni mě."
// AI: poznámky z kontroly, hodnoty tabulky se neměnily
/**
 * @brief Precedenční tabulka operátorů a operandů.
 *
 * Řádek = terminál na zásobníku, sloupec = příchozí token.
 * - `<` nižší priorita (příchozí token se vloží na zásobník)
 * - `>` vyšší priorita (operátor ze zásobníku se zredukuje)
 * - `E` chyba (syntaktická chyba)
 * - `=` stejná priorita (párování závorek "()")
 */
static char prec_table[9][9] = {
    // +-   */   <=>  ==!=  (    )   hod  EOF  UNÁR
    {'>', '<', '>', '>', '<', '>', '<', '>', '<'}, // +-
    {'>', '>', '>', '>', '<', '>', '<', '>', '<'}, // */
    {'<', '<', 'E', '>', '<', '>', '<', '>', '<'}, // REL <=>
    {'<', '<', '<', 'E', '<', '>', '<', '>', '<'}, // EQ == !=
    {'<', '<', '<', '<', '<', '=', '<', 'E', '<'}, // (
    {'>', '>', '>', '>', 'E', '>', 'E', '>', 'E'}, // )
    {'>', '>', '>', '>', 'E', '>', 'E', '>', 'E'}, // hodnota
    {'<', '<', '<', '<', '<', 'E', '<', 'E', '<'}, // EOF
    {'>', '>', '>', '>', '<', '>', '<', '>', '<'}  // unární
};

// IMPLEMENTACE ZÁSOBNÍKU PRECEDENČNÍHO ANALYZÁTORU

/** @brief Druh symbolu na zásobníku precedenčního analyzátoru. */
typedef enum {
    SYM_TERMINAL,     ///< terminál (operátor nebo operand)
    SYM_NON_TERMINAL, ///< neterminál (výsledek redukce)
    SYM_STOP,         ///< zarážka označující dno nebo začátek zásobníku
    SYM_SHIFT_MARKER  ///< značka posunu '<'
} Symbol_type;

/** @brief Položka zásobníku precedenčního analyzátoru. */
typedef struct {
    Symbol_type type;
    TokenType   token_type; ///< token přiřazený symbolu
    ExprNode   *node;       ///< AI - UPDATED: podstrom neterminálu (vlastněný)
    prec_idx_t  prec_idx;   ///< index symbolu do precedenční tabulky
    Token       token;      ///< uložený token pro sémantickou analýzu
} Stack_Item;

// dynamický zásobník
static Stack_Item *stack;
static size_t      stack_capacity;
static int         stack_top = -1; // AI - UPDATED: static

// dynamický zásobník
/**
 * @brief Zvětší zásobník, pokud je potřeba více položek.
 *
 * Pokud je zásobník příliš velký nebo dojde paměť, ukončí překlad kódem 99.
 *
 * @param required požadovaný počet položek
 */
static void reserve_stack(size_t required) {
    size_t capacity;

    if (required <= stack_capacity) {
        return;
    }
    if (required > INT_MAX ||
        !array_next_capacity(stack_capacity, required, sizeof(*stack), 64, &capacity)) {
        fprintf(stderr, "Internal error (99): expression stack is too large\n");
        exit(99);
    }

    Stack_Item *grown = realloc(stack, capacity * sizeof(*grown));
    if (grown == NULL) {
        fprintf(stderr, "Internal error (99): out of memory\n");
        exit(99);
    }
    stack          = grown;
    stack_capacity = capacity;
}

// dynamický zásobník
/** @brief Uvolní paměť zásobníku po dokončení vnějšího výrazu. */
static void release_stack(void) {
    free(stack);
    stack          = NULL;
    stack_capacity = 0;
}

/**
 * @brief Vloží položku na vrchol zásobníku.
 *
 * @param item vkládaná položka
 */
static void push(Stack_Item item) { // AI - UPDATED: static
    // dynamický zásobník
    reserve_stack((size_t)stack_top + 2);
    stack[++stack_top] = item;
}

/**
 * @brief Odebere položku z vrcholu zásobníku.
 *
 * @return odebraná položka, při prázdném zásobníku ukončí překlad kódem 99
 */
static Stack_Item pop(void) { // AI - UPDATED: static
    if (stack_top >= 0) {
        return stack[stack_top--];
    } else {
        fprintf(stderr, "Stack underflow\n");
        exit(99);
    }
}

/**
 * @brief Najde nejvyšší terminál nebo zarážku na zásobníku.
 *
 * @return nalezená položka, pokud chybí, ukončí překlad kódem 99
 */
static Stack_Item get_top_terminal(void) { // AI - UPDATED: static
    for (int i = stack_top; i >= 0; i--) {
        if (stack[i].type == SYM_TERMINAL || stack[i].type == SYM_STOP) {
            return stack[i];
        }
    }
    fprintf(stderr, "No terminal symbol found in stack\n");
    exit(99);
}

// prompt: "Přepiš exp_parser.c tak, aby místo printf stavěl strom výrazu
//          s typy." - chybu našla AI při testování přepisu na "a + b"
// AI - UPDATED: značka se vkládala za nejvyšší neterminál (nebo pod '$'),
//               nyní se vkládá hned za nejvyšší terminál
/**
 * @brief Vloží značku '<' hned ZA nejvyšší terminál (nebo zarážku).
 *
 * Například "$ E" a posun '+' dá "$ < E +".
 */
static void insert_shift_marker(void) {
    int top_index = stack_top;
    while (top_index >= 0 && stack[top_index].type != SYM_TERMINAL &&
           stack[top_index].type != SYM_STOP) {
        top_index--; // přeskočí neterminály nad nejvyšším terminálem
    }

    reserve_stack((size_t)stack_top + 2); // dynamický zásobník

    for (int i = stack_top; i > top_index; i--) {
        stack[i + 1] = stack[i]; // posune neterminály o jednu pozici výš
    }

    Stack_Item marker    = {.type = SYM_SHIFT_MARKER, .token_type = TOKEN_EOF};
    stack[top_index + 1] = marker;
    stack_top++;
}

// prompt: "Přepiš exp_parser.c tak, aby místo printf stavěl strom výrazu
//          s typy." - při chybě je potřeba uvolnit zásobník, tokeny
//          a podstromy na něm zůstávaly neuvolněné
/**
 * @brief Uvolní data na haldě, která vlastní jedna položka zásobníku.
 *
 * @param item uvolňovaná položka
 */
static void free_item(Stack_Item *item) {
    if (item->type == SYM_NON_TERMINAL) {
        expr_free(item->node); // podstrom
        item->node = NULL;
    } else if (item->type == SYM_TERMINAL) {
        // AI - UPDATED: terminál může obsahovat hotové volání funkce (FUNEXP)
        expr_free(item->node);
        item->node = NULL;
        if (item->token_type == TOKEN_IDENTIFIER || item->token_type == TOKEN_STRING_LITERAL) {
            token_destroy(&item->token); // lexém ze scanneru
        }
    }
}

// AI - UPDATED: jen po base, nižší položky patří vnějšímu výrazu
/**
 * @brief Vyprázdní zásobník a uvolní vše, co na něm je (použito při chybě).
 *
 * @param base první index zásobníku, který patří aktuálnímu výrazu
 */
static void stack_clear(int base) {
    while (stack_top >= base) {
        free_item(&stack[stack_top]);
        stack_top--;
    }
}

// prompt: "parse_expression musí sedět k parseru: v CALL_OR_EXP je
//          identifikátor načtený dřív, než se ví, že jde o výraz, takže
//          převezmi tento token i lookahead za ním."
/**
 * @brief Načte další token, nejdřív vrátí uložený lookahead volajícího.
 *
 * @param current   výstup: načtený token
 * @param saved     token, který už načetl parser
 * @param use_saved true, pokud se má nejdřív použít @p saved
 * @return 0 při úspěchu, 1 lexikální chyba, 99 interní chyba
 */
static int read_token(Token *current, Token *saved, bool *use_saved) {
    if (*use_saved) {
        *current   = *saved; // token už načetl parser
        *use_saved = false;
        return 0;
    }
    return get_next_token(current); // 0, 1 lexikální, 99 interní
}

// prompt: "Pravidlo E -> i: místo PUSHS vytvoř list stromu, typ identifikátoru
//          se zjistí z tabulky symbolů přes semantic.c."
/**
 * @brief Pravidlo E -> term: vytvoří listový uzel a převezme s_val tokenu.
 *
 * @param st   tabulka symbolů
 * @param item terminál, ze kterého se uzel vytváří
 * @param out  výstup: vytvořený uzel
 * @return 0 při úspěchu, jinak kód chyby (2, 3, 5, 6, 99)
 */
static int make_term_node(SymTable *st, Stack_Item *item, ExprNode **out) {
    ExprNode *node = NULL;
    data_type type;
    unsigned  id; // jedinečné id proměnné z tabulky symbolů
    int       rc;

    // AI - UPDATED: volání funkce, jeho uzel už vytvořila parse_call
    if (item->node != NULL) {
        *out       = item->node;
        item->node = NULL;
        return 0;
    }

    switch (item->token_type) {
    case TOKEN_INT_LITERAL:
        node = expr_new(EXPR_INT_LIT, type_int);
        if (node != NULL) {
            node->u.i_val = item->token.attribute.i_val;
        }
        break;
    case TOKEN_DOUBLE_LITERAL:
        node = expr_new(EXPR_DOUBLE_LIT, type_double);
        if (node != NULL) {
            node->u.d_val = item->token.attribute.d_val;
        }
        break;
    case TOKEN_STRING_LITERAL:
        node = expr_new(EXPR_STRING_LIT, type_string);
        if (node != NULL) {
            node->u.s_val               = item->token.attribute.s_val; // vlastnictví -> uzel
            item->token.attribute.s_val = NULL;
        }
        break;
    case TOKEN_IDENTIFIER:
        if (symtable_search_var(st, item->token.attribute.s_val) == NULL &&
            symtable_search_func(st, item->token.attribute.s_val) != NULL) {
            return paramlists_bare_call(st, &item->token.attribute.s_val, out);
        }
        // proměnná musí být viditelná a mít známý typ (3 / 6)
        if ((rc = sem_get_variable_type(st, item->token.attribute.s_val, &type, &id)) != 0) {
            return rc;
        }
        node = expr_new(EXPR_VAR, type);
        if (node != NULL) {
            node->u.var.name            = item->token.attribute.s_val; // vlastnictví -> uzel
            node->u.var.id              = id;                          // překrytí je vyřešené
            item->token.attribute.s_val = NULL;
        }
        break;
    default:
        // samotný operátor, např. "a *" na konci výrazu
        fprintf(stderr, "Syntax error: missing operand\n");
        return 2;
    }

    if (node == NULL) {
        fprintf(stderr, "Internal error (99): out of memory\n");
        return 99;
    }
    *out = node;
    return 0;
}

// prompt: "Kolega potřebuje volání funkcí uvnitř výrazů, např.
//          val x = getValue() + 1, print(2 * getValue()) nebo
//          return getValue() + 1. Navrhni, jak to přidat do mého precedenčního
//          parseru bez změny precedenční tabulky."
/**
 * @brief Načte další token volání, po lexikální chybě jsou atributy tokenu neplatné.
 *
 * @param current výstup: načtený token
 * @return 0 při úspěchu, 1 lexikální chyba, 99 interní chyba
 */
static int read_call_token(Token *current) {
    int rc = get_next_token(current);
    if (rc != 0) {
        fprintf(stderr, "%s error while getting the next token\n",
                rc == 1 ? "Lexical" : "Internal");
        current->type = TOKEN_EOF;
    }
    return rc;
}

// prompt: (stejný jako u read_call_token) - argumenty se zpracují rekurzivně,
//          celé volání je pak jeden operand (sloupec "hodnota" tabulky)
/**
 * @brief Zpracuje seznamy argumentů "( args )" identifikátoru uloženého ve stack[item_index].
 *
 * @param st         tabulka symbolů
 * @param item_index index identifikátoru volané funkce na zásobníku
 * @param current    vstup: '(', výstup: první token za poslední ')'
 * @return 0 při úspěchu, jinak kód chyby
 */
static int parse_call(SymTable *st, int item_index, Token *current) {
    ExprNode **args = NULL; // zpracované argumenty
    size_t     argc = 0;    // počet zpracovaných argumentů
    data_type  type;        // návratový typ volané funkce
    ParamLists lists = {0};
    int        rc    = 0;
    do {
        size_t start = argc;
        rc           = read_call_token(current);
        while (rc == 0 && current->type != TOKEN_RPAREN) {
            ExprNode *arg = NULL;
            rc            = parse_expression(st, NULL, current, &arg);
            if (rc != 0)
                break;
            if (argc == SIZE_MAX || argc + 1 > SIZE_MAX / sizeof(*args)) {
                expr_free(arg);
                rc = 99;
                break;
            }
            ExprNode **grown = realloc(args, (argc + 1) * sizeof(*args));
            if (grown == NULL) {
                expr_free(arg);
                rc = 99;
                break;
            }
            args         = grown;
            args[argc++] = arg;
            if (current->type != TOKEN_COMMA)
                break;
            rc = read_call_token(current);
            if (rc == 0 && current->type == TOKEN_RPAREN && !current->newline_before)
                rc = 2;
        }
        if (rc == 0 && current->type != TOKEN_RPAREN)
            rc = 2;
        if (rc == 0 && !paramlists_append(&lists, argc - start))
            rc = 99;
        if (rc == 0)
            rc = read_call_token(current);
    } while (rc == 0 && current->type == TOKEN_LPAREN);
    if (rc == 0)
        rc = paramlists_check_shape(st, stack[item_index].token.attribute.s_val, &lists);
    if (rc == 0)
        rc = sem_check_call(st, stack[item_index].token.attribute.s_val, args, argc, &type);
    paramlists_free(&lists);

    ExprNode *call = (rc == 0) ? expr_new(EXPR_CALL, type) : NULL;
    if (rc == 0 && call == NULL) {
        fprintf(stderr, "Internal error (99): out of memory\n");
        rc = 99;
    }
    if (rc != 0) {
        for (size_t i = 0; i < argc; i++) {
            expr_free(args[i]);
        }
        free(args);
        return rc; // *current dál vlastní volající
    }

    call->u.call.name                       = stack[item_index].token.attribute.s_val; // vlastnictví
    stack[item_index].token.attribute.s_val = NULL;
    call->u.call.args                       = args;
    call->u.call.argc                       = argc;
    stack[item_index].node                  = call; // převezme ho E -> term v make_term_node

    return 0; // current už obsahuje token za posledním seznamem
}

/**
 * @brief Zpracuje postfixové členy (VALMEM) za operandem.
 *
 * Člen se váže k celému operandu (literál, proměnná, volání, výraz v závorkách
 * nebo Unit). Precedenční tabulka zůstává beze změny.
 *
 * @param st         tabulka symbolů
 * @param item_index index operandu na zásobníku
 * @param current    aktuální token, výstup: první token za posledním členem
 * @return 0 při úspěchu, jinak kód chyby
 */
static int parse_members(SymTable *st, int item_index, Token *current) {
    if (current->type != TOKEN_DOT)
        return 0;
    if (stack[item_index].node == NULL) {
        int rc = make_term_node(st, &stack[item_index], &stack[item_index].node);
        if (rc != 0)
            return rc;
    }
    while (current->type == TOKEN_DOT) {
        int rc = read_call_token(current);
        if (rc != 0)
            return rc;
        if (current->type != TOKEN_IDENTIFIER)
            return 2;
        data_type type;
        unsigned  operation;
        rc =
            valmem_check(current->attribute.s_val, stack[item_index].node->type, &operation, &type);
        if (rc != 0)
            return rc;
        ExprNode *member = expr_new(EXPR_MEMBER, type);
        if (member == NULL)
            return 99;
        member->u.member.receiver  = stack[item_index].node;
        member->u.member.name      = current->attribute.s_val;
        member->u.member.operation = operation;
        current->attribute.s_val   = NULL;
        stack[item_index].node     = member;
        rc                         = read_call_token(current);
        if (rc != 0)
            return rc;
    }
    return 0;
}

// prompt: "Přepiš parse_expression tak, aby místo printf stavěl strom a typy
//          se kontrolovaly v semantic.c." + (odpověď pro AI)
//          "Vracet kódy místo exit() je v pořádku."
// AI - UPDATED: parametry (bylo parse_expression(hashtable *ts) s nedefinovaným
//               first_token), sémantické akce staví ExprNode, každá chyba
//               prochází návěštím error
/**
 * @details Implementace precedenční analýzy výrazu, parametry a návratové
 *          hodnoty jsou popsané v exp_parser.h.
 */
int parse_expression(SymTable *st, Token *pending, Token *tok, // AI - UPDATED
                     ExprNode **out) {
    // AI - UPDATED: proměnné pro návěští error, reset zásobníku
    *out = NULL; // při chybě nic
    // AI - UPDATED: base místo resetu zásobníku, argumenty volání jsou vnořené
    int        base      = stack_top + 1; // první index zásobníku tohoto výrazu
    int        rc        = 0;             // kód chyby pro návěští error
    bool       use_saved = false;         // *tok ještě čeká na načtení
    Stack_Item popped_items[4];           // handle aktuální redukce
    int        pop_count = 0;             // počet platných popped_items

    // Inicializace zásobníku zarážkou
    Stack_Item stop_item = {.type = SYM_STOP, .token_type = TOKEN_EOF, .prec_idx = PREC_IDX_EOF};
    push(stop_item);

    TokenType prev_token_type = TOKEN_EOF; // OPRAVENO
    Token     current_token;

    // AI - UPDATED: pending token místo first_token
    if (pending != NULL) {
        current_token = *pending; // identifikátor už načetl parser
        use_saved     = true;     // *tok přijde na řadu potom
    } else {
        current_token = *tok; // první token výrazu
    }

    while (1) {
        // Získá nejvyšší terminál ze zásobníku a určí prioritu
        Stack_Item top_terminal = get_top_terminal();
        prec_idx_t row_idx      = top_terminal.prec_idx;

        // Index do precedenční tabulky pro aktuální token
        prec_idx_t column_idx = get_precedence_index(current_token.type, prev_token_type);

        if (column_idx == PREC_IDX_NONE) {
            column_idx = PREC_IDX_EOF; // nerozpoznané tokeny se berou jako konec výrazu
        }

        // PODMÍNKA UKONČENÍ
        bool is_stop_token = (column_idx == PREC_IDX_EOF || current_token.type == TOKEN_RPAREN);

        if (is_stop_token && top_terminal.prec_idx == PREC_IDX_EOF) {
            // AI - UPDATED: relativně k base (vnořené argumenty volání)
            if (stack_top == base + 1 && stack[base + 1].type == SYM_NON_TERMINAL) {
                break; // KONEC CYKLU WHILE
            } else {
                fprintf(stderr, "Syntax error: invalid or empty expression\n");
                rc = 2;     // AI - UPDATED
                goto error; // AI - UPDATED
            }
        }

        // pokud jsme stále tady, pokračuje se podle tabulky
        char precedence_relation = prec_table[row_idx][column_idx];

        // posun (shift)
        if (precedence_relation == '<') {
            insert_shift_marker();
            Stack_Item new_item = {
                .type       = SYM_TERMINAL,
                .token_type = current_token.type,
                .node       = NULL, // AI - UPDATED: typ se určí až při redukci
                .prec_idx   = column_idx,
                .token      = current_token // uloží lexém
            };

            push(new_item);
            prev_token_type = current_token.type;

            // AI - UPDATED: read_token místo get_next_token (nejdřív pending
            // token)
            if ((rc = read_token(&current_token, tok, &use_saved)) != 0) {
                fprintf(stderr, "%s error while getting the next token\n",
                        rc == 1 ? "Lexical" : "Internal");
                current_token.type = TOKEN_EOF; // atributy jsou neplatné
                goto error;
            }

            // AI - UPDATED: identifikátor následovaný '(' je volání funkce (FUNEXP)
            if (new_item.token_type == TOKEN_IDENTIFIER && current_token.type == TOKEN_LPAREN) {
                if ((rc = parse_call(st, stack_top, &current_token)) != 0) {
                    goto error;
                }
            }
            if ((column_idx == PREC_IDX_VALUE) &&
                (rc = parse_members(st, stack_top, &current_token)) != 0)
                goto error;

            // shoda (párování závorek)
        } else if (precedence_relation == '=') {
            Stack_Item new_item = {.type       = SYM_TERMINAL,
                                   .token_type = current_token.type,
                                   .node       = NULL, // AI - UPDATED
                                   .prec_idx   = column_idx,
                                   .token      = current_token};

            push(new_item);
            prev_token_type = current_token.type;

            // AI - UPDATED: read_token místo get_next_token (nejdřív pending
            // token)
            if ((rc = read_token(&current_token, tok, &use_saved)) != 0) {
                fprintf(stderr, "%s error while getting the next token\n",
                        rc == 1 ? "Lexical" : "Internal");
                current_token.type = TOKEN_EOF; // atributy jsou neplatné
                goto error;
            }

            // redukce
        } else if (precedence_relation == '>') {
            pop_count               = 0; // AI - UPDATED: popped_items deklarované na začátku
            bool found_shift_marker = false;

            // AI - UPDATED: nikdy neodebírá pod tento výraz
            while (stack_top > base) {
                Stack_Item item = pop();
                if (item.type == SYM_SHIFT_MARKER) {
                    found_shift_marker = true;
                    break;
                }
                if (pop_count < 4) {
                    popped_items[pop_count] = item;
                    pop_count++;
                } else {
                    fprintf(stderr, "Syntax error: too many items to reduce\n");
                    free_item(&item); // AI - UPDATED
                    rc = 2;           // AI - UPDATED
                    goto error;       // AI - UPDATED
                }
            }

            if (!found_shift_marker) {
                fprintf(stderr, "Syntax error: no shift marker found during reduction\n");
                rc = 2;     // AI - UPDATED
                goto error; // AI - UPDATED
            }

            // AI - UPDATED: výsledkem každého pravidla je jeden neterminál s uzlem
            Stack_Item new_item = {.type = SYM_NON_TERMINAL, .token_type = TOKEN_EOF, .node = NULL};

            // E -> id / hodnota
            if (pop_count == 1 && popped_items[0].type == SYM_TERMINAL) {
                // AI - UPDATED: listový uzel místo printf PUSHS
                rc = make_term_node(st, &popped_items[0], &new_item.node);
                if (rc != 0) {
                    goto error;
                }
                push(new_item);

                // 2: E -> E op E
            } else if (pop_count == 3 && popped_items[0].type == SYM_NON_TERMINAL &&
                       popped_items[1].type == SYM_TERMINAL &&
                       popped_items[2].type == SYM_NON_TERMINAL) {

                // AI - UPDATED: binární uzel místo printf, typy řeší
                // sem_binary_op
                Stack_Item *op_item = &popped_items[1];
                if (op_item->prec_idx != PREC_IDX_ADD_SUB &&
                    op_item->prec_idx != PREC_IDX_MUL_DIV &&
                    op_item->prec_idx != PREC_IDX_RELATION && op_item->prec_idx != PREC_IDX_EQ) {
                    fprintf(stderr, "Syntax error: expected binary operator\n");
                    rc = 2;
                    goto error;
                }

                ExprOp    op;   // výsledný operátor (DIV/IDIV, ADD/CONCAT ...)
                data_type type; // typ výsledku
                // levý = popped_items[2], pravý = popped_items[0]; může dostat INT2FLOAT
                rc = sem_binary_op(op_item->token_type, &popped_items[2].node,
                                   &popped_items[0].node, &op, &type);
                if (rc != 0) {
                    goto error;
                }

                new_item.node = expr_new(EXPR_BINARY, type);
                if (new_item.node == NULL) {
                    fprintf(stderr, "Internal error (99): out of memory\n");
                    rc = 99;
                    goto error;
                }
                new_item.node->u.bin.op    = op;
                new_item.node->u.bin.left  = popped_items[2].node; // vlastnictví -> uzel
                new_item.node->u.bin.right = popped_items[0].node;
                popped_items[2].node       = NULL;
                popped_items[0].node       = NULL;
                push(new_item);

                // 3: E -> ( E )
            } else if (pop_count == 3 && popped_items[0].type == SYM_TERMINAL &&
                       popped_items[0].token_type == TOKEN_RPAREN &&
                       popped_items[1].type == SYM_NON_TERMINAL &&
                       popped_items[2].type == SYM_TERMINAL &&
                       popped_items[2].token_type == TOKEN_LPAREN) {

                // Závorky negenerují kód, pouze posílají non-terminál dál
                new_item.node        = popped_items[1].node; // AI - UPDATED: předá podstrom
                popped_items[1].node = NULL;                 // AI - UPDATED
                push(new_item);

                // AI - UPDATED: nové pravidlo E -> ( ), literál Unit
                // E -> ( )   literál Unit
            } else if (pop_count == 2 && popped_items[0].type == SYM_TERMINAL &&
                       popped_items[0].token_type == TOKEN_RPAREN &&
                       popped_items[1].type == SYM_TERMINAL &&
                       popped_items[1].token_type == TOKEN_LPAREN) {

                new_item.node = expr_new(EXPR_UNIT_LIT, type_unit);
                if (new_item.node == NULL) {
                    fprintf(stderr, "Internal error (99): out of memory\n");
                    rc = 99;
                    goto error;
                }
                push(new_item);

                // 4: E -> unární_op E
            } else if (pop_count == 2 && popped_items[0].type == SYM_NON_TERMINAL &&
                       popped_items[1].type == SYM_TERMINAL &&
                       popped_items[1].prec_idx == PREC_IDX_UNARY) { // AI - UPDATED

                // AI - UPDATED: kontrola typu, uzel EXPR_NEG místo printf
                // jen čísla (6)
                if ((rc = sem_unary_op(popped_items[0].node)) != 0) {
                    goto error;
                }

                // AI - UPDATED: minus u literálu se rovnou vyhodnotí, -2147483648 zůstane platné
                ExprNode *operand = popped_items[0].node;
                if (popped_items[1].token_type == TOKEN_MINUS && operand->kind == EXPR_INT_LIT) {
                    operand->u.i_val = (int)(-(long long)operand->u.i_val);
                    new_item.node    = operand;
                } else if (popped_items[1].token_type == TOKEN_MINUS &&
                           operand->kind == EXPR_DOUBLE_LIT) {
                    operand->u.d_val = -operand->u.d_val;
                    new_item.node    = operand;
                } else if (popped_items[1].token_type == TOKEN_MINUS) {
                    new_item.node = expr_new(EXPR_NEG, popped_items[0].node->type);
                    if (new_item.node == NULL) {
                        fprintf(stderr, "Internal error (99): out of memory\n");
                        rc = 99;
                        goto error;
                    }
                    new_item.node->u.operand = popped_items[0].node; // vlastnictví
                } else {
                    new_item.node = popped_items[0].node; // unární plus: bez uzlu
                }
                popped_items[0].node = NULL;
                push(new_item);

            } else {
                fprintf(stderr, "Syntax error: invalid reduction pattern\n");
                rc = 2;     // AI - UPDATED
                goto error; // AI - UPDATED
            }
            pop_count = 0; // AI: odebrané položky jsou spotřebované
            if ((rc = parse_members(st, stack_top, &current_token)) != 0)
                goto error;

        } else if (precedence_relation == 'E') {
            fprintf(stderr, "Syntax error: unrecognized precedence relation\n");
            rc = 2;     // AI - UPDATED
            goto error; // AI - UPDATED
        }
    } // KONEC CYKLU WHILE (1)

    // AI - UPDATED: vrací strom, návěští error uvolní vše
    // AI - UPDATED: strom je na base + 1, vnější výraz zůstává na zásobníku
    *out                 = stack[base + 1].node; // hotový strom ($ E na zásobníku)
    stack[base + 1].node = NULL;
    stack_top            = base - 1;
    // dynamický zásobník
    if (base == 0) {
        release_stack();
    }
    *tok = current_token; // uloží zbývající token, který ukončil analýzu
    return 0;             // úspěšná analýza

error:
    for (int i = 0; i < pop_count; i++) {
        free_item(&popped_items[i]); // položky nedokončené redukce
    }
    // AI - UPDATED: maže jen položky tohoto výrazu
    stack_clear(base);    // vše, co zůstalo na zásobníku
    *tok = current_token; // token, na kterém analýza skončila, vlastní volající
    // dynamický zásobník
    if (base == 0) {
        release_stack();
    }
    return rc;
}
