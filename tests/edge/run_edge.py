#!/usr/bin/env python3
# Project:    IFJ26
# Filename:   run_edge.py
#
# prompt: "Pridej vlastni testy pro krajni pripady, white box i black box.
#          Soustred se na klasicke i specialne vybrane hodnoty pro rozbiti
#          programu a necekane stavy. Ocekavane vysledky ber ze zadani."
#
#   python3 tests/edge/run_edge.py --interpreter ./ic26int-linux-x86_64
#   python3 tests/edge/run_edge.py --interpreter ./ic26int-linux-x86_64 -v
#   python3 tests/edge/run_edge.py --filter nan --mutations 0
#
# Build goes to a temp directory, nothing is written into the repository.
import argparse
import collections
import concurrent.futures
import glob
import json
import os
import random
import subprocess
import sys
import tempfile
import time

HEAD = "import ifj26.*\n"
CASES = []
ALLOWED_COMPILER = {0, 1, 2, 3, 4, 5, 6, 7, 10}
# a compiled program may only end with these codes, the rest is a generator bug
ALLOWED_RUNTIME = {0, 40, 57, 58}


def program(body, helpers=""):
    lines = "\n".join("  " + line if line else line for line in body.split("\n"))
    return HEAD + (helpers + "\n" if helpers else "") + "@main def m(): Unit =\n" + lines + "\n"


def add(group, name, source, rc=0, out=None, stdin="", run_rc=0):
    expected = set(rc) if isinstance(rc, (set, tuple, list)) else {rc}
    CASES.append({"id": f"{group}/{name}", "group": group, "source": source, "rc": expected,
                  "out": out, "stdin": stdin, "run_rc": run_rc})


def ok(group, name, body, out, helpers="", stdin=""):
    add(group, name, program(body, helpers), 0, out, stdin)


def bad(group, name, body, rc, helpers=""):
    add(group, name, program(body, helpers), rc)


def run_err(group, name, body, run_rc, out=None, helpers="", stdin=""):
    add(group, name, program(body, helpers), 0, out, stdin, run_rc)


# ---------------------------------------------------------------- black box
def black_lexer():
    g = "bb_lexer"
    for name, text, out in [
        ("int_underscores", "1_000_000", "1000000"), ("int_double_underscore", "1__0", "10"),
        ("int_zero", "0", "0"), ("int_max", "2147483647", "2147483647"),
        ("int_min_expr", "-2147483647 - 1", "-2147483648"),
        ("int_min_literal", "-2147483648", "-2147483648"),
    ]:
        ok(g, name, f"print({text})", out)
    for name, text in [("int_trailing_underscore", "1_"), ("int_leading_zero", "01"),
                       ("int_beyond_64bit", "99999999999999999999"), ("double_beyond_range", "1e999"),
                       ("exp_empty", "1e"), ("exp_sign_only", "1e+"), ("frac_exp_empty", "1.5e")]:
        bad(g, name, f"print({text})", 1)
    for name, text, out in [
        ("dbl_suffix_d", "1.5d", "1.50"), ("dbl_suffix_D", "1.5D", "1.50"), ("dbl_exp", "1e3", "1000.00"),
        ("dbl_exp_plus", "1E+2", "100.00"), ("dbl_exp_minus", "1e-2", "0.01"), ("dbl_no_int", ".5", "0.50"),
        ("dbl_no_int_exp", ".5e1", "5.00"), ("dbl_underscores", "1_0.5_0", "10.50"),
        ("dbl_exp_underscore", "1e1_0", "10000000000.00"), ("dbl_zero", "0.0", "0.00"),
        ("dbl_exp_suffix", "1e2d", "100.00"),
    ]:
        ok(g, name, f"print(toStr({text}))", out)
    bad(g, "dbl_suffix_f", "print(1.5f)", (1, 2))
    bad(g, "dbl_dot_only", "print(1.)", (1, 2))
    ok(g, "id_underscore_start", "val _x = 1\nprint(_x)", "1")
    ok(g, "id_case_sensitive", "val a = 1\nval A = 2\nprint(a)\nprint(A)", "12")
    ok(g, "id_very_long", f"val {'a' * 10000} = 7\nprint({'a' * 10000})", "7")
    bad(g, "id_lone_underscore", "val _ = 1", (1, 2))
    for word in ("def", "do", "Double", "else", "if", "import", "Int", "return",
                 "String", "then", "Unit", "val", "var", "while"):
        bad(g, "keyword_" + word, f"val {word} = 1", 2)
    bad(g, "unknown_char_dollar", "val a$ = 1", 1)
    bad(g, "unknown_char_hash", "val a = #", 1)
    bad(g, "unknown_char_backslash", "val a = \\", 1)
    add(g, "nul_byte", program("print(1)").replace("print", "pr\0int"), 1)

    for name, text, out in [
        ("str_tab", r'"a\tb"', "a\tb"), ("str_backslash", r'"\\"', "\\"), ("str_quote", r'"\""', '"'),
        ("str_newline", r'"\n"', "\n"), ("str_cr", r'"\r"', "\r"), ("str_empty", '""', ""),
        ("str_hash_space", '"a b#c"', "a b#c"), ("str_literal_bs_n", r'"\\n"', "\\n"),
        ("str_all_escapes", r'"\\\"\n\r\t"', '\\"\n\r\t'),
    ]:
        ok(g, name, f"print({text})", out)
    bad(g, "str_bad_escape", r'print("\q")', 1)
    bad(g, "str_hex_escape", r'print("\x41")', 1)
    bad(g, "str_unterminated", 'print("abc', 1)
    add(g, "str_real_newline", HEAD + '@main def m(): Unit =\n  print("a\nb")\n', 1)
    add(g, "raw_no_escape", HEAD + '@main def m(): Unit =\n  print("""a\\nb""")\n', 0, "a\\nb")
    add(g, "raw_multiline", HEAD + '@main def m(): Unit =\n  print("""a\n b""")\n', 0, "a\n b")
    add(g, "raw_unterminated", HEAD + '@main def m(): Unit =\n  print("""abc\n', 1)
    add(g, "str_100k", program(f'print("{"x" * 100000}")'), 0, "x" * 100000)

    ok(g, "comment_after_stmt", "print(1) // x", "1")
    ok(g, "comment_nested", "/* a /* b */ c */ print(1)", "1")
    ok(g, "comment_inline", "print(/* c */ 1)", "1")
    ok(g, "comment_splits_tokens", "val/* c */x = 1\nprint(x)", "1")
    bad(g, "comment_unterminated", "print(1) /* a", 1)
    bad(g, "comment_nested_unterminated", "/* a /* b */ print(1)", 1)
    add(g, "comment_multiline_own_line", HEAD + "@main def m(): Unit =\n  /* a\n  b */\n  print(1)\n", 0, "1")
    add(g, "comment_multiline_then_token", HEAD + "@main def m(): Unit =\n  /* a\n  b */ print(1)\n", (1, 2))


def black_layout():
    g = "bb_layout"
    add(g, "tab_indent", HEAD + "@main def m(): Unit =\n\tprint(1)\n", 1)
    add(g, "blank_line_spaces", HEAD + "@main def m(): Unit =\n  print(1)\n      \n  print(2)\n", 0, "12")
    add(g, "blank_line_tab", HEAD + "@main def m(): Unit =\n  print(1)\n\t\n  print(2)\n", 0, "12")
    add(g, "comment_any_indent", HEAD + "@main def m(): Unit =\n  print(1)\n        // x\n// y\n  print(2)\n", 0, "12")
    add(g, "over_indent", HEAD + "@main def m(): Unit =\n  print(1)\n    print(2)\n", (1, 2))
    add(g, "bad_dedent", HEAD + "@main def m(): Unit =\n    print(1)\n  print(2)\n", (1, 2))
    add(g, "no_final_newline", HEAD + "@main def m(): Unit =\n  print(1)", 0, "1")
    add(g, "no_newline_nested", HEAD + "@main def m(): Unit =\n  if 1 < 2 then\n    print(1)\n  else\n    print(2)", 0, "1")
    add(g, "one_line_at_eof", HEAD + "def f(): Int = 1\n@main def m(): Unit = print(f())", 0, "1")
    ok(g, "semicolons", "print(1); print(2); print(3)", "123")
    bad(g, "double_semicolon", "print(1);; print(2)", 2)
    bad(g, "leading_semicolon", "; print(1)", 2)
    bad(g, "leading_plus", "val x = 1\n+ 2", 2)
    bad(g, "leading_mul", "val x = 1\n* 2", 2)
    bad(g, "empty_then", "if 1 < 2 then\nelse\n  ()", 2)
    bad(g, "empty_else", "if 1 < 2 then\n  ()\nelse", 2)
    bad(g, "empty_while", "while 1 < 0 do", 2)
    bad(g, "then_same_line", "if 1 < 2 then print(1)\nelse\n  ()", 2)
    bad(g, "while_same_line", "while 1 < 0 do print(1)", 2)
    bad(g, "missing_else", "if 1 < 2 then\n  ()", 2)
    bad(g, "else_indented", "if 1 < 2 then\n  ()\n  else\n    ()", 2)
    ok(g, "call_over_lines", "print(\n    1\n  )", "1")
    ok(g, "trailing_comma_newline", "print(f(1,\n    2,\n  ))", "3", "def f(a: Int, b: Int): Int = a + b")
    bad(g, "trailing_comma_same_line", "print(f(1, 2,))", 2, "def f(a: Int, b: Int): Int = a + b")
    bad(g, "only_comma", "print(,)", 2)
    bad(g, "double_comma", "print(f(1,,2))", 2, "def f(a: Int, b: Int): Int = a + b")
    add(g, "param_trailing_newline", HEAD + "def f(a: Int,\n  b: Int,\n  ): Int = a + b\n@main def m(): Unit = print(f(1, 2))\n", 0, "3")
    add(g, "param_trailing_same_line", HEAD + "def f(a: Int,): Int = a\n@main def m(): Unit = print(f(1))\n", 2)
    add(g, "param_only_comma", HEAD + "def f(,): Int = 1\n@main def m(): Unit = print(f())\n", 2)


def black_structure():
    g = "bb_structure"
    for name, source, rc in [
        ("empty", "", 2), ("spaces", "   \n\n  ", 2), ("comment_only", "// x\n/* y */\n", 2),
        ("no_prolog", "@main def m(): Unit = ()\n", 2), ("prolog_only", HEAD, 3),
        ("prolog_twice", HEAD + HEAD + "@main def m(): Unit = ()\n", 2),
        ("wrong_package", "import ifj25.*\n@main def m(): Unit = ()\n", 2),
        ("prolog_no_star", "import ifj26\n@main def m(): Unit = ()\n", 2),
        ("top_level_stmt", HEAD + "print(1)\n@main def m(): Unit = ()\n", 2),
        ("no_main", HEAD + "def f(): Unit = ()\n", 3),
        ("two_mains", HEAD + "@main def a(): Unit = ()\n@main def b(): Unit = ()\n", 4),
        ("main_param", HEAD + "@main def a(x: Int): Unit = ()\n", 5),
        ("main_int", HEAD + "@main def a(): Int = 1\n", 6),
        ("main_inferred_int", HEAD + "@main def a() = 1\n", 6),
        ("main_before_nondef", HEAD + "@main val x = 1\n", 2),
        ("at_space_main", HEAD + "@ main def a(): Unit = ()\n", (1, 2)),
        ("at_other", HEAD + "@other def a(): Unit = ()\n", (1, 2)),
        ("duplicate_function", HEAD + "def f(): Unit = ()\ndef f(): Int = 1\n@main def m(): Unit = ()\n", 4),
        ("redefine_builtin", HEAD + "def print(x: Int): Unit = ()\n@main def m(): Unit = ()\n", 4),
        ("duplicate_param", HEAD + "def f(a: Int, a: Int): Unit = ()\n@main def m(): Unit = ()\n", 4),
        ("unknown_param_type", HEAD + "def f(a: Foo): Unit = ()\n@main def m(): Unit = ()\n", 3),
        ("unknown_return_type", HEAD + "def f(): Foo = 1\n@main def m(): Unit = ()\n", 3),
        ("param_without_type", HEAD + "def f(a): Unit = ()\n@main def m(): Unit = ()\n", 2),
        ("missing_equals", HEAD + "def f(): Unit\n@main def m(): Unit = ()\n", 2),
        ("one_line_return", HEAD + "def f(): Int = return 1\n@main def m(): Unit = ()\n", 2),
    ]:
        add(g, name, source, rc)
    add(g, "prolog_whitespace", "import   ifj26 . *\n@main def m(): Unit = print(1)\n", 0, "1")
    add(g, "prolog_after_comments", "// a\n/* b */\n\nimport ifj26.*\n@main def m(): Unit = print(1)\n", 0, "1")
    add(g, "main_on_own_line", HEAD + "@main\ndef m(): Unit = print(1)\n", 0, "1")
    add(g, "main_any_name", HEAD + "@main def zzz(): Unit = print(1)\n", 0, "1")
    add(g, "main_inferred_unit", HEAD + "@main def m() =\n  print(1)\n", 0, "1")
    add(g, "main_first_helper_after", HEAD + "@main def m(): Unit = print(f())\ndef f(): Int = 4\n", 0, "4")


def black_variables():
    g = "bb_variables"
    ok(g, "double_from_int", "val x: Double = 1\nprint(toStr(x))", "1.00")
    ok(g, "assign_int_to_double", "var x: Double = 1.5\nx = 2\nprint(toStr(x))", "2.00")
    ok(g, "double_from_int_call", "val x: Double = f()\nprint(toStr(x))", "3.00", "def f(): Int = 3")
    ok(g, "assign_int_call", "var x = 1.5\nx = f()\nprint(toStr(x))", "3.00", "def f(): Int = 3")
    ok(g, "infer_from_call", "val x = f()\nprint(toStr(x + 1))", "2.50", "def f(): Double = 1.5")
    ok(g, "unit_variable", "val u = ()\nprint(u)", "()")
    ok(g, "unit_annotated", "val u: Unit = ()\nprint(u)", "()")
    ok(g, "unit_from_print", "val u = print(1)\nprint(u)", "1()")
    ok(g, "shadow_if", "val x = 1\nif 1 < 2 then\n  val x = 2\n  print(x)\nelse\n  ()\nprint(x)", "21")
    ok(g, "shadow_type_change", 'val x = 1\nif 1 < 2 then\n  val x = "s"\n  print(x)\nelse\n  ()\nprint(x)', "s1")
    ok(g, "shadow_uses_outer", "val x = 5\nif 1 < 2 then\n  val x = x + 1\n  print(x)\nelse\n  ()", None)
    ok(g, "shadow_in_while", "var i = 0\nwhile i < 2 do\n  val i2 = i * 10\n  print(i2)\n  i = i + 1", "010")
    ok(g, "assign_outer_in_block", "var x = 1\nif 1 < 2 then\n  x = 2\nelse\n  ()\nprint(x)", "2")
    ok(g, "branch_same_name", "if 1 < 2 then\n  val a = 1\n  print(a)\nelse\n  val a = 2\n  print(a)", "1")
    bad(g, "int_from_double", "val x: Int = 1.5", 6)
    bad(g, "string_from_int", 'val x: String = 1', 6)
    bad(g, "assign_other_type", "var x = 1\nx = 1.5", 6)
    bad(g, "assign_val", "val x = 1\nx = 2", 7)
    bad(g, "assign_param", "f(1)", 7, "def f(a: Int): Unit =\n  a = 2")
    bad(g, "assign_undefined", "x = 1", 3)
    bad(g, "use_undefined", "print(x)", 3)
    bad(g, "use_before_def", "print(x)\nval x = 1", 3)
    bad(g, "self_init", "val x = x", 3)
    bad(g, "redefine", "val x = 1\nval x = 2", 4)
    bad(g, "redefine_val_var", "val x = 1\nvar x = 2", 4)
    bad(g, "redefine_param", "f(1)", 4, "def f(a: Int): Unit =\n  val a = 2")
    bad(g, "unknown_type", "val x: Foo = 1", 3)
    bad(g, "escape_if", "if 1 < 2 then\n  val a = 1\nelse\n  ()\nprint(a)", 3)
    bad(g, "escape_while", "while 1 < 0 do\n  val a = 1\nprint(a)", 3)
    bad(g, "function_as_variable", "print(f)", 3, "def f(): Int = 1")
    bad(g, "variable_as_function", "val g = 1\nprint(g())", 3)
    bad(g, "no_initializer", "val x", 2)
    bad(g, "no_initializer_typed", "var x: Int", 2)


def black_expressions():
    g = "bb_expressions"
    for name, text, out in [
        ("prec_mul", "2 + 3 * 4", "14"), ("parens", "(2 + 3) * 4", "20"), ("left_sub", "2 - 3 - 4", "-5"),
        ("left_div", "100 / 10 / 5", "2"), ("neg_mul", "-2 * 3", "-6"), ("mul_neg", "2 * -3", "-6"),
        ("double_neg", "- -3", "3"), ("neg_parens", "-(-3)", "3"), ("sub_neg", "1 - -1", "2"),
        ("sub_neg_tight", "1--1", "2"), ("trunc_pos", "7 / 2", "3"), ("trunc_neg", "-7 / 2", "-3"),
        ("trunc_neg2", "7 / -2", "-3"), ("trunc_both", "-7 / -2", "3"), ("zero_num", "0 / 5", "0"),
        ("redundant_parens", "(((((1)))))", "1"), ("neg_zero_int", "-0", "0"),
    ]:
        ok(g, name, f"print({text})", out)
    for name, text, out in [
        ("dbl_div", "7.0 / 2", "3.50"), ("int_dbl_div", "1 / 2.0", "0.50"), ("mixed_add", "1 + 2.5", "3.50"),
        ("mixed_sub", "1.5 - 1", "0.50"), ("mixed_mul", "2 * 1.5", "3.00"), ("mixed_nested", "(1 + 1) * 1.5", "3.00"),
        ("int_div_then_dbl", "1 / 2 + 0.5", "0.50"), ("neg_double", "-1.5", "-1.50"),
    ]:
        ok(g, name, f"print(toStr({text}))", out)
    ok(g, "concat", 'print("a" + "b" + "c")', "abc")
    ok(g, "concat_empty", 'print("" + "")', "")
    for name, cond, out in [
        ("lt", "1 < 2", "T"), ("le_eq", "2 <= 2", "T"), ("gt", "1 > 2", "F"), ("ge_eq", "2 >= 2", "T"),
        ("eq_int_dbl", "1 == 1.0", "T"), ("ne_int_dbl", "1 != 1.5", "T"), ("str_lt", '"a" < "b"', "T"),
        ("str_prefix", '"a" < "ab"', "T"), ("str_eq", '"x" == "x"', "T"), ("str_ge", '"b" >= "a"', "T"),
        ("str_empty_lt", '"" < "a"', "T"), ("unit_eq", "() == ()", "T"), ("unit_ne", "() != ()", "F"),
        ("arith_in_rel", "1 + 1 == 2", "T"), ("rel_in_parens", "(1 < 2)", "T"),
    ]:
        ok(g, "cond_" + name, f'if {cond} then\n  print("T")\nelse\n  print("F")', out)
    # IEEE 754: comparisons with NaN are false except !=
    nan = "val big = 1e308 * 10.0\nval n = big - big\n"
    for name, cond, out in [("nan_lt", "n < 1.0", "F"), ("nan_gt", "n > 1.0", "F"), ("nan_le", "n <= 1.0", "F"),
                            ("nan_ge", "n >= 1.0", "F"), ("nan_eq", "n == n", "F"), ("nan_ne", "n != n", "T")]:
        ok(g, name, nan + f'if {cond} then\n  print("T")\nelse\n  print("F")', out)
    for name, text, rc in [
        ("add_str_int", '"a" + 1', 6), ("sub_str", '"a" - "b"', 6), ("mul_str", '"a" * 2', 6),
        ("unit_arith", "() + 1", 6), ("neg_string", '-"a"', 6), ("neg_unit", "-()", 6),
        ("mixed_compare", '1 < "a"', 6), ("eq_str_int", '"a" == 1', 6), ("eq_unit_int", "() == 1", 6),
        ("rel_stored", "1 < 2", 6), ("chain_lt", "1 < 2 < 3", 2), ("chain_eq", "1 == 2 == 3", 2),
        ("no_right", "1 +", 2), ("no_left", "* 2", 2), ("open_paren", "(1", 2), ("close_paren", "1)", 2),
        ("two_values", "1 2", 2), ("empty_mul", "1 * ()", 6), ("op_op", "1 * / 2", 2),
    ]:
        bad(g, name, f"val x = {text}", rc)
    for name, cond in [("if_int", "1"), ("if_str", '"a"'), ("if_unit", "()"), ("if_arith", "1 + 1")]:
        bad(g, name, f"if {cond} then\n  ()\nelse\n  ()", 6)
    bad(g, "while_double", "while 1.5 do\n  ()", 6)
    bad(g, "print_relation", "print(1 < 2)", 6)
    bad(g, "user_arg_relation", "f(1 < 2)", 5, "def f(a: Int): Unit = ()")
    run_err(g, "div_zero", "print(1 / 0)", 57)
    run_err(g, "div_zero_var", "val z = 0\nprint(5 / z)", 57)
    run_err(g, "div_zero_double", "print(toStr(1.0 / 0.0))", 57)
    run_err(g, "div_zero_mixed", "print(toStr(1 / 0.0))", 57)
    run_err(g, "div_zero_after_output", 'print("a")\nprint(1 / 0)', 57, "a")


def black_functions():
    g = "bb_functions"
    fact = "def fact(n: Int): Int =\n  if n < 2 then\n    return 1\n  else\n    return n * fact(n - 1)"
    ok(g, "factorial", "print(fact(10))", "3628800", fact)
    ok(g, "forward_call", "print(a())", "7", "def a(): Int = b()\ndef b(): Int = 7")
    ok(g, "mutual_recursion", "print(even(10))", "1",
       "def even(n: Int): Int =\n  if n == 0 then\n    return 1\n  else\n    return odd(n - 1)\n"
       "def odd(n: Int): Int =\n  if n == 0 then\n    return 0\n  else\n    return even(n - 1)")
    ok(g, "deep_recursion", "print(down(10000))", "0",
       "def down(n: Int): Int =\n  if n == 0 then\n    return 0\n  else\n    return down(n - 1)")
    ok(g, "param_order", "print(s(5, 3))", "2", "def s(a: Int, b: Int): Int = a - b")
    ok(g, "arg_int_to_double", "print(toStr(f(1)))", "1.00", "def f(x: Double): Double = x")
    ok(g, "return_int_as_double", "print(toStr(f()))", "2.00", "def f(): Double =\n  return 2")
    ok(g, "implicit_int_as_double", "print(toStr(f()))", "2.00", "def f(): Double =\n  2")
    ok(g, "one_line_int_as_double", "print(toStr(f()))", "2.00", "def f(): Double = 2")
    ok(g, "unit_param", "f(())", "()", "def f(u: Unit): Unit = print(u)")
    ok(g, "unit_discards_return", "print(f())", "()", "def f(): Unit =\n  return 42")
    ok(g, "unit_discards_last", "print(f())", "()", "def f(): Unit =\n  42")
    ok(g, "unit_evaluates_value", "print(f())", "x()", 'def f(): Unit =\n  return p()\ndef p(): Int =\n  print("x")\n  1')
    ok(g, "bare_return_unit", "f()\nprint(2)", "12", "def f(): Unit =\n  print(1)\n  return\n  print(9)")
    ok(g, "return_from_loop", "print(f())", "3",
       "def f(): Int =\n  var i = 0\n  while i < 10 do\n    if i == 3 then\n      return i\n    else\n      ()\n    i = i + 1\n  return 99")
    ok(g, "return_nested_if", "print(f(1))", "2",
       "def f(x: Int): Int =\n  if x > 0 then\n    if x > 5 then\n      return 1\n    else\n      return 2\n  else\n    return 3")
    ok(g, "implicit_last_expr", "print(f())", "7", "def f(): Int =\n  val a = 3\n  a + 4")
    ok(g, "implicit_last_call", "print(f())", "5", "def g(): Int = 5\ndef f(): Int =\n  print(\"\")\n  g()")
    ok(g, "middle_expr_discarded", "print(f())", "2", "def f(): Int =\n  1\n  2")
    ok(g, "infer_int", "print(f())", "42", "def f() = 42")
    ok(g, "infer_double", "print(toStr(f()))", "1.50", "def f() = 1.5")
    ok(g, "infer_string", "print(f())", "s", 'def f() = "s"')
    ok(g, "infer_block", "print(f())", "3", "def f() =\n  val a = 1\n  a + 2")
    ok(g, "infer_val_is_unit", "print(f())", "()", "def f() =\n  val a = 1")
    ok(g, "infer_assign_is_unit", "print(f())", "()", "def f() =\n  var a = 1\n  a = 2")
    ok(g, "infer_while_is_unit", "print(f())", "()", "def f() =\n  while 1 < 0 do\n    2")
    ok(g, "infer_if_units", "print(f())", "1()", "def f() =\n  if 1 < 2 then\n    print(1)\n  else\n    ()")
    ok(g, "if_all_return_int", "print(f())", "1", "def f(): Int =\n  if 1 < 2 then\n    return 1\n  else\n    return 2")
    ok(g, "explicit_unit_if_values", "print(f())", "()", "def f(): Unit =\n  if 1 < 2 then\n    1\n  else\n    2.5")
    ok(g, "same_name_var_function", "val f = 1\nprint(f + g())", "3", "def g(): Int = 2")
    ok(g, "param_named_as_function", "print(f(1))", "1", "def f(f: Int): Int = f")
    ok(g, "param_shadow_in_if", "print(f(1))", "21",
       "def f(a: Int): Int =\n  if 1 < 2 then\n    val a = 2\n    print(a)\n  else\n    ()\n  a")
    for name, helpers in [
        ("return_needs_annotation", "def f() =\n  return 1"), ("bare_return_inferred", "def f() =\n  return"),
        ("bare_return_int", "def f(): Int =\n  return"), ("return_wrong_type", 'def f(): Int =\n  return "a"'),
        ("return_double_as_int", "def f(): Int =\n  return 1.5"), ("return_relation", "def f(): Int =\n  return 1 < 2"),
        ("implicit_wrong_type", 'def f(): Int =\n  "a"'), ("one_line_wrong_type", 'def f(): Int = "a"'),
        ("ends_with_val", "def f(): Int =\n  val a = 1"), ("ends_with_assign", "def f(): Int =\n  var a = 1\n  a = 2"),
        ("ends_with_while", "def f(): Int =\n  while 1 < 0 do\n    return 1"),
        ("if_values_int", "def f(): Int =\n  if 1 < 2 then\n    1\n  else\n    2"),
        ("if_one_return", "def f(): Int =\n  if 1 < 2 then\n    return 1\n  else\n    2"),
        ("infer_if_values", "def f() =\n  if 1 < 2 then\n    1\n  else\n    2"),
        ("unreachable_wrong", "def f(): Int =\n  return 7\n  2.5"),
        ("unreachable_return_wrong", 'def f(): Int =\n  return 7\n  return "a"'),
        ("direct_recursion_inferred", "def f() = f()"), ("mutual_inferred", "def f() = g()\ndef g() = f()"),
        ("recursion_through_typed", "def f() = g()\ndef g(): Int = f()"),
        ("infer_relation", "def f() = 1 < 2"), ("unit_returns_relation", "def f(): Unit =\n  return 1 < 2"),
    ]:
        bad(g, name, "()", 6, helpers)
    for name, body, helpers in [
        ("too_few", "f()", "def f(a: Int): Unit = ()"), ("too_many", "f(1, 2)", "def f(a: Int): Unit = ()"),
        ("wrong_type", 'f("a")', "def f(a: Int): Unit = ()"), ("double_to_int", "f(1.5)", "def f(a: Int): Unit = ()"),
        ("unit_to_int", "f(())", "def f(a: Int): Unit = ()"), ("second_arg", 'f(1, "a")', "def f(a: Int, b: Int): Unit = ()"),
        ("wrong_expr_type", "f(1.5 + 1)", "def f(a: Int): Unit = ()"),
    ]:
        bad(g, name, body, 5, helpers)
    bad(g, "return_then_operator", "f()", 2, "def f(): Int =\n  return )")
    bad(g, "return_then_keyword", "f()", 2, "def f(): Int =\n  return val")
    bad(g, "one_line_body_keyword", "f()", 2, "def f(): Int = val")
    bad(g, "name_then_junk", "x y", (2, 3))
    bad(g, "undefined_function", "nope()", 3)
    bad(g, "undefined_in_arg", "print(nope())", 3)
    bad(g, "undefined_in_helper", "()", 3, "def f() = nope()")


def black_funexp():
    g = "bb_funexp"
    fact = "def factorial(n: Int): Int =\n  if n < 2 then\n    return 1\n  else\n    return n * factorial(n - 1)"
    ok(g, "spec_example", 'val result = 2 * factorial(readInt() + 1)\nprint(toStr(result + strLength(subStr("abcd", 1, 3))))',
       "50", fact, "3\n")
    tick = "def p(x: Int): Int =\n  print(x)\n  x\ndef add(a: Int, b: Int): Int = a + b"
    ok(g, "args_left_to_right", "print(add(p(1), p(2)))", "123", tick)
    ok(g, "operands_left_to_right", "print(p(1) - p(2))", "12-1", tick)
    ok(g, "nested_order", "print(add(p(1), add(p(2), p(3))))", "1236", tick)
    ok(g, "call_in_condition", "var i = 0\nwhile add(i, 1) < 4 do\n  i = i + 1\nprint(i)", "3", tick)
    ok(g, "call_in_if", 'if add(1, 1) == 2 then\n  print("T")\nelse\n  print("F")', "T", tick)
    ok(g, "call_in_return", "print(f())", "3", "def g(): Int = 2\ndef f(): Int =\n  return g() + 1")
    ok(g, "call_in_assign", "var x = 0\nx = add(1, 2) * 2\nprint(x)", "6", tick)
    ok(g, "stmt_expression", "add(1, 2) + 1\nprint(9)", "9", tick)
    ok(g, "print_of_print", "print(print(1))", "1()")
    ok(g, "unary_call", "print(-add(1, 2))", "-3", tick)
    ok(g, "call_in_parens", "print((add(1, 2)) * (add(2, 2)))", "12", tick)
    ok(g, "mixed_call_double", "print(toStr(add(1, 2) / 2.0))", "1.50", tick)
    ok(g, "arg_expr_coerced", "print(toStr(d(1 + 2)))", "3.00", "def d(x: Double): Double = x")
    ok(g, "str_call_concat", 'print(toStr(1) + "-" + toStr(2.5))', "1-2.50")
    ok(g, "no_args_in_expr", "print(g() + g())", "4", "def g(): Int = 2")
    bad(g, "bad_arg_expr", 'print(add(1 + "a", 2))', 6, tick)
    bad(g, "bad_count_nested", "print(add(add(1), 2))", 5, tick)
    bad(g, "unit_in_arith", "print(1 + print(2))", 6)
    bad(g, "unclosed_nested", "print(add(1, add(2, 3))", 2, tick)
    bad(g, "extra_close", "print(add(1, 2)))", 2, tick)


def black_builtins():
    g = "bb_builtins"
    for name, stdin, out in [
        ("plain", "42\n", "42"), ("negative", "-7\n", "-7"), ("no_newline", "5", "5"), ("max", "2147483647\n", "2147483647"),
        ("min", "-2147483648\n", "-2147483648"), ("over_max", "2147483648\n", "0"), ("under_min", "-2147483649\n", "0"),
        ("over_64bit", "99999999999999999999\n", "0"), ("leading_space", " 5\n", "0"), ("trailing_space", "5 \n", "0"),
        ("underscore", "1_0\n", "0"), ("word", "abc\n", "0"), ("empty_line", "\n", "0"), ("eof", "", "0"),
        ("hex", "0x10\n", "0"), ("decimal", "1.5\n", "0"), ("only_minus", "-\n", "0"),
    ]:
        ok(g, "readInt_" + name, "print(readInt())", out, stdin=stdin)
    for name, stdin, out in [
        ("plain", "1.5\n", "1.50"), ("exp", "1e2\n", "100.00"), ("int_form", "3\n", "3.00"), ("word", "abc\n", "0.00"),
        ("eof", "", "0.00"), ("nan", "nan\n", "0.00"), ("inf", "inf\n", "0.00"), ("neg_inf", "-inf\n", "0.00"),
        ("leading_space", " 1.5\n", "0.00"), ("trailing_space", "1.5 \n", "0.00"),
    ]:
        ok(g, "readDouble_" + name, "print(toStr(readDouble()))", out, stdin=stdin)
    ok(g, "readLine_keeps_newline", "print(readLine())", "ab\n", stdin="ab\ncd\n")
    ok(g, "readLine_twice", "print(readLine())\nprint(readLine())", "ab\ncd\n", stdin="ab\ncd\n")
    ok(g, "readLine_no_newline", "print(readLine())", "ab", stdin="ab")
    ok(g, "readLine_eof", 'print(strLength(readLine()))', "0")
    ok(g, "readLine_past_eof", 'print(readLine())\nprint(strLength(readLine()))', "a\n0", stdin="a\n")
    ok(g, "read_mix", "print(readInt() + 1)\nprint(readLine())\nprint(toStr(readDouble()))", "2x\n1.50", stdin="1\nx\n1.5\n")
    for name, expr, out in [
        ("toStr_int", "toStr(1)", "1"), ("toStr_neg", "toStr(-1.5)", "-1.50"), ("toStr_round", "toStr(1.005)", "1.00"),
        ("toStr_string", 'toStr("x")', "x"), ("toStr_unit", "toStr(())", "()"),
        ("toInt_trunc", "toInt(2.9)", "2"), ("toInt_neg", "toInt(-2.9)", "-2"), ("toInt_zero", "toInt(0.0)", "0"), ("toInt_from_int", "toInt(3)", "3"),
        ("strLength_empty", 'strLength("")', "0"), ("strLength_escape", r'strLength("a\tb")', "3"),
        ("subStr_all", 'subStr("abc", 0, 3)', "abc"), ("subStr_mid", 'subStr("abc", 1, 2)', "b"),
        ("subStr_empty", 'subStr("abc", 1, 1)', ""), ("subStr_start_zero_empty", 'subStr("abc", 0, 0)', ""),
        ("ord_first", 'ord("A", 0)', "65"), ("ord_last", 'ord("AB", 1)', "66"), ("ord_empty", 'ord("", 0)', "-1"),
        ("ord_past", 'ord("a", 1)', "-1"), ("ord_negative", 'ord("a", -1)', "-1"), ("ord_huge", 'ord("a", 2147483647)', "-1"),
        ("chr_A", "chr(65)", "A"), ("chr_max", "chr(127)", "\x7f"), ("chr_newline", "chr(10)", "\n"),
        ("chr_zero_length", "strLength(chr(0))", "1"), ("ord_chr", "ord(chr(90), 0)", "90"),
    ]:
        ok(g, name, f"print({expr})", out)
    for name, expr in [
        ("start_negative", 'subStr("abc", -1, 1)'), ("end_negative", 'subStr("abc", 0, -1)'),
        ("start_gt_end", 'subStr("abc", 2, 1)'), ("start_eq_len", 'subStr("abc", 3, 3)'),
        ("end_past", 'subStr("abc", 0, 4)'), ("empty_string", 'subStr("", 0, 0)'), ("huge_end", 'subStr("abc", 0, 2147483647)'),
    ]:
        run_err(g, "subStr_" + name, f"print({expr})", 40)
    for name, value in [("128", "128"), ("255", "255"), ("256", "256"), ("neg", "-1"), ("max", "2147483647")]:
        run_err(g, "chr_bad_" + name, f"print(chr({value}))", 58)
    for name, expr, rc in [
        ("print_none", "print()", 5), ("print_two", "print(1, 2)", 5), ("strLength_int", "strLength(1)", 5),
        ("toInt_str", 'toInt("1")', 5), ("chr_double", "chr(1.5)", 5), ("chr_str", 'chr("a")', 5),
        ("ord_swapped", 'ord(0, "a")', 5), ("subStr_double", 'subStr("a", 0.0, 1)', 5), ("readInt_arg", "readInt(1)", 5),
        ("readLine_arg", 'readLine("a")', 5), ("toStr_none", "toStr()", 5),
    ]:
        bad(g, name, expr, rc)


# ---------------------------------------------------------------- white box
def white_expression_stack():
    g = "wb_expr_stack"
    # capacity of the dynamic stack doubles from 64
    for depth in (1, 30, 31, 32, 33, 62, 63, 64, 65, 66, 126, 127, 128, 129, 130, 254, 255, 256, 257, 1023, 1024, 1025, 5000):
        ok(g, f"parens_{depth}", "print(" + "(" * depth + "7" + ")" * depth + ")", "7")
    for n in (2, 31, 32, 33, 63, 64, 65, 127, 128, 129, 1000):
        ok(g, f"sum_{n}", "print(" + " + ".join(["1"] * n) + ")", str(n))
    for depth in (31, 32, 63, 64, 65, 127, 128):
        ok(g, f"right_nested_{depth}", "print(" + "1 + (" * depth + "1" + ")" * depth + ")", str(depth + 1))
    for depth in (1, 2, 63, 64, 65, 200):
        ok(g, f"unary_{depth}", "print(" + "- " * depth + "5)", "-5" if depth % 2 else "5")
    ok(g, "deep_then_shallow", "print(" + "(" * 200 + "1" + ")" * 200 + ")\nprint(2)\nprint(" + "(" * 200 + "3" + ")" * 200 + ")", "123")
    bad(g, "deep_unclosed", "print(" + "(" * 300 + "1" + ")" * 299 + ")", 2)
    bad(g, "deep_extra_close", "print(" + "(" * 300 + "1" + ")" * 302, 2)
    bad(g, "deep_type_error", "print(" + "(" * 100 + '1 + "a"' + ")" * 100 + ")", 6)
    bad(g, "deep_undefined", "print(" + "(" * 100 + "nope" + ")" * 100 + ")", 3)
    bad(g, "error_holds_strings", 'val x = "a" + "b" + "c" * ("d" + 1)', 6)
    bad(g, "error_holds_identifiers", "val a = 1\nval x = a + a + a + ( a a )", 2)


def white_calls():
    g = "wb_calls"
    for n in (1, 2, 7, 8, 9, 15, 16, 17, 31, 32, 33, 63, 64, 65, 127, 128, 129, 256, 257):
        params = ", ".join(f"p{i}: Int" for i in range(n))
        ok(g, f"args_{n}", "print(f(" + ", ".join(map(str, range(n))) + "))", str(n - 1),
           f"def f({params}): Int = p{n - 1}")
    id_f = "def g(x: Int): Int = x"
    for depth in (2, 8, 16, 63, 64, 65, 128, 500, 2000):
        ok(g, f"nested_{depth}", "print(" + "g(" * depth + "1" + ")" * depth + ")", "1", id_f)
    math = "def add(a: Int, b: Int): Int = a + b\ndef mul(a: Int, b: Int): Int = a * b"
    ok(g, "reentrant_mix", "print(add(mul(2, 3), add(1, mul(4, 5))) * (add(1, 1) + 1))", "81", math)
    ok(g, "reentrant_paren_args", "print(add((1 + 2) * 3, ((4))))", "13", math)
    ok(g, "call_after_deep_arg", "print(add(" + "(" * 100 + "1" + ")" * 100 + ", 2) + 3)", "6", math)
    bad(g, "error_in_nested_type", 'print(add(mul(1, 2 + "a"), 3))', 6, math)
    bad(g, "error_in_nested_syntax", "print(add(mul(1, 2 +), 3))", 2, math)
    bad(g, "error_in_nested_undefined", "print(add(mul(1, nope), 3))", 3, math)
    bad(g, "error_after_first_arg", 'print(add("s" + "t", ))', (2, 5), math)
    bad(g, "eof_inside_call", "print(add(1,", 2, math)
    bad(g, "eof_after_name", "print(add", (2, 3), math)
    bad(g, "lexical_inside_call", "print(add(1, $))", 1, math)
    bad(g, "string_arg_unterminated", 'print(add(1, "a))', 1, math)
    ok(g, "many_string_args", 'print(c("a", "b", "c"))', "abc",
       "def c(x: String, y: String, z: String): String = x + y + z")


def white_inference():
    g = "wb_inference"
    for n in (2, 8, 64, 200):
        body = "".join(f"def f{i}() = f{i + 1}()\n" for i in range(n)) + f"def f{n}() = 7"
        ok(g, f"reverse_chain_{n}", "print(f0())", "7", body)
        body = f"def f{n}() = 7\n" + "".join(f"def f{i}() = f{i + 1}()\n" for i in range(n - 1, -1, -1))
        ok(g, f"forward_chain_{n}", "print(f0())", "7", body.strip())
    ok(g, "diamond", "print(a())", "3", "def a() = b() + c()\ndef b() = d()\ndef c() = d() + d()\ndef d() = 1 + 0")
    ok(g, "chain_to_double", "print(toStr(a()))", "1.50", "def a() = b()\ndef b() = c()\ndef c() = 1.5")
    ok(g, "chain_to_string", "print(a())", "s", 'def a() = b()\ndef b() = "s"')
    ok(g, "chain_to_unit", "print(a())", "x()", 'def a() = b()\ndef b() = print("x")')
    ok(g, "untyped_calls_typed_recursive", "print(a())", "1", "def a() = r(3)\ndef r(n: Int): Int =\n  if n < 2 then\n    return n\n  else\n    return r(n - 1)")
    ok(g, "typed_calls_untyped", "print(t())", "5", "def t(): Int = u()\ndef u() = 5")
    ok(g, "typed_cycle_ok", "print(a(2))", "0", "def a(n: Int): Int =\n  if n == 0 then\n    return 0\n  else\n    return b(n - 1)\ndef b(n: Int): Int = a(n)")
    ok(g, "builtin_not_in_graph", "print(a())", "3", 'def a() = strLength("abc")')
    ok(g, "local_named_like_function", "print(a())", "1", "def b() = 2\ndef a() =\n  val b = 1\n  b")
    bad(g, "cycle_3", "()", 6, "def a() = b()\ndef b() = c()\ndef c() = a()")
    bad(g, "cycle_via_typed_long", "()", 6, "def a() = b()\ndef b(): Int = c()\ndef c(): Int = a()")
    bad(g, "cycle_inside_arg", "()", 6, "def a(x: Int) = a(a(x))")
    bad(g, "inferred_used_badly", "val x: Int = a()", 6, "def a() = b()\ndef b() = 1.5")
    bad(g, "inferred_wrong_arg", "f(a())", 5, 'def a() = "s"\ndef f(x: Int): Unit = ()')
    bad(g, "inferred_unit_in_arith", "print(1 + a())", 6, "def a() =\n  val x = 1")
    bad(g, "untyped_undefined_callee", "()", 3, "def a() = b()\ndef b() = nope()")
    bad(g, "untyped_wrong_args", "()", 5, "def a() = b(1)\ndef b() = 1")
    bad(g, "error_in_late_untyped", "print(a())", 6, 'def a() = b()\ndef b() = 1 + "x"')
    bad(g, "syntax_error_in_typed_body", "()", 2, "def t(): Int =\n  val = 1")


def white_blocks():
    g = "wb_blocks"
    for depth in (2, 16, 64, 65, 128, 300):
        body = ""
        for i in range(depth):
            body += " " * i + "if 1 == 1 then\n"
        body += " " * depth + "print(1)\n"
        for i in range(depth - 1, -1, -1):
            body += " " * i + "else\n" + " " * (i + 1) + "()\n"
        ok(g, f"nested_if_{depth}", body.rstrip("\n"), "1")
    for depth in (2, 16, 64, 65, 200):
        body = ""
        for i in range(depth):
            body += " " * i + f"var v{i} = 0\n" + " " * i + f"while v{i} < 1 do\n"
            body += " " * (i + 1) + f"v{i} = v{i} + 1\n"
        body += " " * depth + "print(1)"
        ok(g, f"nested_while_{depth}", body, "1")
    ok(g, "defvar_in_loop", "var i = 0\nwhile i < 3 do\n  val a = i\n  var b = a + 1\n  print(b)\n  i = i + 1", "123")
    ok(g, "defvar_nested_loop", "var i = 0\nwhile i < 2 do\n  var j = 0\n  while j < 2 do\n    val k = i * 10 + j\n    print(k)\n    print(\" \")\n    j = j + 1\n  i = i + 1", "0 1 10 11 ")
    ok(g, "defvar_in_called_twice", "f()\nf()", "11", "def f(): Unit =\n  var i = 0\n  while i < 1 do\n    val a = 1\n    print(a)\n    i = i + 1")
    ok(g, "recursion_keeps_locals", "print(f(3))", "6", "def f(n: Int): Int =\n  val mine = n\n  if n == 0 then\n    return 0\n  else\n    val rest = f(n - 1)\n    return mine + rest")
    ok(g, "many_locals", "\n".join(f"val v{i} = {i}" for i in range(1500)) + "\nprint(v0 + v777 + v1499)", "2276")
    ok(g, "shadow_chain", "val x = 0\nif 1 < 2 then\n  val x = 1\n  if 1 < 2 then\n    val x = 2\n    print(x)\n  else\n    ()\n  print(x)\nelse\n  ()\nprint(x)", "210")
    ok(g, "implicit_only_in_body", "print(f())", "9", "def f(): Int =\n  if 1 < 2 then\n    5\n  else\n    6\n  9")
    ok(g, "loop_value_not_returned", "print(f())", "()", "def f() =\n  var i = 0\n  while i < 1 do\n    i = i + 1\n    42")
    ok(g, "stack_balanced_loop", "var i = 0\nwhile i < 20000 do\n  1 + 2\n  g()\n  i = i + 1\nprint(i)", "20000", "def g(): Int = 1")
    ok(g, "return_in_both_after_code", "print(f())", "2", "def f(): Int =\n  val a = 1\n  if a == 2 then\n    return 1\n  else\n    return 2")
    for name in ("else_0", "else_1", "if_skip_0", "if_skip_1", "while_start_0", "while_start_1", "while_end_0", "while_end_1", "readLine_end"):
        ok(g, "label_" + name, "if 1 < 2 then\n  print(" + name + "())\nelse\n  ()\nvar i = 0\nwhile i < 1 do\n  i = i + 1", "7", f"def {name}(): Int = 7")
    for name in ("read_int_ok", "read_int_invalid", "tmp_res", "m_end", "end", "main", "Main", "v1", "LF", "GF", "TF", "int", "string", "float", "nil", "bool", "LABEL", "MOVE"):
        ok(g, "name_" + name, f"print({name}())", "7", f"def {name}(): Int = 7")
    add(g, "header_once", program("print(a())", "def a() = b()\ndef b() = 1"), 0, "1")


def build_cases():
    for func in (black_lexer, black_layout, black_structure, black_variables, black_expressions,
                 black_functions, black_funexp, black_builtins, white_expression_stack, white_calls,
                 white_inference, white_blocks):
        func()


# ---------------------------------------------------------------- execution
def build(src, out_dir):
    sources = sorted(glob.glob(os.path.join(src, "*.c")))
    base = ["gcc", "-std=c2x", "-D_POSIX_C_SOURCE=200809L", f"-I{src}", "-w", "-g"]
    plain = os.path.join(out_dir, "ifj26")
    checked = os.path.join(out_dir, "ifj26_san")
    subprocess.run(base + ["-O1", "-o", plain] + sources, check=True)
    subprocess.run(base + ["-O1", "-fsanitize=address,undefined", "-fno-sanitize-recover=undefined",
                           "-fno-omit-frame-pointer", "-o", checked] + sources, check=True)
    return plain, checked


def run(cmd, data, timeout, env=None):
    try:
        p = subprocess.run(cmd, input=data, capture_output=True, timeout=timeout, env=env)
        return p.returncode, p.stdout, p.stderr
    except subprocess.TimeoutExpired:
        return "timeout", b"", b""


SAN_ENV = dict(os.environ, ASAN_OPTIONS="detect_leaks=1:exitcode=197:detect_stack_use_after_return=0",
               UBSAN_OPTIONS="halt_on_error=1:exitcode=198", LSAN_OPTIONS="exitcode=0")


def sanitizer_issue(rc, err):
    if rc == "timeout":
        return "compiler timeout"
    if rc in (197, 198) or (isinstance(rc, int) and rc < 0):
        return f"crash (rc={rc})"
    for mark in (b"AddressSanitizer", b"runtime error:"):
        if mark in err:
            return "sanitizer: " + mark.decode()
    if b"LeakSanitizer: detected" in err:
        return "memory leak"
    return ""


def execute(case, compiler, interpreter, work):
    rc, code, err = run([compiler], case["source"].encode("latin1"), 30, SAN_ENV)
    issue = sanitizer_issue(rc, err)
    if issue:
        return case["id"], issue
    if rc not in case["rc"]:
        return case["id"], f"compiler rc {rc}, expected {sorted(case['rc'])}"
    if rc != 0 or not interpreter:
        return case["id"], ""
    if code.count(b".IFJcode26") != 1:
        return case["id"], "header .IFJcode26 is not exactly once"
    path = os.path.join(work, f"p{abs(hash(case['id']))}.code")
    with open(path, "wb") as handle:
        handle.write(code)
    run_rc, out, _ = run([interpreter, path], case["stdin"].encode("latin1"), 30)
    os.unlink(path)
    if run_rc != case["run_rc"]:
        return case["id"], f"runtime rc {run_rc}, expected {case['run_rc']}"
    if case["out"] is not None and out != case["out"].encode("latin1"):
        return case["id"], f"stdout {out[:60]!r}, expected {case['out'][:60].encode('latin1')!r}"
    return case["id"], ""


# ---------------------------------------------------------------- robustness
SEEDS = [
    program('val a = 1 + 2 * 3\nvar b: Double = a\nb = b / 2\nprint(toStr(b))\nprint("x\\ty")'),
    program("var i = 0\nwhile i < 3 do\n  if i == 1 then\n    print(f(i))\n  else\n    ()\n  i = i + 1",
            "def f(n: Int): Int =\n  if n < 2 then\n    return 1\n  else\n    return n * f(n - 1)"),
    program('print(g(1, "a"))\nprint(h())', 'def g(a: Int, b: String) = b + toStr(a)\ndef h() = strLength(subStr("abcd", 1, 3)) + ord("A", 0)'),
    HEAD + "// c\n/* a /* b */ */\ndef a(x: Double,\n  y: Int,\n): Double = x * y\n@main\ndef m() =\n  val s = \"\"\"r\\aw\"\"\"\n  print(s); print(a(1, 2))\n",
]
TOKENS = ["(", ")", ",", ":", "=", "==", "<", "+", "-", "*", "/", ";", ".", "\n", "\n  ", "\n    ", " ", "\t", '"', '"""',
          "/*", "*/", "//", "@main", "def ", "val ", "var ", "if ", "then", "else", "while ", "do", "return ",
          "Int", "Double", "String", "Unit", "()", "0", "1", "2147483648", "1e999", "1.5", "x", "f", "print", "\\", "\0", "\x7f", "\xff"]


def mutate(rng, text):
    data = list(text)
    for _ in range(rng.randint(1, 4)):
        pos = rng.randrange(len(data) + 1)
        kind = rng.randrange(6)
        if kind == 0 and data:
            del data[min(pos, len(data) - 1)]
        elif kind == 1:
            data[pos:pos] = list(rng.choice(TOKENS))
        elif kind == 2 and data:
            data[min(pos, len(data) - 1)] = chr(rng.randrange(256))
        elif kind == 3 and data:
            end = min(len(data), pos + rng.randint(1, 30))
            del data[pos:end]
        elif kind == 4 and data:
            end = min(len(data), pos + rng.randint(1, 30))
            data[pos:pos] = data[pos:end]
        elif data:
            other = rng.randrange(len(data))
            a, b = min(pos, len(data) - 1), other
            data[a], data[b] = data[b], data[a]
    return "".join(data)


def fuzz_one(index, seed, compiler, interpreter, work):
    rng = random.Random(seed * 1000003 + index)
    source = mutate(rng, rng.choice(SEEDS))
    data = source.encode("latin1")
    rc, code, err = run([compiler], data, 20, SAN_ENV)
    issue = sanitizer_issue(rc, err)
    if not issue and rc not in ALLOWED_COMPILER:
        issue = f"unexpected compiler rc {rc}"
    if not issue and rc == 0 and interpreter:
        path = os.path.join(work, f"m{index}.code")
        with open(path, "wb") as handle:
            handle.write(code)
        run_rc, _, _ = run([interpreter, path], b"1\n", 2)
        os.unlink(path)
        if run_rc != "timeout" and run_rc not in ALLOWED_RUNTIME:
            issue = f"generated code ended with {run_rc}"
    return issue, source, rc


def stress(compiler):
    def nested_if(n):
        lines = "".join(" " * i + "if 1 == 1 then\n" for i in range(n)) + " " * n + "print(1)\n"
        return lines + "".join(" " * i + "else\n" + " " * (i + 1) + "()\n" for i in range(n - 1, -1, -1))

    results = []
    for name, source in [
        ("parens 100 000", program("print(" + "(" * 100000 + "1" + ")" * 100000 + ")")),
        ("soucet 100 000 clenu", program("print(" + "+".join(["1"] * 100000) + ")")),
        ("soucet 1 000 000 clenu", program("print(" + "+".join(["1"] * 1000000) + ")")),
        ("vnorena volani 10 000", program("print(" + "g(" * 10000 + "1" + ")" * 10000 + ")", "def g(x: Int): Int = x")),
        ("vnorena volani 50 000", program("print(" + "g(" * 50000 + "1" + ")" * 50000 + ")", "def g(x: Int): Int = x")),
        ("vnorene if 3 000", program(nested_if(3000).rstrip("\n"))),
        ("funkce 20 000", HEAD + "".join(f"def f{i}(): Int = 1\n" for i in range(20000)) + "@main def m(): Unit = ()\n"),
        ("parametry 10 000", program("f(" + ",".join("1" for _ in range(10000)) + ")",
                                     "def f(" + ",".join(f"p{i}: Int" for i in range(10000)) + "): Unit = ()")),
        ("retezec 5 MB", program('print("' + "x" * 5000000 + '")')),
        ("bez anotace, opacne 1 000", HEAD + "".join(f"def f{i}() = f{i + 1}()\n" for i in range(1000)) + "def f1000() = 1\n@main def m(): Unit = print(f0())\n"),
    ]:
        start = time.time()
        rc, _, _ = run([compiler], source.encode("latin1"), 60)
        results.append((name, rc, time.time() - start))
    return results


def main():
    parser = argparse.ArgumentParser()
    root = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    parser.add_argument("--src", default=os.path.join(root, "src"))
    parser.add_argument("--interpreter")
    parser.add_argument("--filter", default="")
    parser.add_argument("--mutations", type=int, default=3000)
    parser.add_argument("--seed", type=int, default=26)
    parser.add_argument("--no-stress", action="store_true")
    parser.add_argument("--json")
    parser.add_argument("-v", "--verbose", action="store_true")
    args = parser.parse_args()

    build_cases()
    ids = collections.Counter(c["id"] for c in CASES)
    duplicated = [name for name, count in ids.items() if count > 1]
    if duplicated:
        sys.exit("duplicate test names: " + ", ".join(duplicated))
    chosen = [c for c in CASES if args.filter in c["id"]]
    interpreter = os.path.abspath(args.interpreter) if args.interpreter else None

    with tempfile.TemporaryDirectory() as work:
        plain, checked = build(args.src, work)
        with concurrent.futures.ThreadPoolExecutor(os.cpu_count() or 4) as pool:
            results = list(pool.map(lambda c: execute(c, checked, interpreter, work), chosen))
            failures = [(name, why) for name, why in results if why]
            fuzz = list(pool.map(lambda i: fuzz_one(i, args.seed, checked, interpreter, work), range(args.mutations)))
        limits = [] if args.no_stress else stress(plain)

    totals = collections.Counter(c["group"] for c in chosen)
    failed = collections.Counter(name.split("/")[0] for name, _ in failures)
    print(f"{'group':18}{'pass':>6}{'total':>7}")
    for group in totals:
        print(f"{group:18}{totals[group] - failed[group]:>6}{totals[group]:>7}")
    print(f"\nCELKEM: {len(chosen) - len(failures)} / {len(chosen)}")
    for name, why in failures:
        print(f"  [NE] {name}: {why}")

    fuzz_issues = [(issue, source, rc) for issue, source, rc in fuzz if issue]
    codes = collections.Counter(str(rc) for _, _, rc in fuzz)
    print(f"\nMUTACE: {args.mutations - len(fuzz_issues)} / {args.mutations} bez padu; kody prekladace: {dict(sorted(codes.items()))}")
    for issue, count in collections.Counter(i for i, _, _ in fuzz_issues).most_common():
        print(f"  [NE] {issue}: {count}x")
    if args.verbose:
        for issue, source, _ in fuzz_issues[:5]:
            print(f"--- {issue}\n{source!r}")

    if limits:
        print("\nLIMITY (informativni, bez sanitizeru):")
        for name, rc, seconds in limits:
            state = "ok" if rc == 0 else f"rc={rc}"
            print(f"  {name:30}{state:12}{seconds:7.2f} s")

    if args.json:
        with open(args.json, "w", encoding="utf-8") as handle:
            json.dump({"total": len(chosen), "failures": failures,
                       "mutations": args.mutations, "mutation_issues": [(i, s) for i, s, _ in fuzz_issues],
                       "limits": limits}, handle, indent=1)
    return 1 if failures or fuzz_issues else 0


if __name__ == "__main__":
    sys.exit(main())
