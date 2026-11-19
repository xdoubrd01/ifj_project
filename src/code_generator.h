#ifndef CODE_GENERATOR_H
#define CODE_GENERATOR_H

#include <stdbool.h>
#include "symtable.h"
#include "expr_tree.h"

// Vypis jednotlivych instrukci IFJcode26
void gen_inst0(const char *inst);
void gen_inst1(const char *inst, const char *arg1);
void gen_inst2(const char *inst, const char *arg1, const char *arg2);
void gen_inst3(const char *inst, const char *arg1, const char *arg2, const char *arg3);
void gen_inst4(const char *inst, const char *arg1, const char *arg2, const char *arg3, const char *arg4);

// Konstanty
void print_double(const double val);
void print_bool(const bool val);
void print_nil(void);
void print_int(const int val);
void print_string(const char *str);

// Vestavene funkce
void gen_builtin_print(void);
void gen_builtin_read_int(void);
void gen_builtin_read_double(void);
void gen_builtin_to_str(void);
void gen_builtin_ord(void);
void gen_builtin_substr(void);
void gen_builtin_strLength(void);
void gen_builtin_chr(void);
void gen_builtin_readLine(void);
void gen_builtin_toInt(void);

// Inicializace generatoru
void code_gen_init(void);

// Volani a definice funkci
void gen_funct_call(char *func_name);
void gen_func_start(SymTable *table, char *func_name, data_type ret_type);
void gen_func_return(ExprNode *ret_expr);
void gen_func_end(void);

// Podminky a cykly
void gen_if_start(ExprNode *expr);
void gen_if_else(void);
void gen_if_end(void);
void gen_while_start(ExprNode *expr);
void gen_while_end(void);

// Vyrazy a promenne
void process_expr_node(ExprNode *node);
void gen_var_assign(SymTable *ts, char *var_name, data_type var_type, ExprNode *node);
void gen_def_var(SymTable *ts, char *var_name, data_type var_type, ExprNode *node);

#endif // CODE_GENERATOR_H
