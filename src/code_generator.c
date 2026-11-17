// Project:    IFJ26
// Filename:   code_generator.c
// Author:     Michal Zednicek <xzednim00>

#include <stdio.h>
#include <stdbool.h>
#include "symtable.h"
#include "expr_tree.h"

typedef struct {
    int cnt;       // celkový počet doteraz videných ifov (1, 2, 3...)
    int stack[256];    // zásobník ID aktuálne vnorených ifov (príp. dynamické pole)
    int stack_top;     // index vrcholu zásobníka (-1 = prázdny)
} LabelCounter;

LabelCounter if_counter;
LabelCounter while_counter;


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
    printf("int@%lld\n", val);
}
// AI END

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


// -----   built-in functions       -----
//AI-ASSISTED START
void gen_builtin_print(void) {
    gen_inst1("LABEL", "$$print");
    gen_inst0("CREATEFRAME");
    gen_inst0("PUSHFRAME");

    gen_inst1("DEFVAR", "LF@%val");
    gen_inst1("POPS", "LF@%val");

    gen_inst1("DEFVAR", "LF@%type");
    gen_inst2("TYPE", "LF@%type", "LF@%val");
    gen_inst3("JUMPIFNEQ", "$$print_not_nil", "LF@%type", "string@nil");

    gen_inst1("WRITE", "string@()");
    gen_inst0("POPFRAME");
    gen_inst0("RETURN");

    gen_inst1("LABEL", "$$print_not_nil");
    gen_inst1("WRITE", "LF@%val");
    gen_inst1("PUSHS", "nil@nil");
    gen_inst0("POPFRAME");
    gen_inst0("RETURN");
}

void gen_builtin_read_int(void) {
    gen_inst1("LABEL", "$$read_int");
    gen_inst0("CREATEFRAME");
    gen_inst0("PUSHFRAME");

    gen_inst1("DEFVAR", "LF@%val");
    gen_inst2("READ", "LF@%val", "int");
    
    gen_inst3("JUMPIFNEQ", "$$read_int_ok", "LF@%val", "nil@nil");
    gen_inst2("MOVE", "LF@%val", "int@0");

    gen_inst1("LABEL", "$$read_int_ok");
    gen_inst1("PUSHS", "LF@%val");
    gen_inst0("POPFRAME");
    gen_inst0("RETURN");
}
//AI-ASSISTED END

// prompt: generate C function emitting IFJcode26 subroutine for built-in readDouble()
// AI START
void gen_builtin_read_double(void) {
    gen_inst1("LABEL", "$$read_double");
    gen_inst0("CREATEFRAME");
    gen_inst0("PUSHFRAME");
    gen_inst1("DEFVAR", "LF@%res");
    gen_inst2("READ", "LF@%res", "float");
    gen_inst3("JUMPIFNEQ", "$$read_double_ok", "LF@%res", "nil@nil");
    gen_inst2("MOVE", "LF@%res", "float@0x0p+0");
    gen_inst1("LABEL", "$$read_double_ok");
    gen_inst1("PUSHS", "LF@%res");
    gen_inst0("POPFRAME");
    gen_inst0("RETURN");
}
// AI END

// prompt: generate C function emitting IFJcode26 subroutine for built-in toStr(value)
// AI START
void gen_builtin_to_str(void) {
    gen_inst1("LABEL", "$$to_str");
    gen_inst0("CREATEFRAME");
    gen_inst0("PUSHFRAME");
    gen_inst1("DEFVAR", "LF@%val");
    gen_inst1("DEFVAR", "LF@%type");
    gen_inst1("DEFVAR", "LF@%res");
    gen_inst1("POPS", "LF@%val");

    gen_inst2("TYPE", "LF@%type", "LF@%val");
    gen_inst3("JUMPIFEQ", "$$to_str_int", "LF@%type", "string@int");
    gen_inst3("JUMPIFEQ", "$$to_str_float", "LF@%type", "string@float");
    gen_inst3("JUMPIFEQ", "$$to_str_unit", "LF@%type", "string@nil");
    gen_inst3("JUMPIFEQ", "$$to_str_bool", "LF@%type", "string@bool");

    // string
    gen_inst2("MOVE", "LF@%res", "LF@%val");
    gen_inst1("JUMP", "$$to_str_end");

    gen_inst1("LABEL", "$$to_str_int");
    gen_inst2("INT2STR", "LF@%res", "LF@%val");
    gen_inst1("JUMP", "$$to_str_end");

    gen_inst1("LABEL", "$$to_str_float");
    gen_inst2("FLOAT2STR", "LF@%res", "LF@%val");
    gen_inst1("JUMP", "$$to_str_end");

    gen_inst1("LABEL", "$$to_str_unit");
    gen_inst2("MOVE", "LF@%res", "string@()");
    gen_inst1("JUMP", "$$to_str_end");

    // pre rozsirenie BOOLTHEN
    gen_inst1("LABEL", "$$to_str_bool");
    gen_inst3("JUMPIFEQ", "$$to_str_bool_true", "LF@%val", "bool@true");
    gen_inst2("MOVE", "LF@%res", "string@false");
    gen_inst1("JUMP", "$$to_str_end");
    gen_inst1("LABEL", "$$to_str_bool_true");
    gen_inst2("MOVE", "LF@%res", "string@true");

    gen_inst1("LABEL", "$$to_str_end");
    gen_inst1("PUSHS", "LF@%res");
    gen_inst0("POPFRAME");
    gen_inst0("RETURN");
}
// AI END

// prompt: generate C function emitting IFJcode26 subroutine for built-in ord(value, index)
// AI START
void gen_builtin_ord(void) {
    gen_inst1("LABEL", "$$ord");
    gen_inst0("CREATEFRAME");
    gen_inst0("PUSHFRAME");
    gen_inst1("DEFVAR", "LF@%str");
    gen_inst1("DEFVAR", "LF@%idx");
    gen_inst1("DEFVAR", "LF@%len");
    gen_inst1("DEFVAR", "LF@%cond");
    gen_inst1("DEFVAR", "LF@%res");

    // argumenty boli vlozene v poradi: str, idx -> vyberame opacne
    gen_inst1("POPS", "LF@%idx");
    gen_inst1("POPS", "LF@%str");

    // kontrola index < 0
    gen_inst3("LT", "LF@%cond", "LF@%idx", "int@0");
    gen_inst3("JUMPIFEQ", "$$ord_invalid", "LF@%cond", "bool@true");

    // kontrola index >= len (pokryva aj prazdny retazec, kde len == 0)
    gen_inst2("STRLEN", "LF@%len", "LF@%str");
    gen_inst3("LT", "LF@%cond", "LF@%idx", "LF@%len");
    gen_inst3("JUMPIFNEQ", "$$ord_invalid", "LF@%cond", "bool@true");

    // validny index
    gen_inst3("STRI2INT", "LF@%res", "LF@%str", "LF@%idx");
    gen_inst1("JUMP", "$$ord_end");

    gen_inst1("LABEL", "$$ord_invalid");
    gen_inst2("MOVE", "LF@%res", "int@-1");

    gen_inst1("LABEL", "$$ord_end");
    gen_inst1("PUSHS", "LF@%res");
    gen_inst0("POPFRAME");
    gen_inst0("RETURN");
}
// AI END

// prompt: generate C function emitting IFJcode26 subroutine for built-in substr(value, start, end)
// AI START
void gen_builtin_substr(void) {
    gen_inst1("LABEL", "$$substr");
    gen_inst0("CREATEFRAME");
    gen_inst0("PUSHFRAME");
    gen_inst1("DEFVAR", "LF@%str");
    gen_inst1("DEFVAR", "LF@%start");
    gen_inst1("DEFVAR", "LF@%end");
    gen_inst1("DEFVAR", "LF@%len");
    gen_inst1("DEFVAR", "LF@%cond");
    gen_inst1("DEFVAR", "LF@%res");
    gen_inst1("DEFVAR", "LF@%char");

    // argumenty vlozene v poradi: str, start, end
    gen_inst1("POPS", "LF@%end");
    gen_inst1("POPS", "LF@%start");
    gen_inst1("POPS", "LF@%str");

    gen_inst2("STRLEN", "LF@%len", "LF@%str");

    // 1. start < 0 -> error 40
    gen_inst3("LT", "LF@%cond", "LF@%start", "int@0");
    gen_inst3("JUMPIFEQ", "$$substr_err", "LF@%cond", "bool@true");

    // 2. end < 0 -> error 40
    gen_inst3("LT", "LF@%cond", "LF@%end", "int@0");
    gen_inst3("JUMPIFEQ", "$$substr_err", "LF@%cond", "bool@true");

    // 3. start > end -> error 40
    gen_inst3("GT", "LF@%cond", "LF@%start", "LF@%end");
    gen_inst3("JUMPIFEQ", "$$substr_err", "LF@%cond", "bool@true");

    // 4. start >= len (t.j. nie je start < len) -> error 40
    gen_inst3("LT", "LF@%cond", "LF@%start", "LF@%len");
    gen_inst3("JUMPIFNEQ", "$$substr_err", "LF@%cond", "bool@true");

    // 5. end > len -> error 40
    gen_inst3("GT", "LF@%cond", "LF@%end", "LF@%len");
    gen_inst3("JUMPIFEQ", "$$substr_err", "LF@%cond", "bool@true");

    // cyklus od start po end (vynimajuc end)
    gen_inst2("MOVE", "LF@%res", "string@");
    gen_inst1("LABEL", "$$substr_loop");
    gen_inst3("JUMPIFEQ", "$$substr_end", "LF@%start", "LF@%end");

    gen_inst3("GETCHAR", "LF@%char", "LF@%str", "LF@%start");
    gen_inst3("CONCAT", "LF@%res", "LF@%res", "LF@%char");
    gen_inst3("ADD", "LF@%start", "LF@%start", "int@1");
    gen_inst1("JUMP", "$$substr_loop");

    gen_inst1("LABEL", "$$substr_err");
    gen_inst1("EXIT", "int@40");

    gen_inst1("LABEL", "$$substr_end");
    gen_inst1("PUSHS", "LF@%res");
    gen_inst0("POPFRAME");
    gen_inst0("RETURN");
}
// AI END

// other built-in functions (strLength, chr, readLine, toInt) are translated directly to IFJ26code instructions

// -----   IFJ26code header generation      -----

void code_gen_init(void) {
    if_counter.cnt = 0;
    if_counter.stack_top = 0;
    while_counter.cnt = 0;
    while_counter.stack_top = 0;

    gen_inst0(".IFJcode26");

    gen_inst1("DEFVAR", "GF@%tmp1");
    gen_inst1("DEFVAR", "GF@%tmp2");
    gen_inst1("DEFVAR", "GF@%tmp3");

    gen_inst1("JUMP", "$main");

    gen_builtin_print();
    gen_builtin_read_int();
    gen_builtin_read_double();
    gen_builtin_to_str();
    gen_builtin_ord();
    gen_builtin_substr();
}

// -----    function generation     -----

// void gen_func_start(SymTable *table, char *func_name) {
//     printf("LABEL $%s\n", func_name);
//     gen_inst1("DEFVAR", "LF@%retval");
//     gen_inst2("MOVE", "LF@%retval", "nil@nil");

//     symbol_data *func_data = symtable_search_func(table, func_name);
//     for (size_t i = 0; i < func_data->data.func.param_count ;i++) {
//         /* code */
//     }
    
// }

// // AI-ASSISTED START
// void gen_func_return(char *func_name, ExprNode *ret_expr, data_type func_ret_type) {
//     if (ret_expr != NULL) {
//         // 1. Vždy vyhodnotíme výraz (aj keď je návratový typ Unit, kvôli vedľajším efektom)
//         gen_expr_tree(ret_expr);

//         // Ak funkcia vracia Double a výraz bol Int, prevedieme ho
//         if (func_ret_type == type_double && ret_expr->kind == EXPR_INT_LIT && ret_expr->kind != EXPR_INT2FLOAT) {
//             printf("INT2FLOATS\n");
//         }
//         printf("POPS LF@%%retval\n");
//     }
//     // Nepodmienený skok na spoločný epilóg funkcie, aby prebehol POPFRAME
//     printf("JUMP $%s$end\n", func_name);
// }

// void gen_func_implicit_end(FuncBodyEndKind end_kind, data_type last_expr_type, data_type func_ret_type) {
//     if (end_kind == FUNC_END_EXPR_ON_STACK) {
//         if (func_ret_type == type_unit) {
//             // Pravidlo 4: výraz/volanie na konci Unit funkcie sa vyhodnotí, ale výsledok sa zahodí
//             printf("POPS GF@$tmp1\n");
//             printf("MOVE LF@%%retval nil@nil\n");
//         } else {
//             // Pravidlo 2: vráti sa výsledok posledného výrazu alebo volania
//             if (func_ret_type == type_double && last_expr_type == type_int) {
//                 printf("INT2FLOATS\n");
//             }
//             printf("POPS LF@%%retval\n");
//         }
//     } else {
//         // Pravidlo 2 a 3: blok skončil príkazom bez hodnoty (val/var, priradenie, while, if)
//         printf("MOVE LF@%%retval nil@nil\n");
//     }
// }

// void gen_func_end(char *func_name) {
//     printf("LABEL $%s$end\n", func_name);
//     // Presunie aktuálny LF zo zásobníka rámcov do TF (odkiaľ si volajúci vyzdvihne TF@%retval)
//     printf("POPFRAME\n");
//     // Návrat na inštrukciu za CALL
//     printf("RETURN\n");
// }
// AI-ASSISTED END

// -----    IF/ELSE/WHILE ...               -----

// AI-ASSISTED START
void gen_if_start(ExprNode *expr) {
    int current_id = if_counter.cnt++;
    if_counter.stack[if_counter.stack_top] = current_id;
    if_counter.stack_top++;

    expr_tree_post_order(expr);
    printf("PUSHS bool@false\n");
    printf("JUMPIFEQS $else_%d\n", current_id);
}

void gen_if_else(void) {
    int current_id = if_counter.stack[if_counter.stack_top - 1];

    printf("JUMP $if_skip_%d\n", current_id);
    printf("LABEL $else_%d\n", current_id);
}

void gen_if_end(void) {
    int current_id = if_counter.stack[--if_counter.stack_top];

    printf("LABEL $if_skip_%d\n", current_id);
}
// AI-ASSISTED END

// -----    WHILE LOOP                      -----

void gen_while_start(ExprNode *expr) {
    int current_id = while_counter.cnt++;
    while_counter.stack[while_counter.stack_top] = current_id;
    while_counter.stack_top++;

    // TODO: GENERATE VARIABLES BEFORE LABEL (waiting for parser and symtable adjustment)
    printf("LABEL $while_start_%d\n", current_id);

    expr_tree_post_order(expr);
    printf("PUSHS bool@false\n");
    printf("JUMPIFEQS $while_end_%d\n", current_id);
}

void gen_while_end(void) {
    int current_id = while_counter.stack[--while_counter.stack_top];

    printf("JUMPI $while_start_%d\n", current_id);
    printf("LABEL $while_end_%d\n", current_id);
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
        printf("PUSHS LF@%s\n", node->u.var.id);
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