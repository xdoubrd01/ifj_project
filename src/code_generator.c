// Project:    IFJ26
// Filename:   code_generator.c
// Author:     Michal Zednicek <xzednim00>

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include "symtable.h"
#include "expr_tree.h"
#include "code_buffer.h"
#include "code_generator.h"

typedef struct {
    int cnt;       // celkový počet doteraz videných ifov (1, 2, 3...)
    int stack[256];    // zásobník ID aktuálne vnorených ifov (príp. dynamické pole)
    int stack_top;     // index vrcholu zásobníka (-1 = prázdny)
} LabelCounter;

LabelCounter if_counter;
LabelCounter while_counter;

CodeBuffer *code_buff;

static char *current_func_name = NULL;
static data_type current_func_ret_type = type_unknown;


// -----    functions to generate IFJ26code instructions    -----

// prompt:
// write functions in c to generate instructions on stdout.
// make multiple for 0 to 4 arguments. example:
// void gen_inst0(const char *inst) {
// printf("%s\n", inst);
// }

// AI START
void gen_inst0(const char *inst) {
    buffer_append_format(code_buff, "%s\n", inst);
}

void gen_inst1(const char *inst, const char *arg1) {
    buffer_append_format(code_buff, "%s %s\n", inst, arg1);
}

void gen_inst2(const char *inst, const char *arg1, const char *arg2) {
    buffer_append_format(code_buff, "%s %s %s\n", inst, arg1, arg2);
}

void gen_inst3(const char *inst, const char *arg1, const char *arg2, const char *arg3) {
    buffer_append_format(code_buff, "%s %s %s %s\n", inst, arg1, arg2, arg3);
}

void gen_inst4(const char *inst, const char *arg1, const char *arg2, const char *arg3, const char *arg4) {
    buffer_append_format(code_buff, "%s %s %s %s %s\n", inst, arg1, arg2, arg3, arg4);
}
// AI END


// -----    constant data formatting functions      -----

// prompt:
// write fuknction to convert C float to this format:
// float@0x1.2666666666666p
// use %a format specifier for printf function

// AI START
void print_double(const double val) {
    buffer_append_format(code_buff, "float@%a\n", val);
}

// prompt:
// do the same for those formats
// bool@true, nil@nil, int@-5
void print_bool(const bool val) {
    buffer_append_format(code_buff, "bool@%s\n", val ? "true" : "false");
}

void print_nil(void) {
    buffer_append_string(code_buff, "nil@nil\n");
}

void print_int(const int val) {
    buffer_append_format(code_buff, "int@%lld\n", (long long)val);
}
// AI END

void print_string(const char *str) {
    buffer_append_string(code_buff, "string@");
    if (str != NULL) {
        for (const char *p = str; *p != '\0'; p++) {
            unsigned char c = (unsigned char)*p;
            if (c <= 32 || c == 35 || c == 92) {
                buffer_append_format(code_buff, "\\%03d", c);
            } else {
                buffer_append_char(code_buff, *p);
            }
        }
    }
    buffer_append_string(code_buff, "\n");
}


// -----   built-in functions       -----
//AI-ASSISTED START
void gen_builtin_print(void) {
    gen_inst1("LABEL", "$print");
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
    gen_inst1("LABEL", "$readInt");
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
    gen_inst1("LABEL", "$readDouble");
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
    gen_inst1("LABEL", "$toStr");
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
    gen_inst1("LABEL", "$ord");
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
    gen_inst1("LABEL", "$subStr");
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

// Prompt:
// napis pomocne funkcie pre definovanie tychto funkcii v cielovom jazyku: 
// strLength, chr, readLine, toInt. pouzi instrukcnu sadu jazyka ifj26code.
// navratovu hodnotu vracaj pomocou datoveho zasobniku
// AI START
// Definícia vstavanej funkcie strLength(value: String): Int
void gen_builtin_strLength(void) {
    gen_inst1("LABEL", "$strLength");
    gen_inst0("CREATEFRAME");
    gen_inst0("PUSHFRAME");

    gen_inst1("DEFVAR", "LF@value");
    gen_inst1("POPS", "LF@value");

    gen_inst1("DEFVAR", "LF@%retval");
    gen_inst2("STRLEN", "LF@%retval", "LF@value");

    gen_inst1("PUSHS", "LF@%retval");
    gen_inst0("POPFRAME");
    gen_inst0("RETURN");
}

// Definícia vstavanej funkcie chr(code: Int): String
void gen_builtin_chr(void) {
    gen_inst1("LABEL", "$chr");
    gen_inst0("CREATEFRAME");
    gen_inst0("PUSHFRAME");

    gen_inst1("DEFVAR", "LF@code");
    gen_inst1("POPS", "LF@code");

    gen_inst1("DEFVAR", "LF@%retval");
    gen_inst2("INT2CHAR", "LF@%retval", "LF@code");

    gen_inst1("PUSHS", "LF@%retval");
    gen_inst0("POPFRAME");
    gen_inst0("RETURN");
}

// Definícia vstavanej funkcie readLine(): String
void gen_builtin_readLine(void) {
    gen_inst1("LABEL", "$readLine");
    gen_inst0("CREATEFRAME");
    gen_inst0("PUSHFRAME");

    gen_inst1("DEFVAR", "LF@%retval");
    gen_inst2("READ", "LF@%retval", "string");

    // Ak READ narazil na EOF (vrátil nil), nahradíme výsledok prázdnym reťazcom
    gen_inst3("JUMPIFNEQ", "$readLine_end", "LF@%retval", "nil@nil");
    gen_inst2("MOVE", "LF@%retval", "string@");

    gen_inst1("LABEL", "$readLine_end");
    gen_inst1("PUSHS", "LF@%retval");
    gen_inst0("POPFRAME");
    gen_inst0("RETURN");
}

// Definícia vstavanej funkcie toInt(value: Double): Int
void gen_builtin_toInt(void) {
    gen_inst1("LABEL", "$toInt");
    gen_inst0("CREATEFRAME");
    gen_inst0("PUSHFRAME");

    gen_inst1("DEFVAR", "LF@value");
    gen_inst1("POPS", "LF@value");

    gen_inst1("DEFVAR", "LF@%retval");
    gen_inst2("FLOAT2INT", "LF@%retval", "LF@value");

    gen_inst1("PUSHS", "LF@%retval");
    gen_inst0("POPFRAME");
    gen_inst0("RETURN");
}
// AI END

// -----   IFJ26code header generation      -----

void code_gen_init(void) {
    if (!buffer_init(code_buff)) {
        fprintf(stderr, "Error allocating buffer!\n");
        exit(99);
    }

    if_counter.cnt = 0;
    if_counter.stack_top = 0;
    while_counter.cnt = 0;
    while_counter.stack_top = 0;

    gen_inst0(".IFJcode26");

    gen_inst1("DEFVAR", "GF@%tmp_op1");
    gen_inst1("DEFVAR", "GF@%tmp_op2");
    gen_inst1("DEFVAR", "GF@%tmp_res");

    gen_inst1("JUMP", "$main");

    gen_builtin_print();
    gen_builtin_read_int();
    gen_builtin_read_double();
    gen_builtin_to_str();
    gen_builtin_ord();
    gen_builtin_substr();

    gen_builtin_strLength();
    gen_builtin_chr();
    gen_builtin_readLine();
    gen_builtin_toInt();

    buffer_flush(code_buff);
}

// -----    function generation     -----

void gen_funct_call(char *func_name){
    buffer_append_format(code_buff, "CALL $%s\n", func_name);
}

void gen_func_start(SymTable *table, char *func_name, data_type ret_type) {
    current_func_name = func_name;
    current_func_ret_type = ret_type;

    printf("LABEL $%s\n", func_name);
    printf("CREATEFRAME\n");
    printf("PUSHFRAME\n");

    symbol_data *func_data = symtable_search_func(table, func_name);
    if (func_data == NULL) {
        return;
    }

    size_t param_count = func_data->data.func.param_count;

    // Definícia lokálnej premennej a jej naplnenie zo zásobníka od posledného parametra po prvý
    for (size_t i = param_count; i > 0; i--) {
        symbol_data *var_data = symtable_search_var(table, func_data->data.func.param_names[i - 1]);
        if (var_data != NULL) {
            buffer_append_format(code_buff, "DEFVAR LF@v%u\n", var_data->data.var.id);
            buffer_append_format(code_buff, "POPS LF@v%u\n", var_data->data.var.id);
        }
    }
}

// AI-ASSISTED START
void gen_func_return(ExprNode *ret_expr) {
    if (ret_expr != NULL) {
        // 1. Vždy vyhodnotíme výraz (aj keď je návratový typ Unit, kvôli vedľajším efektom)
        process_expr_node(ret_expr);

        // Ak funkcia vracia Double a výraz bol Int, prevedieme ho
        if (current_func_ret_type == type_double && ret_expr->kind == EXPR_INT_LIT && ret_expr->kind != EXPR_INT2FLOAT) {
            gen_inst0("INT2FLOATS");
        }
    }
    // Nepodmienený skok na spoločný epilóg funkcie, aby prebehol POPFRAME
    buffer_append_format(code_buff, "JUMP $%s$end\n", current_func_name);
}

// TODO: pridat osetrenie pre 'return func()'
// mozne riesenie vraciat hodnotu na vrchu zasobniku -> vyraz z volanej funkcie pomocou return bude automaticky na vrchu ->
// podporovane rozsirenie FUNEX
void gen_func_end(void) {
    buffer_append_format(code_buff, "LABEL $%s$end\n", current_func_name);

    if (current_func_ret_type == type_unit) {
        buffer_append_string(code_buff, "PUSHS nil@nil\n");
    }

    gen_inst0("POPFRAME");
    // Návrat na inštrukciu za CALL
    gen_inst0("RETURN");

    buffer_flush(code_buff);
}
// AI-ASSISTED END

// -----    IF/ELSE/WHILE ...               -----

// AI-ASSISTED START
void gen_if_start(ExprNode *expr) {
    int current_id = if_counter.cnt++;
    if_counter.stack[if_counter.stack_top] = current_id;
    if_counter.stack_top++;

    process_expr_node(expr);
    buffer_append_string(code_buff, "PUSHS bool@false\n");
    buffer_append_format(code_buff, "JUMPIFEQS $else_%d\n", current_id);
}

void gen_if_else(void) {
    int current_id = if_counter.stack[if_counter.stack_top - 1];

    buffer_append_format(code_buff, "JUMP $if_skip_%d\n", current_id);
    buffer_append_format(code_buff, "LABEL $else_%d\n", current_id);
}

void gen_if_end(void) {
    int current_id = if_counter.stack[--if_counter.stack_top];

    buffer_append_format(code_buff, "LABEL $if_skip_%d\n", current_id);
}
// AI-ASSISTED END

// -----    WHILE LOOP                      -----

void gen_while_start(ExprNode *expr) {
    int current_id = while_counter.cnt++;
    while_counter.stack[while_counter.stack_top] = current_id;
    while_counter.stack_top++;

    // TODO: GENERATE VARIABLES BEFORE LABEL (waiting for parser and symtable adjustment)
    buffer_append_format(code_buff, "LABEL $while_start_%d\n", current_id);

    process_expr_node(expr);
    buffer_append_string(code_buff, "PUSHS bool@false\n");
    buffer_append_format(code_buff, "JUMPIFEQS $while_end_%d\n", current_id);
}

void gen_while_end(void) {
    int current_id = while_counter.stack[--while_counter.stack_top];

    buffer_append_format(code_buff, "JUMP $while_start_%d\n", current_id);
    buffer_append_format(code_buff, "LABEL $while_end_%d\n", current_id);
}



// -----    expr_tree process functions     -----

void process_expr_node(ExprNode *node) {
    if (node->kind == EXPR_BINARY) {
        ExprNode *left = node->u.bin.left;
        if (left) {
            process_expr_node(left);
        }
        ExprNode *right = node->u.bin.right;
        if (right) {
            process_expr_node(right);
        }

        // prompt:
        // Complete logic for EXPR_BINARY
        // AI START
        switch (node->u.bin.op) {
            case OP_ADD:
                gen_inst0("ADDS");
                break;
            case OP_SUB:
                gen_inst0("SUBS");
                break;
            case OP_MUL:
                gen_inst0("MULS");
                break;
            case OP_DIV:
                gen_inst0("DIVS");
                break;
            case OP_IDIV:
                gen_inst0("IDIVS");
                break;
            case OP_EQ:
                gen_inst0("EQS");
                break;
            case OP_NEQ:
                gen_inst0("EQS");
                gen_inst0("NOTS");
                break;
            case OP_LT:
                gen_inst0("LTS");
                break;
            case OP_LEQ:
                gen_inst0("GTS");
                gen_inst0("NOTS");
                break;
            case OP_GT:
                gen_inst0("GTS");
                break;
            case OP_GEQ:
                gen_inst0("LTS");
                gen_inst0("NOTS");
                break;
            case OP_CONCAT:
                // IFJcode26 does not provide a CONCATS instruction.
                // We must pop the operands into temporary variables (e.g., in GF), 
                // perform CONCAT, and push the result back.
                // (Make sure to define GF@%tmp_op1, GF@%tmp_op2, GF@%tmp_res during program initialization)
                buffer_append_string(code_buff, "POPS GF@%tmp_op2\n");
                buffer_append_string(code_buff, "POPS GF@%tmp_op1\n");
                buffer_append_string(code_buff, "CONCAT GF@%tmp_res GF@%tmp_op1 GF@%tmp_op2\n");
                buffer_append_string(code_buff, "PUSHS GF@%tmp_res\n");
                break;
        }
        // AI END
    }

    switch (node->kind) {
    case EXPR_INT_LIT:
        buffer_append_string(code_buff, "PUSHS ");
        print_int(node->u.i_val);
        break;
    
    case EXPR_DOUBLE_LIT:
        buffer_append_string(code_buff, "PUSHS ");
        print_double(node->u.d_val);
        break;

    case EXPR_STRING_LIT:
        buffer_append_string(code_buff, "PUSHS ");
        print_string(node->u.s_val);
        break;

    case EXPR_UNIT_LIT:
        buffer_append_string(code_buff, "PUSHS ");
        print_nil();
        break;

    case EXPR_VAR:
        buffer_append_format(code_buff, "PUSHS LF@%u\n", node->u.var.id);
        break;

    case EXPR_NEG:
        process_expr_node(node->u.operand);
        if (node->type == type_int) {
            gen_inst1("PUSHS", "int@-1");
        } else {
            gen_inst1("PUSHS", "float@-0x1p+0");
        }
        gen_inst0("MULS");
        break;
        // analyse node.u.type, proccess negation of data type (if int -> reverse operand, bool...)

    case EXPR_INT2FLOAT:
        process_expr_node(node->u.operand);
        gen_inst0("INT2FLOATS");
        break;

    default:
        break;
    }
}


// -----    variable related functions      -----

// AI ASSISTED START
void gen_var_assign(SymTable *ts, char *var_name, data_type var_type, ExprNode *node) {
    symbol_data *var_data = symtable_search_var(ts, var_name);
    unsigned var_id =  var_data->data.var.id;

    process_expr_node(node);

    // Implicitná konverzia Int -> Double na vrchole dátového zásobníka
    if (var_type == type_double && node->type == type_int) {
        gen_inst0("INT2FLOATS");
    }

    // Uloženie výsledku zo zásobníka do premennej v LF (bez DEFVAR)
    buffer_append_format(code_buff, "POPS LF@v%u\n", var_id);
}

void gen_def_var(SymTable *ts, char *var_name, data_type var_type, ExprNode *node) {
    symbol_data *var_data = symtable_search_var(ts, var_name);
    unsigned var_id =  var_data->data.var.id;

    printf("DEFVAR LF@v%u\n", var_id);

    if (node != NULL) {
        gen_var_assign(ts, var_name, var_type, node);
    }
}
// AI ASSISTED END