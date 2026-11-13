// Project:    IFJ26
// Filename:   code_generator.c
// Author:     Michal Zednicek <xzednim00>

#include <stdio.h>
#include <stdbool.h>
#include "symtable.h"
#include "expr_tree.h"

// -----    functions to generate IFJ26code instructions    -----

// prompt:
// write functions in c to generate instructions on stdout.
// make multiple for 0 to 4 arguments. example:
// void gen_inst0(const char *inst) {
// printf("%s\n", inst);
// }

// AI START
void gen_inst0(const char *inst) {
    printf("%s\n", inst);
}

void gen_inst1(const char *inst, const char *arg1) {
    printf("%s %s\n", inst, arg1);
}

void gen_inst2(const char *inst, const char *arg1, const char *arg2) {
    printf("%s %s %s\n", inst, arg1, arg2);
}

void gen_inst3(const char *inst, const char *arg1, const char *arg2, const char *arg3) {
    printf("%s %s %s %s\n", inst, arg1, arg2, arg3);
}

void gen_inst4(const char *inst, const char *arg1, const char *arg2, const char *arg3, const char *arg4) {
    printf("%s %s %s %s %s\n", inst, arg1, arg2, arg3, arg4);
}
// AI END


// -----    constant data formatting functions      -----

// prompt:
// write fuknction to convert C float to this format:
// float@0x1.2666666666666p
// use %a format specifier for printf function

// AI START
void print_double(const double val) {
    printf("float@%a\n", val);
}

// prompt:
// do the same for those formats
// bool@true, nil@nil, int@-5
void print_bool(const bool val) {
    printf("bool@%s\n", val ? "true" : "false");
}

void print_nil(void) {
    printf("nil@nil\n");
}

void print_int(const int val) {
    printf("int@%d\n", val);
}
// AI END

// -----   built-in functions       -----

void print_string(const char *str) {
    printf("string@");
    if (str != NULL) {
        for (const char *p = str; *p != '\0'; p++) {
            unsigned char c = (unsigned char)*p;
            if (c <= 32 || c == 35 || c == 92) {
                printf("\\%03d", (unsigned char)*p);
            } else {
                putchar(*p);
            }
        }
    }
    printf("\n");
}

// -----    expr_tree process functions     -----

void process_expr_leaf(ExprNode *node) {
    switch (node->kind)
    {
    case EXPR_INT_LIT:
        printf("PUSHS ");
        print_int(node->u.i_val);
        break;
    
    case EXPR_DOUBLE_LIT:
        printf("PUSHS ");
        print_double(node->u.d_val);
        break;

    case EXPR_STRING_LIT:
        printf("PUSHS ");
        print_string(node->u.s_val);
        break;

    case EXPR_UNIT_LIT:
        printf("PUSHS ");
        print_nil();
        break;

    case EXPR_VAR:
        printf("PUSHS ");
        printf(node->u.var.id);
        break;

    // case EXPR_BINARY:
        // call post_order()?

    // case EXPR_NEG:
        // analyse node.u.type, proccess negation of data type (if int -> reverse operand, bool...)

    // case EXPR_INT2FLOAT:
        // extract value of node.u.s_val, convert to string
        // if node.u.var, then gen .inst. INT2FLOAT

    default:
        break;
    }
}

void expr_tree_post_order(ExprNode *node) {
    if (node->kind == EXPR_BINARY) {
        ExprNode *left = node->u.bin.left;
        if (left) {
            expr_tree_post_order(left);
        }
        ExprNode *right = node->u.bin.right;
        if (right) {
            expr_tree_post_order(right);
        }
        process_expr_leaf(node);
    }
    
}