// Project:    IFJ26
// Filename:   code_generator.c
// Author:     Michal Zednicek <xzednim00>

#include <stdio.h>
#include <stdbool.h>

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
    printf("%s %s, %s\n", inst, arg1, arg2);
}

void gen_inst3(const char *inst, const char *arg1, const char *arg2, const char *arg3) {
    printf("%s %s, %s, %s\n", inst, arg1, arg2, arg3);
}

void gen_inst4(const char *inst, const char *arg1, const char *arg2, const char *arg3, const char *arg4) {
    printf("%s %s, %s, %s, %s\n", inst, arg1, arg2, arg3, arg4);
}
// AI END


// -----    constant data formatting functions      -----

// prompt:
// write fuknction to convert C float to this format:
// float@0x1.2666666666666p
// use %a format specifier for printf function

// AI START
void print_float(const float val) {
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

void print_int(const long long int val) {
    printf("int@%lld\n", val);
}
// AI END

void print_string(const char *str) {
    printf("string@");
    if (str != NULL) {
        for (const char *p = str; *p != '\0'; p++) {
            unsigned char c = (unsigned char)*p;
            if (c <= 32 || c == 35 || c == 92) {
                printf("\\0%d", (unsigned char)*p);
            } else {
                putchar(*p);
            }
        }
    }
    printf("\n");
}