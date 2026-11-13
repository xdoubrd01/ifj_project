// Project:    IFJ26
// Filename:   code_generator.c
// Author:     Michal Zednicek <xzednim00>

#include <stdio.h>
#include <stdbool.h>

// functions to generate IFJ26code instructions

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
    printf("%s %s, %s\n", inst, arg1, arg2);
}

void gen_inst3(const char *inst, const char *arg1, const char *arg2, const char *arg3) {
    printf("%s %s, %s, %s\n", inst, arg1, arg2, arg3);
}

void gen_inst4(const char *inst, const char *arg1, const char *arg2, const char *arg3, const char *arg4) {
    printf("%s %s, %s, %s, %s\n", inst, arg1, arg2, arg3, arg4);
}
// AI END

