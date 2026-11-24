#!/usr/bin/env python3
"""Python test runner for the current IFJ26 project; standard library only."""
import argparse
from collections import Counter
import json
import math
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import sys
import tempfile

COMMON = ['array_utils.c', 'dynamic_buffer.c', 'str_utils.c', 'token_utils.c']
TABLES = ['symtable.c', 'hashtable.c', 'str_utils.c']
SUITES = ['helpers', 'tables', 'scanner', 'expressions', 'semantic',
          'parser', 'semantic_programs', 'codegen', 'integration']

HELPERS_C = r'''
#include "array_utils.h"
#include "dynamic_buffer.h"
#include "str_utils.h"
#include "token_utils.h"
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
static bool format(DynamicBuffer *b, const char *fmt, ...) {
    va_list a; va_start(a, fmt);
    bool ok = dynamic_buffer_append_vformat(b, fmt, a);
    va_end(a); return ok;
}
int main(int argc, char **argv) {
    if (argc != 2) return 99;
    int test = atoi(argv[1]);
    if (test == 0) {
        size_t n = 123;
        assert(array_next_capacity(0, 17, sizeof(int), 16, &n));
        assert(n >= 17 && n <= SIZE_MAX / sizeof(int));
        size_t old = n;
        assert(!array_next_capacity(0, SIZE_MAX, sizeof(int), 16, &n));
        assert(n == old);
        assert(!array_next_capacity(0, 1, 0, 16, &n));
        assert(!array_next_capacity(0, 1, 1, 16, NULL));
        assert(array_next_capacity(0, SIZE_MAX, 1, 16, &n));
        assert(n == SIZE_MAX);
    } else if (test == 1) {
        DynamicBuffer b; dynamic_buffer_init(&b);
        for (int i = 0; i < 10000; ++i)
            assert(dynamic_buffer_append_char(&b, 'x'));
        assert(format(&b, ":%s:%d", "done", 42));
        assert(b.length == 10008 && strcmp(b.data + 10000, ":done:42") == 0);
        char *data = b.data; size_t cap = b.capacity;
        assert(!dynamic_buffer_reserve(&b, SIZE_MAX));
        assert(b.data == data && b.capacity == cap && b.length == 10008);
        dynamic_buffer_clear(&b);
        assert(b.length == 0 && b.capacity == cap);
        assert(dynamic_buffer_append_string(&b, "again"));
        assert(strcmp(b.data, "again") == 0);
        dynamic_buffer_free(&b); dynamic_buffer_free(&b);
        assert(b.data == NULL && b.capacity == 0 && b.length == 0);
    } else if (test == 2) {
        char input[] = "name";
        char *s = string_duplicate(input); assert(s);
        input[0] = 'X'; assert(strcmp(s, "name") == 0); free(s);
        const char bytes[] = {'a', '\0', 'b'};
        s = string_duplicate_n(bytes, 3); assert(s);
        assert(memcmp(s, bytes, 3) == 0 && s[3] == '\0'); free(s);
        s = string_duplicate(""); assert(s && s[0] == '\0'); free(s);
        assert(string_duplicate(NULL) == NULL);
        assert(string_duplicate_n("", SIZE_MAX) == NULL);
    } else if (test == 3) {
        Token t = {.type = TOKEN_IDENTIFIER, .newline_before = true};
        t.attribute.s_val = string_duplicate("owned"); assert(t.attribute.s_val);
        token_destroy(&t);
        assert(t.type == TOKEN_EOF && !t.newline_before && !t.attribute.s_val);
        token_destroy(&t); token_destroy(NULL);
        t = (Token){.type = TOKEN_STRING_LITERAL};
        t.attribute.s_val = string_duplicate("moved"); assert(t.attribute.s_val);
        char *moved = t.attribute.s_val; t.attribute.s_val = NULL;
        token_destroy(&t); assert(strcmp(moved, "moved") == 0); free(moved);
        t = (Token){.type = TOKEN_INT_LITERAL}; t.attribute.i_val = 123;
        token_destroy(&t); assert(t.type == TOKEN_EOF);
    } else return 99;
    return 0;
}
'''

TABLES_C = r'''
#include "hashtable.h"
#include "symtable.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static size_t visited;
static void count(void *p) { assert(p); ++visited; }
int main(int argc, char **argv) {
    if (argc != 2) return 99;
    int test = atoi(argv[1]);
    if (test == 0) {
        const char *keys[] = {"key_2", "key_11", "key_20", "key_101", "key_110"};
        int v[] = {1, 2, 3, 4, 5, 6};
        hashtable *t = ht_create(16); assert(t);
        assert(ht_search(NULL, "x") == NULL && !ht_delete(NULL, "x"));
        assert(ht_search(t, NULL) == NULL && !ht_delete(t, NULL));
        for (int i = 0; i < 4; ++i) {
            char key[32]; snprintf(key, sizeof(key), "%s", keys[i]);
            assert(ht_insert(t, key, &v[i])); key[0] = 'X';
        }
        assert(ht_delete(t, keys[0]) && ht_delete(t, keys[2]));
        assert(!ht_delete(t, keys[0]) && ht_search(t, keys[0]) == NULL);
        assert(ht_search(t, keys[1]) == &v[1]);
        assert(ht_search(t, keys[3]) == &v[3]);
        assert(ht_insert(t, keys[1], &v[5]));
        assert(ht_insert(t, keys[4], &v[4]));
        assert(ht_search(t, keys[1]) == &v[5]);
        visited = 0; ht_foreach(t, count); assert(visited == 3);
        ht_free(t); assert(v[0] == 1 && v[5] == 6);
    } else if (test == 1) {
        int values[256]; void *model[127] = {0};
        for (int i = 0; i < 256; ++i) values[i] = i;
        hashtable *t = ht_create(1); assert(t);
        uint32_t random = 0x12345678u;
        for (int step = 0; step < 10000; ++step) {
            random = random * 1664525u + 1013904223u;
            unsigned i = (random >> 9) % 127, op = (random >> 24) % 3;
            char key[32]; snprintf(key, sizeof(key), "item_%u", i);
            if (op == 0) {
                void *value = &values[(random >> 16) % 256];
                assert(ht_insert(t, key, value)); model[i] = value;
            } else if (op == 1) {
                assert(ht_delete(t, key) == (model[i] != NULL)); model[i] = NULL;
            } else assert(ht_search(t, key) == model[i]);
            if (step % 64 == 0) {
                size_t expected = 0;
                for (unsigned j = 0; j < 127; ++j) {
                    snprintf(key, sizeof(key), "item_%u", j);
                    assert(ht_search(t, key) == model[j]); expected += model[j] != NULL;
                }
                visited = 0; ht_foreach(t, count); assert(visited == expected);
            }
        }
        ht_free(t);
    } else if (test == 2) {
        SymTable *t = symtable_create(1); assert(t);
        assert(!symtable_insert_var(t, "x", type_int, true));
        assert(symtable_enter_scope(t));
        assert(symtable_insert_var(t, "x", type_int, true));
        symbol_data *outer = symtable_search_var(t, "x"); assert(outer);
        assert(outer->data.var.id == 0 && outer->data.var.is_mutable);
        assert(!symtable_insert_param(t, "x", type_double));
        assert(symtable_insert_param(t, "p", type_double));
        symbol_data *p = symtable_search_var(t, "p");
        assert(p && p->data.var.id == 1 && p->data.var.is_parameter);
        assert(!p->data.var.is_mutable && !outer->data.var.is_parameter);
        assert(symtable_enter_scope(t));
        assert(symtable_insert_var(t, "x", type_string, false));
        assert(symtable_search_var(t, "x")->data.var.id == 2);
        assert(symtable_leave_scope(t));
        assert(symtable_search_var(t, "x") == outer);
        symtable_destroy(t);
    } else if (test == 3) {
        SymTable *t = symtable_create(0); assert(t);
        data_type types[] = {type_int, type_string};
        assert(symtable_insert_func(t, "f", type_unit, 2, types));
        char a[] = "first", b[] = "second";
        const char *names[] = {a, b};
        assert(symtable_set_func_param_names(t, "f", 2, names));
        a[0] = 'X'; b[0] = 'Y'; types[0] = type_double;
        symbol_data *f = symtable_search_func(t, "f"); assert(f);
        assert(f->data.func.params[0] == type_int);
        assert(strcmp(f->data.func.param_names[0], "first") == 0);
        assert(strcmp(f->data.func.param_names[1], "second") == 0);
        const char *bad[] = {"valid", NULL};
        assert(!symtable_set_func_param_names(t, "f", 2, bad));
        assert(strcmp(f->data.func.param_names[0], "first") == 0);
        symtable_destroy(t);
    } else return 99;
    return 0;
}
'''

NODES_C = r'''
static ExprNode *node(ExprKind kind, data_type type) {
    ExprNode *n = expr_new(kind, type); if (!n) exit(99); return n;
}
static ExprNode *integer(int value) {
    ExprNode *n = node(EXPR_INT_LIT, type_int); n->u.i_val = value; return n;
}
static ExprNode *decimal(double value) {
    ExprNode *n = node(EXPR_DOUBLE_LIT, type_double); n->u.d_val = value; return n;
}
static ExprNode *text(const char *value) {
    ExprNode *n = node(EXPR_STRING_LIT, type_string);
    n->u.s_val = string_duplicate(value); if (!n->u.s_val) exit(99); return n;
}
static ExprNode *binary(ExprOp op, data_type type, ExprNode *a, ExprNode *b) {
    ExprNode *n = node(EXPR_BINARY, type);
    n->u.bin.op = op; n->u.bin.left = a; n->u.bin.right = b; return n;
}
'''
SEMANTIC_C = r'''
#include "semantic.h"
#include "str_utils.h"
#include <assert.h>
#include <stdlib.h>
''' + NODES_C + r'''
int main(int argc, char **argv) {
    if (argc != 2) return 99;
    int test = atoi(argv[1]);
    if (test <= 2) {
        ExprNode *a, *b; ExprOp op; data_type type;
        if (test == 0) {
            a = integer(2); b = decimal(3.5);
            assert(sem_binary_op(TOKEN_PLUS, &a, &b, &op, &type) == 0);
            assert(op == OP_ADD && type == type_double);
            assert(a->kind == EXPR_INT2FLOAT && b->kind == EXPR_DOUBLE_LIT);
        } else if (test == 1) {
            a = text("A"); b = text("B");
            assert(sem_binary_op(TOKEN_PLUS, &a, &b, &op, &type) == 0);
            assert(op == OP_CONCAT && type == type_string);
            assert(sem_binary_op(TOKEN_MINUS, &a, &b, &op, &type) == 6);
        } else {
            a = integer(-5); b = integer(2);
            assert(sem_binary_op(TOKEN_DIV, &a, &b, &op, &type) == 0);
            assert(op == OP_IDIV && type == type_int);
        }
        expr_free(a); expr_free(b);
    } else if (test == 3) {
        SymTable *t = symtable_create(0); assert(t && symtable_enter_scope(t));
        unsigned id;
        assert(sem_declare_variable(t, "x", type_unknown, type_int, true, &id) == 0);
        assert(sem_declare_variable(t, "x", type_int, type_int, true, NULL) == 4);
        assert(sem_check_assignment(t, "x", type_int, NULL) == 0);
        assert(sem_check_assignment(t, "x", type_string, NULL) == 6);
        assert(sem_check_assignment(t, "missing", type_int, NULL) == 3);
        assert(sem_declare_variable(t, "fixed", type_int, type_int, false, NULL) == 0);
        assert(sem_check_assignment(t, "fixed", type_int, NULL) == 7);
        assert(symtable_insert_param(t, "p", type_int));
        assert(sem_check_assignment(t, "p", type_int, NULL) == 7);
        symtable_destroy(t);
    } else if (test == 4) {
        ExprNode *n = integer(2);
        assert(sem_coerce_expr(&n, type_double) == 0);
        assert(n->kind == EXPR_INT2FLOAT && n->type == type_double);
        expr_free(n);
        n = binary(OP_LT, type_bool, integer(1), integer(2));
        assert(sem_check_condition(n) == 0); expr_free(n);
        n = integer(1); assert(sem_check_condition(n) == 6); expr_free(n);
    } else return 99;
    return 0;
}
'''

SCANNER_C = r'''
#include "scanner.h"
#include "token_utils.h"
#include <stdio.h>
static const char *name(TokenType type) {
    switch (type) {
#define T(x) case x: return #x;
''' + ''.join('T(' + n + ')\n' for n in '''TOKEN_EOF TOKEN_EOL TOKEN_IDENTIFIER
TOKEN_INT_LITERAL TOKEN_DOUBLE_LITERAL TOKEN_STRING_LITERAL TOKEN_KW_DEF
TOKEN_KW_DO TOKEN_KW_ELSE TOKEN_KW_IF TOKEN_KW_RETURN TOKEN_KW_THEN TOKEN_KW_VAL
TOKEN_KW_VAR TOKEN_KW_WHILE TOKEN_KW_IMPORT TOKEN_TYPE_INT TOKEN_TYPE_DOUBLE
TOKEN_TYPE_STRING TOKEN_TYPE_UNIT TOKEN_ASSIGN TOKEN_PLUS TOKEN_MINUS TOKEN_MUL
TOKEN_DIV TOKEN_EQ TOKEN_NEQ TOKEN_LT TOKEN_LEQ TOKEN_GT TOKEN_GEQ TOKEN_LPAREN
TOKEN_RPAREN TOKEN_COLON TOKEN_COMMA TOKEN_DOT TOKEN_SEMICOLON TOKEN_AT_MAIN
TOKEN_INDENT TOKEN_DEDENT'''.split()) + r'''
#undef T
    default: return "UNKNOWN";
    }
}
int main(int argc, char **argv) {
    (void)argv; bool flags = argc > 1;
    scanner_init(stdin);
    for (;;) {
        Token t = {0}; int rc = get_next_token(&t);
        if (rc) { scanner_free(); return rc; }
        printf("%s", name(t.type));
        if (t.type == TOKEN_INT_LITERAL) printf("|%d", t.attribute.i_val);
        if (t.type == TOKEN_DOUBLE_LITERAL) printf("|%.17g", t.attribute.d_val);
        if (t.type == TOKEN_IDENTIFIER || t.type == TOKEN_STRING_LITERAL) {
            putchar('|');
            for (unsigned char *p = (unsigned char *)t.attribute.s_val; *p; ++p)
                printf("%02x", *p);
        }
        if (flags) printf("|N%d", t.newline_before);
        putchar('\n'); bool end = t.type == TOKEN_EOF;
        token_destroy(&t);
        if (end) break;
    }
    scanner_free(); return 0;
}
'''

EXPRESSIONS_C = r'''
#include "exp_parser.h"
#include "token_utils.h"
#include <stdio.h>
int main(void) {
    SymTable *t = symtable_create(0);
    if (!t || !symtable_enter_scope(t)) return 99;
    if (!symtable_insert_var(t, "x", type_int, true)) return 99;
    if (!symtable_insert_var(t, "y", type_double, true)) return 99;
    scanner_init(stdin); Token tok = {0}; ExprNode *tree = NULL;
    int rc = get_next_token(&tok);
    if (!rc) rc = parse_expression(t, NULL, &tok, &tree);
    if (!rc) { expr_dump(tree, stdout); putchar('\n'); token_destroy(&tok); }
    expr_free(tree); scanner_free(); symtable_destroy(t); return rc;
}
'''

CODEGEN_C = r'''
#include "code_generator.h"
#include "code_buffer.h"
#include "str_utils.h"
#include <stdlib.h>
extern CodeBuffer code_buff;
''' + NODES_C + r'''
static void emit(ExprNode *n) { process_expr_node(n); expr_free(n); }
static void print_top(void) {
    gen_funct_call("print"); gen_inst1("POPS", "GF@%tmp_res");
}
int main(int argc, char **argv) {
    if (argc != 2) return 99;
    int test = atoi(argv[1]);
    SymTable *t = symtable_create(0);
    if (!t || !symtable_insert_func(t, "entry", type_unit, 0, NULL)) return 99;
    if (!symtable_enter_scope(t)) return 99;
    code_gen_init("entry"); gen_func_start(t, "entry", type_unit);
    if (test == 0) {
        emit(binary(OP_ADD, type_int, integer(2),
                    binary(OP_MUL, type_int, integer(3), integer(4)))); print_top();
    } else if (test == 1) {
        emit(binary(OP_IDIV, type_int, integer(-5), integer(2))); print_top();
    } else if (test == 2) {
        emit(text("A #\\\nB\t")); print_top();
    } else if (test == 3) {
        emit(binary(OP_ADD, type_double, decimal(1.5), decimal(2.5)));
        gen_funct_call("toStr"); print_top();
    } else if (test == 4) {
        ExprNode *n = binary(OP_EQ, type_bool, integer(1), integer(1));
        gen_if_start(n); gen_inst1("WRITE", "string@T");
        gen_if_else(); gen_inst1("WRITE", "string@F"); gen_if_end(); expr_free(n);
    } else if (test == 5) {
        ExprNode *n = binary(OP_EQ, type_bool, integer(1), integer(2));
        gen_while_start(n); gen_inst1("WRITE", "string@BAD"); gen_while_end();
        gen_inst1("WRITE", "string@END"); expr_free(n);
    } else if (test == 6 || test == 7) {
        ExprNode *n = binary(OP_EQ, type_bool, integer(1), integer(2));
        for (int i = 0; i < 300; ++i) {
            if (test == 6) gen_if_start(n); else gen_while_start(n);
        }
        for (int i = 0; i < 300; ++i) {
            if (test == 6) { gen_if_else(); gen_if_end(); }
            else gen_while_end();
        }
        expr_free(n);
    } else if (test == 8) {
        /* The entry is emitted first; the colliding function follows it. */
    } else return 99;
    gen_func_end();
    if (test == 8) {
        if (!symtable_insert_func(t, "readLine_end", type_unit, 0, NULL)) return 99;
        gen_func_start(t, "readLine_end", type_unit); gen_func_end();
    }
    buffer_free(&code_buff); symtable_destroy(t); return 0;
}
'''


def frontend_driver(src):
    declarations = re.findall(r'\bvoid\s+(\w+)\s*\(([^;{}]*)\)\s*;',
                              (src / 'code_generator.h').read_text(encoding='utf-8-sig'))
    stubs = []
    for name, parameters in declarations:
        names = [] if parameters.strip() in ('', 'void') else [
            re.findall(r'\b\w+\b', p)[-1] for p in parameters.split(',')]
        stubs.append('void ' + name + '(' + parameters + ') {'
                     + ''.join('(void)' + n + ';' for n in names) + '}')
    return '''#include "parser.h"
#include "code_generator.h"
#include "dynamic_buffer.h"
#include <stdio.h>
''' + '\n'.join(stubs) + r'''
int main(void) {
    DynamicBuffer input;
    dynamic_buffer_init(&input);
    int c;
    while ((c = getchar()) != EOF) {
        if (!dynamic_buffer_append_char(&input, (char)c)) {
            dynamic_buffer_free(&input);
            return 99;
        }
    }
    if (ferror(stdin)) {
        dynamic_buffer_free(&input);
        return 99;
    }
    parse(input.data != NULL ? input.data : "", input.length);
    dynamic_buffer_free(&input);
    return 0;
}
'''


class BuildError(Exception):
    pass


class Runner:
    def __init__(self, args):
        self.a = args
        self.root = args.project.resolve()
        self.src = self.root / 'src'
        self.out = Path(tempfile.mkdtemp(prefix='run-', dir=self.root / 'tests/results'))
        self.results = []
        self.programs = json.loads(Path(__file__).with_name('programs.json').read_text(encoding='utf-8'))
        self.vm = shlex.split(args.interpreter) if args.interpreter else None
        self.color = args.color == 'always' or (args.color == 'auto' and sys.stdout.isatty())

    def show(self, status, name, reason=''):
        words = {'PASS': 'ANO', 'FAIL': 'NE', 'SKIP': 'NEOVERENO'}
        colors = {'PASS': '\033[32m', 'FAIL': '\033[31m', 'SKIP': '\033[33m'}
        label = words[status]
        if self.color:
            label = colors[status] + label + '\033[0m'
        print(f'[{label}] {name}' + (': ' + reason if reason else ''), flush=True)

    def record(self, status, name, reason='', folder=None, **facts):
        self.results.append(dict(status=status, name=name, reason=reason,
                                 folder=str(folder) if folder else None, **facts))
        self.show(status, name, reason)

    def execute(self, argv, data, folder, label, timeout=None):
        folder.mkdir(parents=True, exist_ok=True)
        try:
            p = subprocess.run(argv, input=data, capture_output=True,
                               timeout=timeout or self.a.timeout, cwd=self.root)
            rc, out, err = p.returncode, p.stdout, p.stderr
        except subprocess.TimeoutExpired as e:
            rc, out, err = None, e.stdout or b'', (e.stderr or b'') + b'\nTIMEOUT\n'
        except OSError as e:
            rc, out, err = None, b'', str(e).encode()
        (folder / (label + '.stdout')).write_bytes(out)
        (folder / (label + '.stderr')).write_bytes(err)
        return rc, out, err

    def build(self, name, code, sources, sanitize=True):
        folder = self.out / '_build'
        folder.mkdir(exist_ok=True)
        driver, binary = folder / (name + '.c'), folder / name
        driver.write_text(code, encoding='utf-8')
        argv = shlex.split(self.a.cc) + ['-std=c23', '-O1', '-g', '-Wall', '-Wextra',
                                       '-Wpedantic', '-I', str(self.src)]
        if self.a.sanitize and sanitize:
            argv += ['-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                     '-fno-omit-frame-pointer']
        elif name == 'codegen':
            argv += ['-fsanitize=undefined', '-fno-sanitize-recover=undefined']
        argv += [str(self.src / s) for s in dict.fromkeys(sources)] + [str(driver), '-o', str(binary)]
        rc, _, _ = self.execute(argv, b'', folder, name, 60)
        if rc != 0:
            raise BuildError('Preklad driveru selhal; viz ' + str(folder / (name + '.stderr')))
        return binary

    def check(self, name, argv, data=b'', expected_rc=0, expected=None):
        folder = self.out / name
        folder.mkdir(parents=True, exist_ok=True)
        (folder / 'input').write_bytes(data)
        (folder / 'expected.rc').write_text(str(expected_rc))
        if expected is not None:
            (folder / 'expected.stdout').write_bytes(expected)
        rc, out, err = self.execute(argv, data, folder, 'actual')
        reason = ''
        if rc != expected_rc:
            reason = f'Ocekavan kod {expected_rc}, skutecny {rc}.'
        elif expected is not None and out != expected:
            reason = f'Lisi se vystup: ocekavano {expected[:100]!r}, skutecnost {out[:100]!r}.'
        self.record('FAIL' if reason else 'PASS', name, reason, folder,
                    expected_rc=expected_rc, actual_rc=rc)
        return not reason, out, folder

    def unit(self, suite, code, sources, names):
        binary = self.build(suite, code, sources)
        for i, name in enumerate(names):
            self.check(suite + '/' + name, [str(binary), str(i)], expected=b'')

    def scanner(self):
        binary = self.build('scanner', SCANNER_C, ['scanner.c'] + COMMON)
        examples = [
            ('empty', '', ['TOKEN_EOF']),
            ('names', 'abc val var Int', ['TOKEN_IDENTIFIER|616263', 'TOKEN_KW_VAL', 'TOKEN_KW_VAR', 'TOKEN_TYPE_INT', 'TOKEN_EOF']),
            ('numbers', '42 1.5 .5 2e2', ['TOKEN_INT_LITERAL|42', 'TOKEN_DOUBLE_LITERAL|1.5', 'TOKEN_DOUBLE_LITERAL|0.5', 'TOKEN_DOUBLE_LITERAL|200', 'TOKEN_EOF']),
            ('string', r'"A\n"', ['TOKEN_STRING_LITERAL|410a', 'TOKEN_EOF']),
            ('quotes', '"' * 7, ['TOKEN_STRING_LITERAL|22', 'TOKEN_EOF']),
            ('indent', 'a\n  b\nc\n', ['TOKEN_IDENTIFIER|61', 'TOKEN_EOL', 'TOKEN_INDENT', 'TOKEN_IDENTIFIER|62', 'TOKEN_EOL', 'TOKEN_DEDENT', 'TOKEN_IDENTIFIER|63', 'TOKEN_EOL', 'TOKEN_EOF']),
            ('operators', '= == != < <= > >= + - * / ;', ['TOKEN_' + n for n in ['ASSIGN', 'EQ', 'NEQ', 'LT', 'LEQ', 'GT', 'GEQ', 'PLUS', 'MINUS', 'MUL', 'DIV', 'SEMICOLON', 'EOF']]),
            ('nested_comment', 'a /* x /* y */ z */ b', ['TOKEN_IDENTIFIER|61', 'TOKEN_IDENTIFIER|62', 'TOKEN_EOF']),
        ]
        for name, source, expected in examples:
            self.check('scanner/' + name, [str(binary)], source.encode(),
                       expected=('\n'.join(expected) + '\n').encode())
        for i, source in enumerate(['?', '01', '01.5', '1e+', '1_', r'"\q"', '"unfinished', '/* unfinished', 'a\n\tb\n']):
            self.check('scanner/invalid_' + str(i), [str(binary)], source.encode(), 1)
        self.check('scanner/newline_flag', [str(binary), 'flags'], b'a\nb\n',
                   expected=b'TOKEN_IDENTIFIER|61|N0\nTOKEN_EOL|N0\nTOKEN_IDENTIFIER|62|N1\nTOKEN_EOL|N0\nTOKEN_EOF|N0\n')
        hello = self.programs[0]['source']
        self.check('scanner/whole_program', [str(binary)], hello.encode())
        for i, case in enumerate(self.programs):
            if case['compile_rc'] == 1:
                self.check(f"scanner/program_{i:03d}_{case['name']}",
                           [str(binary)], case['source'].encode(), 1)
        fixtures = Path(__file__).with_name('scanner')
        fixture_binary = self.build('scanner_fixtures',
            (fixtures / 'scanner_test_driver.c').read_text(encoding='utf-8'),
            ['scanner.c'] + COMMON)
        for i, path in enumerate(sorted((fixtures / 'cases').glob('*/*.ifj26'))):
            self.check(f'scanner/fixture_{i:03d}_{path.stem}', [str(fixture_binary), str(path)],
                       expected_rc=int(path.with_suffix('.rc').read_text()),
                       expected=path.with_suffix('.expected').read_bytes())


    def expressions(self):
        binary = self.build('expressions', EXPRESSIONS_C,
                            ['exp_parser.c', 'valmem.c', 'paramlists.c', 'expr_tree.c', 'semantic.c', 'scanner.c'] + TABLES + COMMON,
                            sanitize=False)
        tests = [('precedence', '2 + 3 * 4', 0, b'(+ int 2 (* int 3 4))\n'),
                 ('parentheses', '(2 + 3) * 4', 0, b'(* int (+ int 2 3) 4)\n'),
                 ('conversion', 'x + y', 0, b'(+ double (int2float (var x$0 int)) (var y$1 double))\n'),
                 ('concat', '"A" + "B"', 0, b'(concat string "A" "B")\n'),
                 ('undefined', 'missing + 1', 3, None),
                 ('bad_type', '"A" - 1', 6, None),
                 ('unfinished', '1 +', 2, None)]
        for name, source, rc, output in tests:
            self.check('expressions/' + name, [str(binary)], (source + '\n').encode(), rc, output)

    def frontend(self, extended=False):
        sources = ['parser.c', 'scanner.c', 'exp_parser.c', 'valmem.c', 'paramlists.c', 'expr_tree.c', 'semantic.c'] + TABLES + COMMON
        binary = self.build('frontend', frontend_driver(self.src), sources, sanitize=False)
        if extended:
            from semantic_cases import cases
            for i, (group, name, source, rc) in enumerate(cases()):
                clean = re.sub(r'[^A-Za-z0-9_.-]', '_', group + '_' + name)[:90]
                self.check(f'semantic_programs/{i:04d}_{clean}',
                           [str(binary)], source.encode(), rc, b'')
        else:
            for case in self.programs:
                if case['compile_rc'] != 1:
                    self.check('parser/' + case['name'], [str(binary)],
                               case['source'].encode(), case['compile_rc'], b'')

    def target(self, name, code, expected, runtime_rc, folder, stdin=b''):
        path = folder / 'program.ifjcode26'
        path.write_bytes(code)
        (folder / 'expected.stdout').write_bytes(expected)
        (folder / 'expected.runtime_rc').write_text(str(runtime_rc))
        (folder / 'runtime.stdin').write_bytes(stdin)
        lines = [line.split(b'#', 1)[0].strip() for line in code.splitlines()]
        if next((line for line in lines if line), b'').lower() != b'.ifjcode26':
            self.record('FAIL', name, 'Chybi hlavicka .IFJcode26.', folder)
            return
        if not self.vm:
            self.record('SKIP', name, 'Chybi interpret; beh a vystup nebyly overeny.', folder)
            return
        rc, out, _ = self.execute(self.vm + [str(path)], stdin, folder, 'interpreter')
        reason = ''
        if rc != runtime_rc:
            reason = f'Interpret: ocekavan kod {runtime_rc}, skutecny {rc}.'
        elif out != expected:
            reason = f'Vystup: ocekavano {expected[:100]!r}, skutecnost {out[:100]!r}.'
        self.record('FAIL' if reason else 'PASS', name, reason, folder,
                    expected_rc=runtime_rc, actual_rc=rc)

    def codegen(self):
        binary = self.build('codegen', CODEGEN_C,
                            ['code_generator.c', 'code_label_counter.c', 'code_buffer.c', 'expr_tree.c'] + TABLES + COMMON)
        tests = [('arithmetic', b'14'), ('division', b'-2'), ('escaping', b'A #\\\nB\t'),
                 ('double', b'4.00'), ('if', b'T'), ('while', b'END'),
                 ('deep_if_300', b''), ('deep_while_300', b''), ('label_collision', b'')]
        for i, (name, expected) in enumerate(tests):
            name = 'codegen/' + name
            folder = self.out / name
            rc, code, _ = self.execute([str(binary), str(i)], b'', folder, 'generator')
            if rc != 0:
                self.record('FAIL', name, f'Generator skoncil kodem {rc}.', folder)
            else:
                self.target(name, code, expected, 0, folder)

    def integration(self):
        folder = self.out / '_build'
        rc, _, _ = self.execute(['make', '-B', 'all', 'CC=' + self.a.cc], b'', folder, 'project', 60)
        if rc != 0 or not (self.root / 'ifj26').is_file():
            raise BuildError('make selhal; viz ' + str(folder / 'project.stderr'))
        for case in self.programs:
            name, source, expected_rc = case['name'], case['source'], case['compile_rc']
            expected = bytes.fromhex(case['stdout_hex'])
            runtime_rc, stdin = case['runtime_rc'], bytes.fromhex(case['stdin_hex'])
            name = 'integration/' + name
            folder = self.out / name
            folder.mkdir(parents=True, exist_ok=True)
            (folder / 'source.ifj26').write_text(source)
            rc, code, _ = self.execute([str(self.root / 'ifj26')], source.encode(), folder, 'compiler')
            if rc != expected_rc:
                self.record('FAIL', name, f'Preklad: ocekavan kod {expected_rc}, skutecny {rc}.', folder)
            elif expected_rc:
                self.record('PASS', name, 'Spravny chybovy kod prekladace.', folder)
            else:
                self.target(name, code, expected, runtime_rc, folder,
                            stdin)

    def run(self):
        for suite in SUITES if self.a.suite == 'all' else [self.a.suite]:
            print('\n' + suite.upper(), flush=True)
            try:
                if suite == 'helpers':
                    self.unit(suite, HELPERS_C, COMMON, ['capacity', 'buffer', 'strings', 'tokens'])
                elif suite == 'tables':
                    self.unit(suite, TABLES_C, TABLES, ['collisions', 'model_10000', 'scopes_and_ids', 'parameter_names'])
                elif suite == 'semantic':
                    self.unit(suite, SEMANTIC_C, ['semantic.c', 'expr_tree.c'] + TABLES,
                              ['numeric_conversion', 'strings', 'division', 'variables', 'coercion_condition'])
                elif suite == 'parser': self.frontend()
                elif suite == 'semantic_programs': self.frontend(True)
                else: getattr(self, suite)()
            except (BuildError, OSError, ValueError) as e:
                self.record('FAIL', suite + '/build', str(e))
        counts = Counter(r['status'] for r in self.results)
        print('\nSOUHRN')
        for suite in SUITES:
            group = [r for r in self.results if r['name'].split('/')[0] == suite]
            if group:
                c = Counter(r['status'] for r in group)
                print(f"{suite}: ANO {c['PASS']}/{len(group)}, NE {c['FAIL']}, NEOVERENO {c['SKIP']}")
        status = 'FAIL' if counts['FAIL'] else ('SKIP' if counts['SKIP'] else 'PASS')
        self.show(status, 'CELKEM', f"ANO {counts['PASS']}/{len(self.results)}, NE {counts['FAIL']}, NEOVERENO {counts['SKIP']}")
        print('Podrobnosti:', self.out)
        (self.out / 'report.json').write_text(json.dumps({'counts': {s: counts[s] for s in ('PASS', 'FAIL', 'SKIP')}, 'results': self.results}, ensure_ascii=False, indent=2), encoding='utf-8')
        return 1 if counts['FAIL'] else (2 if counts['SKIP'] else 0)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--project', type=Path, default=Path(__file__).resolve().parent.parent)
    p.add_argument('--suite', choices=['all'] + SUITES, default='all')
    p.add_argument('--cc', default=os.environ.get('CC', 'gcc'))
    p.add_argument('--interpreter', help='Cesta k interpretu IFJcode26.')
    p.add_argument('--timeout', type=float, default=5)
    p.add_argument('--sanitize', action='store_true', help='ASan/UBSan pro vlastni C drivery; ne pro frontend a expressions.')
    p.add_argument('--color', choices=['auto', 'always', 'never'], default='auto')
    a = p.parse_args()
    if not math.isfinite(a.timeout) or a.timeout <= 0:
        p.error('--timeout musi byt kladny.')
    if not (a.project / 'src/scanner.h').is_file():
        p.error('--project musi ukazovat na slozku se src a Makefile.')
    if not a.interpreter:
        for candidate in [a.project / 'ic26int-linux-x86_64', a.project / 'tools/ic26int-linux-x86_64']:
            if candidate.is_file() and os.access(candidate, os.X_OK):
                a.interpreter = str(candidate.resolve()); break
        if not a.interpreter:
            a.interpreter = shutil.which('ic26int')
    if a.interpreter:
        parts = shlex.split(a.interpreter)
        if not parts or not shutil.which(parts[0]):
            p.error('Interpret nebyl nalezen nebo neni spustitelny.')
        parts[0] = str(Path(shutil.which(parts[0])).resolve())
        a.interpreter = shlex.join(parts)
    (a.project / 'tests/results').mkdir(parents=True, exist_ok=True)
    return Runner(a).run()


if __name__ == '__main__':
    raise SystemExit(main())
