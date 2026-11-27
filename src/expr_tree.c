/**
 * @par Projekt
 * Implementace překladače imperativního jazyka IFJ26
 * @file        expr_tree.c
 * @authors     Jakub Grásgruber <xgrasgj00>
 * @brief       Vytváření, uvolňování a ladicí výpis uzlů stromu výrazu.
 */

#include "expr_tree.h"
#include <stdlib.h>

// prompt: "Funkce, která alokuje jeden uzel stromu, při chybě alokace má
//          volající nahlásit chybu 99."
/**
 * @brief Vytvoří nový uzel výrazu daného druhu a typu se všemi položkami vynulovanými.
 *
 * @param kind druh uzlu
 * @param type typ výsledku uzlu
 * @return nový uzel, nebo NULL při chybě alokace
 */
ExprNode *expr_new(ExprKind kind, data_type type) {
    ExprNode *node = calloc(1, sizeof(ExprNode)); // vynulováno -> ukazatele jsou NULL
    if (node == NULL) {
        return NULL; // volající nahlásí chybu 99
    }
    node->kind = kind;
    node->type = type;
    return node;
}

// prompt: "Funkce, která uvolní celý strom, uzly vlastní řetězce převzaté
//          z tokenů, takže uvolni i je."
/**
 * @brief Rekurzivně uvolní celý strom včetně vlastněných řetězců a identifikátorů.
 *
 * @param node kořen uvolňovaného stromu, NULL je povolen
 */
void expr_free(ExprNode *node) {
    if (node == NULL) {
        return;
    }
    switch (node->kind) {
    case EXPR_STRING_LIT:
        free(node->u.s_val); // vlastněná hodnota řetězce
        break;
    case EXPR_VAR:
        free(node->u.var.name); // vlastněný identifikátor
        break;
    case EXPR_BINARY:
        expr_free(node->u.bin.left); // rekurzivně uvolní oba operandy
        expr_free(node->u.bin.right);
        break;
    case EXPR_NEG:
    case EXPR_INT2FLOAT:
        expr_free(node->u.operand); // jediný operand
        break;
    // AI - UPDATED: argumenty a jméno volané funkce
    case EXPR_MEMBER:
        expr_free(node->u.member.receiver);
        free(node->u.member.name);
        break;
    case EXPR_CALL:
        for (size_t i = 0; i < node->u.call.argc; i++) {
            expr_free(node->u.call.args[i]);
        }
        free(node->u.call.args);
        free(node->u.call.name);
        break;
    default:
        break; // literály bez dat na haldě
    }
    free(node);
}

// prompt: "Ladicí výpis stromu, abych mohl zkontrolovat výsledek parseru
//          na testovacích vstupech bez generátoru."
// AI: type_name / op_name jsou jen pomocné funkce pro expr_dump
/**
 * @brief Vrátí jméno typu pro ladicí výpis.
 *
 * @param type typ
 * @return textové jméno typu
 */
static const char *type_name(data_type type) {
    switch (type) {
    case type_int:
        return "int";
    case type_double:
        return "double";
    case type_string:
        return "string";
    case type_unit:
        return "unit";
    case type_bool:
        return "bool";
    case type_any:
        return "any";
    default:
        return "unknown";
    }
}

/**
 * @brief Vrátí jméno operátoru pro ladicí výpis.
 *
 * @param op operátor
 * @return textové jméno operátoru
 */
static const char *op_name(ExprOp op) {
    static const char *names[] = {"+",  "-",  "*", "/",  "idiv", "concat",
                                  "==", "!=", "<", "<=", ">",    ">="};
    return names[op];
}

// AI: např. "i + d" -> (+ double (int2float (var i$0 int)) (var d$1 double))
/**
 * @brief Vypíše strom v prefixovém tvaru.
 *
 * @param node kořen vypisovaného stromu
 * @param out  výstupní proud
 */
void expr_dump(const ExprNode *node, FILE *out) {
    if (node == NULL) {
        fprintf(out, "(null)");
        return;
    }
    switch (node->kind) {
    case EXPR_INT_LIT:
        fprintf(out, "%d", node->u.i_val);
        break;
    case EXPR_DOUBLE_LIT:
        fprintf(out, "%g", node->u.d_val);
        break;
    case EXPR_STRING_LIT:
        fprintf(out, "\"%s\"", node->u.s_val);
        break;
    case EXPR_UNIT_LIT:
        fprintf(out, "()");
        break;
    case EXPR_VAR:
        fprintf(out, "(var %s$%u %s)", node->u.var.name, node->u.var.id, type_name(node->type));
        break;
    case EXPR_BINARY:
        fprintf(out, "(%s %s ", op_name(node->u.bin.op), type_name(node->type));
        expr_dump(node->u.bin.left, out);
        fprintf(out, " ");
        expr_dump(node->u.bin.right, out);
        fprintf(out, ")");
        break;
    case EXPR_NEG:
        fprintf(out, "(neg %s ", type_name(node->type));
        expr_dump(node->u.operand, out);
        fprintf(out, ")");
        break;
    case EXPR_INT2FLOAT:
        fprintf(out, "(int2float ");
        expr_dump(node->u.operand, out);
        fprintf(out, ")");
        break;
    // AI - UPDATED: volání funkce
    case EXPR_MEMBER:
        fprintf(out, "(member %s ", node->u.member.name);
        expr_dump(node->u.member.receiver, out);
        fprintf(out, ")");
        break;
    case EXPR_CALL:
        fprintf(out, "(call %s %s", node->u.call.name, type_name(node->type));
        for (size_t i = 0; i < node->u.call.argc; i++) {
            fprintf(out, " ");
            expr_dump(node->u.call.args[i], out);
        }
        fprintf(out, ")");
        break;
    }
}
