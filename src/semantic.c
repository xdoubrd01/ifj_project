/**

@par         IFJ26
@file        semantic.c
@authors     Jakub Grásgruber <xgrasgj00>
@brief       Sémantické kontroly proměnných, volání a výrazů včetně implicitního převodu Int na Double.
*/

#include "semantic.h"
#include <stdio.h>
#include <stdlib.h>

// prompt: "Int -> Double je jediný implicitní převod, generátor ho potřebuje
//          mít ve stromu explicitně (INT2FLOATS), přidej ho tam, kde musí být
//          oba operandy čísla."
// AI: pomocné funkce pro sem_binary_op / sem_unary_op / sem_coerce_expr
/**
 * @brief Zjistí, jestli je typ číselný.
 *
 * @param type testovaný typ
 * @return true pro Int nebo Double
 */
static bool is_numeric(data_type type) { return type == type_int || type == type_double; }

/**
 * @brief Obalí uzel typu Int do EXPR_INT2FLOAT, uzel typu Double nechá beze změny.
 *
 * @param node ukazatel na převáděný uzel, po převodu ukazuje na nový uzel
 * @return 0 při úspěchu, 99 při chybě alokace
 */
static int to_double(ExprNode **node) {
    if ((*node)->type != type_int) {
        return 0; // už je double
    }
    ExprNode *conv = expr_new(EXPR_INT2FLOAT, type_double);
    if (conv == NULL) {
        fprintf(stderr, "Internal error (99): out of memory\n");
        return 99;
    }
    conv->u.operand = *node; // původní uzel se stane operandem
    *node           = conv;  // volající teď drží uzel převodu
    return 0;
}

// prompt: "Uprav sem_declare_variable pro SymTable: redefinice jen ve stejném
//          bloku (chyba 4), typ lze vynechat a pak se vezme z inicializace,
//          uvedený typ ji musí přijmout (Int -> Double)."
// AI - UPDATED: hashtable* -> SymTable*, exit() -> návratový kód, přidáno
//               odvození typu a kontrola kompatibility, out_id pro generátor
/**
 * @brief Definuje novou proměnnou (val / var) v aktuálním rozsahu.
 *
 * @param st            tabulka symbolů
 * @param var_name      jméno proměnné
 * @param declared_type typ uvedený za ':', nebo type_unknown, pokud chybí
 * @param value_type    typ inicializace
 * @param is_mutable    true pro var, false pro val
 * @param out_id        výstup: jedinečné id proměnné, může být NULL
 * @return 0 při úspěchu, 4 redefinice, 6 typová chyba, 99 interní chyba
 */
int sem_declare_variable(SymTable *st, const char *var_name, data_type declared_type,
                         data_type value_type, bool is_mutable, unsigned *out_id) {
    // redefinice jen ve stejném bloku, překrytí vnější proměnné je povolené
    if (symtable_search_current_var(st, var_name) != NULL) {
        fprintf(stderr, "Semantic error(4): Variable '%s' is already defined\n", var_name);
        return 4;
    }

    data_type final_type = declared_type;
    if (declared_type == type_unknown) {
        // typ se odvodí z inicializace; Boolean nejde uložit (bez BOOLTHEN)
        if (value_type != type_int && value_type != type_double && value_type != type_string &&
            value_type != type_unit) {
            fprintf(stderr, "Semantic error(6): Cannot infer type of '%s'\n", var_name);
            return 6;
        }
        final_type = value_type;
    } else if (!symtable_type_compatible(declared_type, value_type)) {
        // uvedený typ musí přijmout inicializaci (Int -> Double je povolený)
        fprintf(stderr, "Semantic error(6): Incompatible initializer of '%s'\n", var_name);
        return 6;
    }

    if (!symtable_insert_var(st, var_name, final_type, is_mutable)) {
        fprintf(stderr, "Internal error (99): cannot insert '%s'\n", var_name);
        return 99; // duplicita se kontrolovala výše -> chyba alokace
    }
    if (out_id != NULL) {
        // id nového záznamu, generátor ho použije v DEFVAR
        *out_id = symtable_search_current_var(st, var_name)->data.var.id;
    }
    return 0;
}

// prompt: "Uprav sem_get_variable_type pro SymTable, proměnná musí být
//          viditelná (3), neznámý typ je chyba (6)."
// AI - UPDATED: symtable_search -> symtable_search_var (překrytí),
//               type_unknown -> chyba 6, vrací kód, typ přes out_type
/**
 * @brief Vrátí typ viditelné proměnné použité ve výrazu.
 *
 * @param st       tabulka symbolů
 * @param var_name jméno proměnné
 * @param out_type výstup: typ proměnné
 * @param out_id   výstup: jedinečné id proměnné, může být NULL
 * @return 0 při úspěchu, 3 nedefinovaná proměnná, 6 neznámý typ
 */
int sem_get_variable_type(SymTable *st, const char *var_name, data_type *out_type,
                          unsigned *out_id) {
    // nejbližší viditelná proměnná nebo parametr (respektuje překrytí)
    symbol_data *found = symtable_search_var(st, var_name);
    if (found == NULL) {
        fprintf(stderr, "Semantic error(3): Variable '%s' is not defined\n", var_name);
        return 3;
    }

    if (found->data.var.var_type == type_unknown) {
        // typ nešlo odvodit -> chyba 6
        fprintf(stderr, "Semantic error(6): Type of '%s' is unknown\n", var_name);
        return 6;
    }

    *out_type = found->data.var.var_type;
    if (out_id != NULL) {
        *out_id = found->data.var.id; // která z překrytých proměnných
    }
    return 0;
}

// prompt: "sem_check_assignment je deklarovaná v semantic.h, ale chybí
//          implementace, doplň ji: proměnná musí existovat (3), do val
//          a parametrů nejde přiřadit (7), typ proměnné musí přijmout
//          hodnotu (6)."
/**
 * @brief Zkontroluje přiřazení 'jméno = hodnota' do existující proměnné.
 *
 * @param st         tabulka symbolů
 * @param var_name   jméno cílové proměnné
 * @param value_type typ přiřazované hodnoty
 * @param out_id     výstup: jedinečné id cílové proměnné, může být NULL
 * @return 0 při úspěchu, 3 nedefinovaná proměnná, 6 typová chyba,
 *         7 zápis do nemodifikovatelné proměnné nebo parametru
 */
int sem_check_assignment(SymTable *st, const char *var_name, data_type value_type,
                         unsigned *out_id) {
    symbol_data *found = symtable_search_var(st, var_name);
    if (found == NULL) {
        fprintf(stderr, "Semantic error(3): Variable '%s' is not defined\n", var_name);
        return 3;
    }
    if (!found->data.var.is_mutable || found->data.var.is_parameter) {
        // val nebo parametr funkce
        fprintf(stderr, "Semantic error(7): '%s' is not mutable\n", var_name);
        return 7;
    }
    if (!symtable_type_compatible(found->data.var.var_type, value_type)) {
        // typ proměnné se nikdy nemění
        fprintf(stderr, "Semantic error(6): Incompatible assignment to '%s'\n", var_name);
        return 6;
    }
    if (out_id != NULL) {
        *out_id = found->data.var.id; // cílová proměnná pro MOVE
    }
    return 0;
}

// prompt: "Při přiřazení výrazu typu Int do proměnné typu Double musí být
//          převod také ve stromu, přidej pro parser funkci."
/**
 * @brief Převede celý výraz na cílový typ (jen Int -> Double).
 *
 * @param expr   ukazatel na kořen výrazu, po převodu ukazuje na nový kořen
 * @param target cílový typ
 * @return 0 při úspěchu, 6 nekompatibilní typ, 99 chyba alokace
 */
int sem_coerce_expr(ExprNode **expr, data_type target) {
    if ((*expr)->type == target) {
        return 0; // není co dělat
    }
    if (target == type_double && (*expr)->type == type_int) {
        return to_double(expr); // jediný implicitní převod
    }
    fprintf(stderr, "Semantic error(6): Incompatible expression type\n");
    return 6;
}

// prompt: "Podmínka if / while musí být relační výraz, přidej kontrolu, kterou
//          může volat parser (Boolean existuje jen v podmínkách)."
/**
 * @brief Zkontroluje, že podmínka if / while je relace (type_bool).
 *
 * @param cond strom podmínky
 * @return 0 při úspěchu, 6 pokud podmínka není relace
 */
int sem_check_condition(const ExprNode *cond) {
    if (cond->type != type_bool) {
        // if / while potřebuje relační výraz
        fprintf(stderr, "Semantic error(6): Condition is not a relation\n");
        return 6;
    }
    return 0;
}

// prompt: "Přidej sémantickou kontrolu volání funkcí uvnitř výrazů:
//          nedefinovaná funkce (3), špatný počet nebo typ argumentů (5),
//          Boolean nejde předat (6), převod argumentů Int -> Double ve stromu."
/**
 * @brief Zkontroluje volání funkce a převede argumenty na typy parametrů.
 *
 * @param st       tabulka symbolů
 * @param name     jméno volané funkce
 * @param args     stromy argumentů, mohou být nahrazeny uzlem převodu
 * @param argc     počet argumentů
 * @param out_type výstup: návratový typ funkce
 * @return 0 při úspěchu, 3 nedefinovaná funkce, 5 špatný počet nebo typ
 *         argumentů, 6 argument typu Boolean, 99 chyba alokace
 */
int sem_check_call(SymTable *st, const char *name, ExprNode **args, size_t argc,
                   data_type *out_type) {
    symbol_data *func = symtable_search_func(st, name);
    if (func == NULL) {
        fprintf(stderr, "Semantic error(3): Function '%s' is not defined\n", name);
        return 3;
    }
    if (argc != func->data.func.param_count) {
        fprintf(stderr, "Semantic error(5): Wrong argument count of '%s'\n", name);
        return 5;
    }
    for (size_t i = 0; i < argc; i++) {
        data_type expected = func->data.func.params[i];
        // AI - UPDATED: špatný typ u parametru s uvedeným typem je 5, kontroluje se první
        if (!symtable_type_compatible(expected, args[i]->type)) {
            fprintf(stderr, "Semantic error(5): Wrong argument type of '%s'\n", name);
            return 5;
        }
        if (args[i]->type == type_bool) {
            // relaci lze použít jen přímo v if / while
            fprintf(stderr, "Semantic error(6): Boolean argument of '%s'\n", name);
            return 6;
        }
        if (expected != type_any) {
            int rc = sem_coerce_expr(&args[i], expected); // Int -> Double
            if (rc != 0) {
                return rc;
            }
        }
    }
    *out_type = func->data.func.ret_type;
    return 0;
}

// prompt: "Kontrola operandu unárního + / -, povolená jsou jen čísla (6)."
/**
 * @brief Zkontroluje operand unárního + / -.
 *
 * @param operand strom operandu
 * @return 0 při úspěchu, 6 pokud operand není číslo
 */
int sem_unary_op(const ExprNode *operand) {
    if (!is_numeric(operand->type)) {
        // unární + / - jen pro čísla
        fprintf(stderr, "Semantic error(6): Unary operator on non-numeric type\n");
        return 6;
    }
    return 0;
}

// prompt: "Uprav sem_get_binary_op_result pro strom: vlož převod Int -> Double
//          do operandů a rozhodni tady DIV / IDIV a ADD / CONCAT, aby generátor
//          dostal výsledný operátor."
// AI - UPDATED: bylo sem_get_binary_op_result (vracela jen typ), nyní může
//               obalit operandy do EXPR_INT2FLOAT a vrací ExprOp;
//               == / != nad Boolean, unknown a any je chyba 6
/**
 * @brief Zkontroluje binární operaci a určí její operátor a typ výsledku.
 *
 * @param op       token operátoru
 * @param left     levý operand, může být obalen do EXPR_INT2FLOAT
 * @param right    pravý operand, může být obalen do EXPR_INT2FLOAT
 * @param out_op   výstup: výsledný operátor
 * @param out_type výstup: typ výsledku
 * @return 0 při úspěchu, 6 nekompatibilní typy, 99 interní chyba
 */
int sem_binary_op(TokenType op, ExprNode **left, ExprNode **right, ExprOp *out_op,
                  data_type *out_type) {
    data_type left_type   = (*left)->type;
    data_type right_type  = (*right)->type;
    bool      are_numeric = is_numeric(left_type) && is_numeric(right_type);
    bool      has_double  = (left_type == type_double || right_type == type_double);
    int       rc          = 0;

    if (op == TOKEN_PLUS || op == TOKEN_MINUS || op == TOKEN_MUL || op == TOKEN_DIV) {
        // String + String -> konkatenace
        if (op == TOKEN_PLUS && left_type == type_string && right_type == type_string) {
            *out_op   = OP_CONCAT;
            *out_type = type_string;
            return 0;
        }
        if (!are_numeric) {
            fprintf(stderr, "Semantic error(6): Incompatible types for arithmetic "
                            "operation.\n");
            return 6;
        }
        if (has_double) {
            // jeden Double -> druhý se převede, výsledek je Double
            if ((rc = to_double(left)) != 0 || (rc = to_double(right)) != 0) {
                return rc;
            }
            *out_type = type_double;
        } else {
            *out_type = type_int; // Int op Int -> Int
        }

        if (op == TOKEN_PLUS) {
            *out_op = OP_ADD;
        } else if (op == TOKEN_MINUS) {
            *out_op = OP_SUB;
        } else if (op == TOKEN_MUL) {
            *out_op = OP_MUL;
        } else {
            *out_op = has_double ? OP_DIV : OP_IDIV; // Int / Int je celočíselné dělení
        }
        return 0;
    }

    if (op == TOKEN_LT || op == TOKEN_LEQ || op == TOKEN_GT || op == TOKEN_GEQ || op == TOKEN_EQ ||
        op == TOKEN_NEQ) {
        bool is_equality = (op == TOKEN_EQ || op == TOKEN_NEQ);
        bool are_strings = (left_type == type_string && right_type == type_string);
        // == / != i pro stejné typy (Unit); Boolean bez BOOLTHEN není hodnota,
        // unknown není platný nikdy
        bool are_same = (left_type == right_type && left_type != type_bool &&
                         left_type != type_unknown && left_type != type_any);

        if (are_numeric) {
            // čísla se porovnávají s převodem Int -> Double
            if (has_double && ((rc = to_double(left)) != 0 || (rc = to_double(right)) != 0)) {
                return rc;
            }
        } else if (!are_strings && !(is_equality && are_same)) {
            fprintf(stderr, "Semantic error(6): Incompatible types for relational "
                            "operation.\n");
            return 6;
        }

        *out_type = type_bool; // návratová hodnota
        switch (op) {
        case TOKEN_LT:
            *out_op = OP_LT;
            break;
        case TOKEN_LEQ:
            *out_op = OP_LEQ;
            break;
        case TOKEN_GT:
            *out_op = OP_GT;
            break;
        case TOKEN_GEQ:
            *out_op = OP_GEQ;
            break;
        case TOKEN_EQ:
            *out_op = OP_EQ;
            break;
        default:
            *out_op = OP_NEQ;
            break;
        }
        return 0;
    }

    fprintf(stderr, "Internal Error (99): Unknown operator in semantic analysis.\n");
    return 99;
}
