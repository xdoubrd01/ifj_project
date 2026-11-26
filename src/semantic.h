/**

@par         IFJ26
@file        semantic.h
@authors     Jakub Grásgruber <xgrasgj00>
@brief       Rozhraní sémantických kontrol, které volá parser a výrazový parser.
*/

#ifndef SEMANTIC_H
#define SEMANTIC_H

#include "expr_tree.h" // AI - UPDATED: uzly jsou potřeba kvůli implicitním převodům
#include "scanner.h"
#include "symtable.h"
#include <stdbool.h>

// prompt: "Uprav semantic.c pro aktuální symtable.h na origin/development
//          (SymTable, rozsahy), zachovej logiku mých funkcí."
// prompt: "Přidej jedinečné id proměnné (kvůli překrytí) a uprav moji část."
// AI - UPDATED: hashtable* -> SymTable*, void + exit() -> návratový kód,
//               nové funkce pro přiřazení, převod a podmínku
/*
 * Všechny funkce vrací 0 při úspěchu, jinak kód chyby ze zadání:
 *   3 = nedefinovaná proměnná, 4 = redefinice, 6 = typová chyba,
 *   7 = zápis do nemodifikovatelné proměnné / parametru, 99 = interní chyba.
 * Chybové hlášení se vypíše na stderr, volající jen předá kód dál.
 * out_id = jedinečné id proměnné (symtable data.var.id) pro generátor kódu,
 *          může být NULL, pokud není potřeba.
 */

// Funkce pro parser

/**
 * @brief Definuje proměnnou v aktuálním rozsahu.
 *
 * @param st            tabulka symbolů
 * @param var_name      jméno proměnné
 * @param declared_type typ uvedený za ':', nebo type_unknown, pokud chybí
 * @param value_type    typ inicializace (výraz nebo volání)
 * @param is_mutable    true pro var, false pro val
 * @param out_id        výstup: jedinečné id proměnné, může být NULL
 * @return 0 při úspěchu, jinak kód chyby
 */
int sem_declare_variable(SymTable *st, const char *var_name, data_type declared_type,
                         data_type value_type, bool is_mutable, unsigned *out_id);

/**
 * @brief Zkontroluje přiřazení 'var_name = hodnota' (proměnná existuje, je modifikovatelná, typ).
 *
 * Pokud je hodnotou strom výrazu, je potřeba potom zavolat sem_coerce_expr(),
 * aby se převod Int -> Double objevil ve stromu.
 *
 * @param st         tabulka symbolů
 * @param var_name   jméno cílové proměnné
 * @param value_type typ přiřazované hodnoty
 * @param out_id     výstup: jedinečné id cílové proměnné, může být NULL
 * @return 0 při úspěchu, jinak kód chyby
 */
int sem_check_assignment(SymTable *st, const char *var_name, data_type value_type,
                         unsigned *out_id);

/**
 * @brief Převede výraz na cílový typ (v případě potřeby vloží EXPR_INT2FLOAT).
 *
 * @param expr   ukazatel na kořen výrazu
 * @param target cílový typ
 * @return 0 při úspěchu, jinak kód chyby
 */
int sem_coerce_expr(ExprNode **expr, data_type target);

/**
 * @brief Podmínka if / while musí mít typ Boolean (relační výraz).
 *
 * @param cond strom podmínky
 * @return 0 při úspěchu, 6 pokud podmínka není relace
 */
int sem_check_condition(const ExprNode *cond);

// Funkce pro výrazový parser

/**
 * @brief Najde viditelnou proměnnou a uloží její typ do *out_type.
 *
 * @param st       tabulka symbolů
 * @param var_name jméno proměnné
 * @param out_type výstup: typ proměnné
 * @param out_id   výstup: jedinečné id proměnné, může být NULL
 * @return 0 při úspěchu, jinak kód chyby
 */
int sem_get_variable_type(SymTable *st, const char *var_name, data_type *out_type,
                          unsigned *out_id);

/**
 * @brief Zkontroluje binární operaci, může obalit operandy do EXPR_INT2FLOAT.
 *
 * @param op       token operátoru
 * @param left     levý operand
 * @param right    pravý operand
 * @param out_op   výstup: výsledný operátor
 * @param out_type výstup: typ výsledku
 * @return 0 při úspěchu, jinak kód chyby
 */
int sem_binary_op(TokenType op, ExprNode **left, ExprNode **right, ExprOp *out_op,
                  data_type *out_type);

/**
 * @brief Zkontroluje operand unárního + / -.
 *
 * @param operand strom operandu
 * @return 0 při úspěchu, 6 pokud operand není číslo
 */
int sem_unary_op(const ExprNode *operand);

// prompt: "Přidej sémantickou kontrolu volání funkcí uvnitř výrazů."
/**
 * @brief Zkontroluje volání funkce (3 nedefinovaná, 5 argumenty, 6 argument Boolean).
 *
 * Převede argumenty Int -> Double ve stromu a návratový typ uloží do *out_type.
 *
 * @param st       tabulka symbolů
 * @param name     jméno volané funkce
 * @param args     stromy argumentů
 * @param argc     počet argumentů
 * @param out_type výstup: návratový typ funkce
 * @return 0 při úspěchu, jinak kód chyby
 */
int sem_check_call(SymTable *st, const char *name, ExprNode **args, size_t argc,
                   data_type *out_type);

#endif // SEMANTIC_H
