/**

@par         IFJ26
@file        exp_parser.h
@authors     Jakub Grásgruber <xgrasgj00>
@brief       Rozhraní precedenční syntaktické analýzy výrazů.
*/

#ifndef EXP_PARSER_H
#define EXP_PARSER_H

#include "expr_tree.h" // AI - UPDATED: výsledkem je strom výrazu
#include "scanner.h"
#include "symtable.h"

// prompt: "Podívej se na origin/development (src/parser.c, src/scanner.h,
//          src/symtable.h). Uprav parse_expression tak, aby místo výpisu kódu
//          vracela strom výrazu a aby seděla k lookaheadu, který používá parser
//          (identifikátor už načtený v CALL_OR_EXP). Nejdřív navrhni rozhraní."
// AI - UPDATED: bylo int parse_expression(hashtable *ts, Token *next_token,
//               Token *next_token) (duplicitní parametr, nešlo přeložit)
/**
 * @brief Zpracuje výraz precedenční analýzou a postaví typovaný strom.
 *
 * @param st      tabulka symbolů pro vyhledávání proměnných
 * @param pending NULL, nebo token, který už parser načetl (identifikátor
 *                v CALL_OR_EXP -> IDENTIFIER CALL_OR_EXP_OPT); jeho s_val se převezme
 * @param tok     vstup: aktuální lookahead (první token za @p pending),
 *                výstup: první token, který už do výrazu NEPATŘÍ (vlastní ho volající)
 * @param out     při úspěchu strom výrazu (volající ho uvolní funkcí expr_free),
 *                při chybě NULL
 * @return 0 při úspěchu, jinak kód chyby
 *         (1 lexikální, 2 syntaktická, 3 nedefinovaná proměnná, 6 typová, 99 interní)
 */
int parse_expression(SymTable *st, Token *pending, Token *tok, ExprNode **out);

#endif // EXP_PARSER_H
