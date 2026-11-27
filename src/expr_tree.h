/**
 * @par Projekt
 * Implementace překladače imperativního jazyka IFJ26
 * @file        expr_tree.h
 * @authors     Jakub Grásgruber <xgrasgj00>
 * @brief       Typovaný strom výrazu, který předává výrazový parser generátoru kódu.
 */

#ifndef EXPR_TREE_H
#define EXPR_TREE_H

#include "symtable.h"
#include <stdio.h>

// prompt: "exp_parser.c vypisuje IFJcode přes printf, chci, aby stavěl strom
//          výrazu s typy pro kolegu, který dělá generátor kódu. Než napíšeš
//          jakýkoli kód, nejdřív navrhni rozhraní."
/*
 * Strom výrazu staví precedenční parser (exp_parser.c).
 * Každý uzel už má typ výsledku zkontrolovaný sémantickou analýzou.
 *
 * Záruky pro generátor kódu:
 *  - oba operandy EXPR_BINARY mají vždy stejný typ
 *    (převod int -> double je explicitní uzel EXPR_INT2FLOAT)
 *  - DIV / IDIV a ADD / CONCAT jsou už rozhodnuté
 *  - závorky a unární plus nevytváří žádný uzel
 *  - výsledek relačního operátoru má typ type_bool
 *  - EXPR_VAR nese u.var.id z tabulky symbolů: stejné jméno v různých
 *    rozsazích (překrytí) má jiné id, např. použij LF@name$id
 *  - AI - UPDATED: EXPR_CALL vkládá argumenty zleva doprava, potom CALL;
 *    na datovém zásobníku zůstane právě jeden výsledek. Převod Int -> Double
 *    u argumentů už je ve stromu.
 */

/** @brief Druh uzlu. */
typedef enum {
    EXPR_INT_LIT,    ///< literál Int              -> u.i_val
    EXPR_DOUBLE_LIT, ///< literál Double           -> u.d_val
    EXPR_STRING_LIT, ///< literál String           -> u.s_val (surová hodnota, bez escape sekvencí)
    EXPR_UNIT_LIT,   ///< literál Unit ()          -> bez dat
    EXPR_VAR,        ///< proměnná / parametr      -> u.var (jméno + id)
    EXPR_BINARY,     ///< binární operace          -> u.bin
    EXPR_NEG,        ///< unární minus             -> u.operand
    EXPR_INT2FLOAT,  ///< implicitní Int -> Double -> u.operand
    // AI - UPDATED: volání funkce (FUNEXP)
    EXPR_MEMBER, ///< příjemce.člen -> u.member
    EXPR_CALL    ///< volání funkce             -> u.call
} ExprKind;

/** @brief Výsledný binární operátor. */
typedef enum {
    OP_ADD,    ///< int + int, double + double         -> ADDS
    OP_SUB,    ///< -                                  -> SUBS
    OP_MUL,    ///< *                                  -> MULS
    OP_DIV,    ///< double / double                    -> DIVS
    OP_IDIV,   ///< int / int                          -> IDIVS
    OP_CONCAT, ///< string + string                    -> CONCAT
    OP_EQ,     ///< ==                                 -> EQS
    OP_NEQ,    ///< !=                                 -> EQS + NOTS
    OP_LT,     ///< <                                  -> LTS
    OP_LEQ,    ///< <=                                 -> GTS + NOTS
    OP_GT,     ///< >                                  -> GTS
    OP_GEQ     ///< >=                                 -> LTS + NOTS
} ExprOp;

// prompt: "Proměnná má ve stromu jen jméno, takže generátor nerozliší
//          překryté proměnné. Přidej do symtable jedinečné id a předej ho
//          do stromu."
// prompt: "Kolega potřebuje volání funkcí uvnitř výrazů, např.
//          val x = getValue() + 1, print(2 * getValue()) nebo
//          return getValue() + 1. Navrhni, jak to přidat do mé části bez
//          změny precedenční tabulky."
/** @brief Jeden uzel stromu výrazu. */
typedef struct ExprNode {
    ExprKind  kind; ///< druh uzlu
    data_type type; ///< typ výsledku uzlu
    union {
        struct {
            struct ExprNode *receiver;
            char            *name;
            unsigned         operation;
        } member;
        int    i_val;      ///< EXPR_INT_LIT
        double d_val;      ///< EXPR_DOUBLE_LIT
        char  *s_val;      ///< EXPR_STRING_LIT (vlastní ho uzel)
        struct {           // AI - UPDATED: bylo char *var_name, přidáno id kvůli překrytí
            char    *name; ///< jméno proměnné (vlastní ho uzel)
            unsigned id;   ///< jedinečné id z tabulky symbolů (data.var.id)
        } var;             ///< EXPR_VAR
        struct {
            ExprOp           op;    ///< operátor
            struct ExprNode *left;  ///< levý operand (vyhodnocuje se první)
            struct ExprNode *right; ///< pravý operand
        } bin;                      ///< EXPR_BINARY
        struct ExprNode *operand;   ///< EXPR_NEG, EXPR_INT2FLOAT
        // AI - UPDATED: volaná funkce a její argumenty (FUNEXP)
        struct {
            char             *name; ///< volaná funkce (vlastní ji uzel)
            struct ExprNode **args; ///< argumenty, vyhodnocují se zleva doprava (vlastněné)
            size_t            argc; ///< počet argumentů
        } call;                     ///< EXPR_CALL
    } u;
} ExprNode;

// prompt: "Rozhraní je v pořádku, klidně vytvoř samostatné soubory expr_tree.
//          Drž to co nejzákladnější a nejjednodušší, ale funkční."
/**
 * @brief Alokuje vynulovaný uzel daného druhu a typu.
 *
 * @param kind druh uzlu
 * @param type typ výsledku uzlu
 * @return nový uzel, nebo NULL při chybě alokace
 */
ExprNode *expr_new(ExprKind kind, data_type type);

/**
 * @brief Uvolní celý podstrom včetně vlastněných řetězců.
 *
 * @param node kořen podstromu, NULL je povolen
 */
void expr_free(ExprNode *node);

/**
 * @brief Ladicí výpis stromu v prefixovém tvaru, např. (+ int (var x$0 int) 1).
 *
 * @param node kořen vypisovaného stromu
 * @param out  výstupní proud
 */
void expr_dump(const ExprNode *node, FILE *out);

#endif // EXPR_TREE_H
