# Project:    IFJ26
# Filename:   cases.py
#
# AI: file written with AI (Claude Code, Claude Opus 5.5)
# prompt: "vytvor testovaci skripty pro nas kod a znovu analyzuj, co kod
#          rozbiji; drz se zakladni verze, ruzne vstupy, zakladni i krajni
#          hodnoty, porovnavej ocekavani s vystupy"
#
# Every expectation comes from the IFJ26 specification. A tuple in `rc` means
# the specification does not settle the code and any of the values is fine.
import random
import textwrap
from dataclasses import dataclass

from ic26sim import c_hex_float as hx

HEAD = "import ifj26.*\n"


@dataclass(frozen=True)
class Case:
    name: str
    group: str
    source: str
    rc: object = 0           # expected compiler code (int or tuple)
    out: bytes = b""         # expected stdout of the compiled program
    stdin: bytes = b""
    run_rc: int = 0          # expected return code of the compiled program


CASES = []


def main_of(body, helpers=""):
    """Wraps statements into a valid program with @main."""
    body = textwrap.indent(textwrap.dedent(body).strip("\n"), "  ")
    helpers = textwrap.dedent(helpers).strip("\n")
    helpers = helpers + "\n" if helpers else ""
    return HEAD + helpers + "@main\ndef main(): Unit =\n" + body + "\n"


def to_bytes(text):
    return text.encode("latin-1") if isinstance(text, str) else text


def add(group, name, source, rc=0, out="", stdin="", run_rc=0):
    CASES.append(Case(f"{group}.{name}", group, source, rc, to_bytes(out),
                      to_bytes(stdin), run_rc))


def ok(group, name, body, out="", helpers="", stdin=""):
    add(group, name, main_of(body, helpers), 0, out, stdin)


def err(group, name, rc, body, helpers=""):
    add(group, name, main_of(body, helpers), rc)


def run_err(group, name, run_rc, body, out="", helpers="", stdin=""):
    add(group, name, main_of(body, helpers), 0, out, stdin, run_rc)


# ---------------------------------------------------------------- lexer
G = "01_lexer"
for name, literal, value in [
        ("int_zero", "0", "0"), ("int_plain", "42", "42"),
        ("int_sep", "1_000_000", "1000000"), ("int_double_sep", "1__000", "1000"),
        ("int_max", "2147483647", "2147483647")]:
    ok(G, name, f"val x = {literal}\nprint(x)", value)
for name, literal, value in [
        ("dbl_plain", "1.5", 1.5), ("dbl_no_int", ".5", .5), ("dbl_exp", "1e3", 1e3),
        ("dbl_EXP", "1E3", 1e3), ("dbl_exp_plus", "1e+3", 1e3),
        ("dbl_exp_minus", "1e-1", 1e-1), ("dbl_frac_exp", "1.5e2", 150.0),
        ("dbl_suffix_d", "2.5d", 2.5), ("dbl_suffix_D", "2.5D", 2.5),
        ("dbl_exp_suffix", "1e2d", 100.0), ("dbl_sep", "1_0.5_0", 10.5),
        ("dbl_dot_exp", ".5e1", 5.0), ("dbl_exp_zeros", "2e001", 20.0),
        ("dbl_zero", "0.0", 0.0), ("dbl_tiny", "1e-300", 1e-300),
        ("dbl_big", "1e300", 1e300)]:
    ok(G, name, f"val x = {literal}\nprint(x)", hx(value))
ok(G, "str_empty", 'val s = ""\nprint(s)', "")
ok(G, "str_escapes", r'print("A\tB\n\"C\"\\")', 'A\tB\n"C"\\')
ok(G, "str_cr", r'print("a\rb")', "a\rb")
ok(G, "str_hash_space", 'print("a # b  c")', "a # b  c")
ok(G, "str_comment_like", 'print("// /* x */")', "// /* x */")
ok(G, "str_all_special", r'print("!$%&()*+,-./:;<=>?@[]^_`{|}~")',
   "!$%&()*+,-./:;<=>?@[]^_`{|}~")
add(G, "ml_string", HEAD + '@main\ndef main(): Unit =\n  val x = """A\n hoj s\\nvete\n """\n'
    '  print(x)\n', 0, "A\n hoj s\\nvete\n ")
add(G, "ml_quote_inside", HEAD + '@main\ndef main(): Unit =\n  val x = """a"b""c"""\n'
    '  print(x)\n', 0, 'a"b""c')
add(G, "ml_empty", HEAD + '@main\ndef main(): Unit =\n  val x = """"""\n  print(x)\n', 0, "")
add(G, "ml_seven_quotes", HEAD + '@main\ndef main(): Unit =\n  val x = """""""\n'
    '  print(x)\n', 0, '"')
ok(G, "comment_between", "val x = 1 /* c */ + 2\nprint(x)", "3")
ok(G, "comment_nested", "val x = 1 /* a /* b */ c */ + 2\nprint(x)", "3")
ok(G, "comment_line_end", "val x = 1 // c\nprint(x) // d", "1")
add(G, "comment_before_prolog", "// a\n/* b */\n\n" + HEAD
    + "@main\ndef main(): Unit =\n  print(1)\n", 0, "1")
add(G, "comment_multiline_is_eol", HEAD + "@main\ndef main(): Unit =\n"
    "  val x = 1 /* a\n  b */\n  print(x)\n", 0, "1")
add(G, "comment_multiline_then_token", HEAD + "@main\ndef main(): Unit =\n"
    "  val x = 1 /* a\n  b */ + 2\n  print(x)\n", (1, 2))
add(G, "blank_lines_in_block", HEAD + "@main\ndef main(): Unit =\n  print(1)\n\n   \n"
    "      // c\n  print(2)\n", 0, "12")
add(G, "prolog_spaces", "import   ifj26 . *\n@main\ndef main(): Unit =\n  print(1)\n", 0, "1")
add(G, "indent_one_space", HEAD + "@main\ndef main(): Unit =\n print(1)\n", 0, "1")
add(G, "indent_eight", HEAD + "@main\ndef main(): Unit =\n        print(1)\n", 0, "1")
add(G, "indent_mixed_widths", HEAD + "@main\ndef main(): Unit =\n  if 1 < 2 then\n"
    "       print(1)\n  else\n     print(2)\n", 0, "1")
add(G, "no_final_newline_block", HEAD + "@main\ndef main(): Unit =\n  print(1)", 0, "1")
add(G, "no_final_newline_one_line", HEAD + "@main\ndef main(): Unit = print(1)", 0, "1")
add(G, "crlf_free_eof_comment", HEAD + "@main\ndef main(): Unit =\n  print(1)\n// end", 0, "1")
ok(G, "paren_newline", "val x = (1 +\n    2)\nprint(x)", "3")
ok(G, "multiline_args", 'val p = subStr(\n    "abcd",\n    1,\n    3,\n  )\nprint(p)', "bc")
for name, body in [
        ("unknown_char", "val x = 1 $ 2"), ("question", "val x = ?"),
        ("bad_escape", r'print("\q")'), ("bad_hex_escape", r'print("\x41")'),
        ("unterminated", 'val x = "abc'), ("newline_in_str", 'val x = "a\nb"'),
        ("leading_zero", "val x = 012"), ("zero_sep", "val x = 0_1"),
        ("trailing_sep", "val x = 1_"), ("exp_no_digit", "val x = 1e"),
        ("exp_sign_no_digit", "val x = 1e+"), ("f_suffix", "val x = 1.5f"),
        ("long_suffix", "val x = 1L"), ("hex", "val x = 0x10"),
        ("dbl_overflow", "val x = 1e400"), ("sep_before_exp", "val x = 1_e2"),
        ("d_then_digit", "val x = 1.0d2")]:
    err(G, "e_" + name, 1, body)
add(G, "e_tab_indent", HEAD + "@main\ndef main(): Unit =\n\tprint(1)\n", 1)
add(G, "e_unterminated_comment", HEAD + "/* abc\n@main\ndef main(): Unit =\n  print(1)\n", 1)
add(G, "e_unterminated_nested", HEAD + "/* a /* b */\n@main\ndef main(): Unit =\n  print(1)\n", 1)
add(G, "e_unterminated_ml", HEAD + '@main\ndef main(): Unit =\n  val x = """abc\n', 1)
add(G, "e_at_other", HEAD + "@foo\ndef main(): Unit =\n  print(1)\n", (1, 2))
add(G, "e_at_space", HEAD + "@ main\ndef main(): Unit =\n  print(1)\n", (1, 2))

# ---------------------------------------------------------------- syntax
G = "02_syntax"
for name, src in [
        ("no_prolog", "@main\ndef main(): Unit =\n  print(1)\n"),
        ("bad_prolog_name", "import ifj25.*\n@main\ndef main(): Unit =\n  print(1)\n"),
        ("prolog_no_star", "import ifj26\n@main\ndef main(): Unit =\n  print(1)\n"),
        ("double_prolog", HEAD + HEAD + "@main\ndef main(): Unit =\n  print(1)\n"),
        ("prolog_after_def", "def f(): Unit = print(1)\n" + HEAD
         + "@main\ndef main(): Unit =\n  f()\n"),
        ("empty_input", ""),
        ("empty_body", HEAD + "@main\ndef main(): Unit =\n"),
        ("top_level_stmt", HEAD + "val x = 1\n@main\ndef main(): Unit =\n  print(1)\n"),
        ("nested_def", HEAD + "@main\ndef main(): Unit =\n  def f(): Int = 1\n  print(1)\n"),
        ("def_no_assign", HEAD + "@main\ndef main(): Unit\n  print(1)\n"),
        ("def_no_paren", HEAD + "@main\ndef main: Unit =\n  print(1)\n"),
        ("param_no_type", HEAD + "def f(a) = a\n@main\ndef main(): Unit =\n  print(1)\n"),
        ("param_bad_type", HEAD + "def f(a: 1) = a\n@main\ndef main(): Unit =\n  print(1)\n"),
        ("param_same_line_comma", HEAD + "def f(a: Int,): Int = a\n@main\ndef main(): Unit =\n"
         "  print(1)\n"),
        ("body_not_indented", HEAD + "@main\ndef main(): Unit =\nprint(1)\n")]:
    add(G, name, src, 2)
for name, body in [
        ("if_no_then", "if 1 < 2\n  print(1)\nelse\n  print(2)"),
        ("if_no_else", "if 1 < 2 then\n  print(1)"),
        ("if_inline_body", "if 1 < 2 then print(1)\nelse\n  print(2)"),
        ("if_empty_branch", "if 1 < 2 then\nelse\n  print(2)"),
        ("else_alone", "else\n  print(1)"),
        ("while_no_do", "var i = 0\nwhile i < 3\n  i = i + 1"),
        ("while_inline_body", "var i = 0\nwhile i < 3 do i = i + 1"),
        ("rel_chain", "if 1 < 2 < 3 then\n  print(1)\nelse\n  print(2)"),
        ("rel_chain2", "if 1 >= 2 <= 3 then\n  print(1)\nelse\n  print(2)"),
        ("eq_chain", "if 1 == 2 == 3 then\n  print(1)\nelse\n  print(2)"),
        ("eq_chain2", "if 1 != 2 == 3 then\n  print(1)\nelse\n  print(2)"),
        ("unfinished_expr", "val x = 1 +"),
        ("unfinished_expr_newline", "val x = 1 +\nprint(x)"),
        ("line_starts_operator", "val x = 1\n* 2"),
        ("missing_operand", "val x = * 2"),
        ("two_terms", "val x = 1 2"),
        ("two_ids", "val a = 1\nval x = a a"),
        ("unit_unit", "val x = ()()"),
        ("value_then_paren", "val x = 1 (2)"),
        ("empty_parens_binop", "val x = ( * 1)"),
        ("unbalanced_close", "val x = 1)"),
        ("var_no_init", "var x"),
        ("val_no_name", "val = 1"),
        ("val_no_assign", "val x 1"),
        ("double_assign", "var x = 1\nx = = 2"),
        ("assign_to_literal", "1 = 2"),
        ("keyword_ident", "val if = 1"),
        ("keyword_type_ident", "val Int = 1"),
        ("call_missing_paren", "print(1"),
        ("call_same_line_comma", "print(1,)"),
        ("call_leading_comma", "print(,1)"),
        ("call_double_comma", "subStr(\"a\",,1,1)"),
        ("call_missing_comma", 'subStr("abc" 0 1)'),
        ("stmt_semicolon_only", ";"),
        ("extra_indent", "val x = 1\n    val y = 2")]:
    err(G, name, 2, body)
err(G, "dedent_bad_level", (1, 2), "if 1 < 2 then\n    print(1)\n  else\n    print(2)")
err(G, "else_misaligned", (1, 2), "if 1 < 2 then\n  print(1)\n else\n  print(2)")

# ---------------------------------------------------------------- errors 3
G = "03_undefined"
for name, body, helpers in [
        ("var_in_print", "print(x)", ""),
        ("var_in_expr", "val y = x + 1", ""),
        ("var_in_cond", "if x < 1 then\n  print(1)\nelse\n  print(2)", ""),
        ("var_in_while", "while x < 1 do\n  print(1)", ""),
        ("var_stmt", "x", ""),
        ("var_stmt_expr", "x + 1", ""),
        ("assign_rhs", "var a = 1\na = b", ""),
        ("self_init", "val a = a", ""),
        ("func", "foo()", ""),
        ("func_init", "val a = foo()", ""),
        ("func_assign", "var a = 1\na = foo()", ""),
        ("func_arg", "print(nope)", ""),
        ("use_before_def", "print(y)\nval y = 1", ""),
        ("out_of_if", "if 1 < 2 then\n  val t = 1\n  print(t)\nelse\n  print(0)\nprint(t)", ""),
        ("out_of_else", "if 1 < 2 then\n  print(0)\nelse\n  val t = 1\n  print(t)\nprint(t)", ""),
        ("then_var_in_else", "if 1 < 2 then\n  val t = 1\n  print(t)\nelse\n  print(t)", ""),
        ("out_of_while", "var i = 0\nwhile i < 1 do\n  val t = i\n  i = i + 1\nprint(t)", ""),
        ("func_as_value", "val x = f", "def f(): Int = 1"),
        ("other_func_local", "print(a)", "def f(a: Int): Int = a"),
        ("local_of_other_func", "print(loc)", "def f(): Unit =\n  val loc = 1\n  print(loc)"),
        ("case_sensitive", "val Abc = 1\nprint(abc)", "")]:
    err(G, name, 3, body, helpers)
err(G, "assign", (3, 7), "z = 1")  # section 2.2 says 7, section 6 says 3
add(G, "no_main", HEAD + "def f(): Unit =\n  print(1)\n", 3)
add(G, "only_prolog", HEAD, 3)
add(G, "undefined_in_function", HEAD + "def f(): Int = x\n@main\ndef main(): Unit =\n  print(1)\n", 3)

# ---------------------------------------------------------------- errors 4
G = "04_redefinition"
err(G, "var", 4, "val x = 1\nval x = 2")
err(G, "var_val_mix", 4, "var x = 1\nval x = 2")
err(G, "var_other_type", 4, 'val x = 1\nval x = "a"')
err(G, "in_if_branch", 4, "if 1 < 2 then\n  val a = 1\n  val a = 2\nelse\n  print(1)")
err(G, "in_while", 4, "var i = 0\nwhile i < 1 do\n  val a = 1\n  val a = 2\n  i = 1")
err(G, "func", 4, "print(1)", "def f(): Unit = print(1)\ndef f(): Unit = print(2)")
err(G, "func_overload", 4, "print(1)", "def f(a: Int): Int = a\ndef f(a: String): String = a")
err(G, "dup_param", 4, "print(1)", "def f(a: Int, a: Int): Int = a")
err(G, "dup_param_types", 4, "print(1)", "def f(a: Int, a: String): Int = 1")
add(G, "two_mains", HEAD + "@main\ndef a(): Unit =\n  print(1)\n@main\ndef b(): Unit =\n  print(2)\n", 4)
err(G, "main_dup_name", 4, "print(1)", "def main(): Unit = print(2)")

# ---------------------------------------------------------------- errors 5
G = "05_arguments"
H_F = "def f(a: Int): Int =\n  return a"
err(G, "too_many", 5, "val x = f(1, 2)", H_F)
err(G, "too_few", 5, "val x = f()", H_F)
err(G, "stmt_too_many", 5, "f(1, 2)", H_F)
err(G, "stmt_too_few", 5, "f()", H_F)
err(G, "wrong_type_str", 5, 'val x = f("a")', H_F)
err(G, "wrong_type_dbl", 5, "val x = f(1.5)", H_F)
err(G, "wrong_type_unit", 5, "val x = f(())", H_F)
err(G, "wrong_type_var", 5, 'val s = "a"\nval x = f(s)', H_F)
err(G, "second_arg", 5, 'val x = g(1, 2)', "def g(a: Int, b: String): Int =\n  return a")
for name, body in [
        ("print_none", "print()"), ("print_two", "print(1, 2)"),
        ("readLine_arg", "val x = readLine(1)"), ("readInt_arg", "val x = readInt(1)"),
        ("readDouble_arg", "val x = readDouble(1)"), ("toStr_none", "val x = toStr()"),
        ("toInt_str", 'val x = toInt("a")'), ("strLength_int", "val x = strLength(1)"),
        ("subStr_few", 'val x = subStr("a", 0)'),
        ("subStr_type", 'val x = subStr("a", "b", 1)'),
        ("subStr_dbl", 'val x = subStr("a", 0.5, 1)'), ("ord_few", 'val x = ord("a")'),
        ("ord_type", "val x = ord(1, 0)"), ("chr_dbl", "val x = chr(1.5)"),
        ("chr_str", 'val x = chr("a")')]:
    err(G, name, 5, body)
add(G, "main_with_param", HEAD + "@main\ndef main(a: Int): Unit =\n  print(a)\n", 5)

# ---------------------------------------------------------------- errors 6
G = "06_types"
for name, body in [
        ("decl_dbl_to_int", "val x: Int = 1.5"),
        ("decl_str_to_int", 'val x: Int = "a"'),
        ("decl_int_to_str", 'val x: String = 1'),
        ("decl_unit_to_int", "val x: Int = ()"),
        ("decl_call_unit", "val x: Int = print(1)"),
        ("decl_call_str", "val x: Int = readLine()"),
        ("decl_call_dbl", "val x: Int = readDouble()"),
        ("decl_expr_dbl", "val x: Int = 1 / 2.0"),
        ("assign_str", 'var x = 1\nx = "a"'),
        ("assign_dbl_to_int", "var x = 1\nx = 1.5"),
        ("assign_unit", "var x = 1\nx = ()"),
        ("assign_call_str", "var x = 1\nx = readLine()"),
        ("store_bool", "val b = 1 < 2"),
        ("store_bool_typed", "val b: Int = 1 < 2"),
        ("assign_bool", "var x = 1\nx = 1 < 2"),
        ("cond_int", "if 1 then\n  print(1)\nelse\n  print(2)"),
        ("cond_str", 'if "a" then\n  print(1)\nelse\n  print(2)'),
        ("cond_arith", "if 1 + 1 then\n  print(1)\nelse\n  print(2)"),
        ("cond_var", "val a = 1\nif a then\n  print(1)\nelse\n  print(2)"),
        ("cond_unit", "if () then\n  print(1)\nelse\n  print(2)"),
        ("while_cond_int", "var i = 1\nwhile i do\n  i = 0"),
        ("concat_int", 'val x = "a" + 1'),
        ("concat_int_left", 'val x = 1 + "a"'),
        ("str_minus", 'val x = "a" - "b"'),
        ("str_mul", 'val x = "a" * 2'),
        ("str_div", 'val x = "a" / "b"'),
        ("neg_str", 'val x = -"a"'),
        ("neg_unit", "val x = -()"),
        ("unit_add", "val x = () + 1"),
        ("unit_add_unit", "val x = () + ()"),
        ("arith_on_bool", "if (1 < 2) + 1 then\n  print(1)\nelse\n  print(2)"),
        ("eq_str_int", 'if "a" == 1 then\n  print(1)\nelse\n  print(2)'),
        ("neq_str_dbl", 'if "a" != 1.5 then\n  print(1)\nelse\n  print(2)'),
        ("eq_unit_int", "if () == 1 then\n  print(1)\nelse\n  print(2)"),
        ("lt_str_int", 'if "a" < 1 then\n  print(1)\nelse\n  print(2)'),
        ("lt_unit", "if () < () then\n  print(1)\nelse\n  print(2)"),
        ("rel_then_eq", "if 1 < 2 == 3 then\n  print(1)\nelse\n  print(2)"),
        ("eq_two_rel", "if 1 < 2 == 3 < 4 then\n  print(1)\nelse\n  print(2)")]:
    err(G, name, 6, body)
add(G, "main_returns_int", HEAD + "@main\ndef main(): Int =\n  return 1\n", 6)
add(G, "main_returns_str", HEAD + '@main\ndef main(): String = "a"\n', 6)
err(G, "pass_bool_builtin", (2, 6), "print(1 < 2)")
err(G, "pass_bool_user", (2, 5, 6), "f(1 < 2)", "def f(a: Int): Unit = print(a)")

# ---------------------------------------------------------------- errors 7
G = "07_immutable"
err(G, "val", 7, "val x = 1\nx = 2")
err(G, "val_same_value", 7, "val x = 1\nx = 1")
err(G, "val_from_nested", 7, "val x = 1\nif 1 < 2 then\n  x = 2\nelse\n  print(1)")
err(G, "val_from_while", 7, "val x = 1\nwhile x < 1 do\n  x = 2")
err(G, "val_call", 7, "val x = 1\nx = readInt()")
err(G, "param", 7, "f(1)", "def f(a: Int): Unit =\n  a = 2")
err(G, "param_nested", 7, "f(1)",
    "def f(a: Int): Unit =\n  if a < 1 then\n    a = 2\n  else\n    print(a)")
err(G, "shadow_val_over_var", 7, "var x = 1\nif 1 < 2 then\n  val x = 2\n  x = 3\nelse\n  print(1)")
ok(G, "shadow_var_over_val_ok", "val x = 1\nif 1 < 2 then\n  var x = 2\n  x = 3\n  print(x)\n"
   "else\n  print(0)\nprint(x)", "31")

# ---------------------------------------------------------------- expressions
G = "10_expressions"
for name, expr, value in [
        ("prec_mul", "2 + 3 * 4", "14"), ("prec_paren", "(2 + 3) * 4", "20"),
        ("assoc_sub", "10 - 3 - 2", "5"), ("assoc_sub_paren", "10 - (3 - 2)", "9"),
        ("assoc_div", "100 / 10 / 2", "5"), ("assoc_div_paren", "100 / (10 / 2)", "20"),
        ("assoc_mix", "2 * 3 / 4", "1"), ("assoc_mix2", "2 / 3 * 4", "0"),
        ("unary", "-5", "-5"), ("unary_expr", "-(2 + 3)", "-5"),
        ("unary_mul", "-2 * 3", "-6"), ("mul_unary", "2 * -3", "-6"),
        ("sub_unary", "2 - -3", "5"), ("double_neg", "- -3", "3"),
        ("triple_neg", "- - -3", "-3"), ("unary_paren_neg", "-(-(-1))", "-1"),
        ("idiv_pos", "7 / 2", "3"), ("idiv_neg_left", "-7 / 2", "-3"),
        ("idiv_neg_right", "7 / -2", "-3"), ("idiv_both_neg", "-7 / -2", "3"),
        ("idiv_exact", "6 / 3", "2"), ("zero_div_n", "0 / 5", "0"),
        ("deep_paren", "((((((1 + 2))))))", "3"),
        ("long_chain", "1 + 2 + 3 + 4 + 5 + 6 + 7 + 8 + 9 + 10", "55"),
        ("mixed_chain", "1 + 2 * 3 - 4 / 2 + 5 * (6 - 1)", "30")]:
    ok(G, name, f"val x = {expr}\nprint(x)", value)
for name, expr, value in [
        ("dbl_add", "1.5 + 2.25", 3.75), ("dbl_sub", "1.5 - 2.0", -0.5),
        ("dbl_mul", "1.5 * 2.0", 3.0), ("dbl_div", "7.0 / 2.0", 3.5),
        ("mix_add", "1 + 0.5", 1.5), ("mix_add_r", "0.5 + 1", 1.5),
        ("mix_mul", "2 * 1.5", 3.0), ("mix_div_l", "7 / 2.0", 3.5),
        ("mix_div_r", "7.0 / 2", 3.5), ("mix_sub", "1 - 0.25", 0.75),
        ("mix_nested", "1 + 2 * 1.5", 4.0), ("int_sub_then_dbl", "7 / 2 + 0.5", 3.5),
        ("neg_dbl", "-1.5", -1.5), ("neg_dbl_expr", "-(1.5 + 1)", -2.5)]:
    ok(G, name, f"val x = {expr}\nprint(x)", hx(value))
ok(G, "concat", 'val s = "a" + "b"\nprint(s)', "ab")
ok(G, "concat_three", 'val s = "a" + "b" + "c"\nprint(s)', "abc")
ok(G, "concat_paren", 'val s = "a" + ("b" + "c")\nprint(s)', "abc")
ok(G, "concat_empty", 'val s = "" + "" + "x"\nprint(s)', "x")
ok(G, "concat_nested_in_call", 'val a = "x"\nval b = a + a + "y" + a\nprint(b)', "xxyx")
for name, cond, truth in [
        ("lt_t", "1 < 2", 1), ("lt_f", "2 < 1", 0), ("lt_eq", "1 < 1", 0),
        ("le_t", "1 <= 2", 1), ("le_eq", "1 <= 1", 1), ("le_f", "2 <= 1", 0),
        ("gt_t", "2 > 1", 1), ("gt_f", "1 > 2", 0), ("gt_eq", "1 > 1", 0),
        ("ge_t", "2 >= 1", 1), ("ge_eq", "1 >= 1", 1), ("ge_f", "1 >= 2", 0),
        ("eq_t", "1 == 1", 1), ("eq_f", "1 == 2", 0),
        ("ne_t", "1 != 2", 1), ("ne_f", "1 != 1", 0),
        ("dbl_lt", "1.5 < 2.5", 1), ("dbl_eq", "1.5 == 1.5", 1),
        ("mix_lt", "1 < 1.5", 1), ("mix_gt", "2.5 > 2", 1),
        ("mix_eq_t", "2 == 2.0", 1), ("mix_eq_f", "2 == 2.5", 0),
        ("mix_ne", "2.0 != 2", 0), ("mix_le", "2.0 <= 2", 1), ("mix_ge", "2 >= 2.5", 0),
        ("str_lt", '"a" < "b"', 1), ("str_gt", '"b" > "a"', 1),
        ("str_prefix", '"ab" > "a"', 1), ("str_le", '"a" <= "a"', 1),
        ("str_ge", '"a" >= "b"', 0), ("str_eq", '"abc" == "abc"', 1),
        ("str_ne", '"abc" != "abd"', 1), ("str_case", '"A" < "a"', 1),
        ("str_empty", '"" < "a"', 1), ("unit_eq", "() == ()", 1), ("unit_ne", "() != ()", 0),
        ("arith_rel", "1 + 2 * 3 == 7", 1), ("arith_rel2", "2 * 2 < 3 + 3", 1),
        ("paren_cond", "(1 < 2)", 1), ("rel_vs_eq_paren", "(1 + 1) == (4 / 2)", 1)]:
    ok(G, "cond_" + name, f"if {cond} then\n  print(1)\nelse\n  print(0)", str(truth))
ok(G, "expr_stmt_literal", "1\nprint(2)", "2")
ok(G, "expr_stmt_paren", "(1 + 2) * 3\nprint(2)", "2")
ok(G, "expr_stmt_unary", "-1\nprint(2)", "2")
ok(G, "expr_stmt_unit", "()\nprint(2)", "2")
ok(G, "expr_stmt_var", "val a = 1\na\nprint(2)", "2")
ok(G, "expr_stmt_var_op", "val a = 1\na + 1\nprint(2)", "2")
ok(G, "expr_stmt_var_rel", "val a = 1\na\nprint(a)", "1")

# ---------------------------------------------------------------- variables
G = "11_variables"
ok(G, "val_int", "val x = 1\nprint(x)", "1")
ok(G, "typed_all", 'val a: Int = 1\nval b: Double = 1.5\nval c: String = "s"\nval d: Unit = ()\n'
   "print(a)\nprint(b)\nprint(c)\nprint(d)", "1" + hx(1.5) + "s()")
ok(G, "widen_decl", "val d: Double = 2\nprint(d)", hx(2.0))
ok(G, "widen_decl_expr", "val a = 3\nval d: Double = a + 1\nprint(d)", hx(4.0))
ok(G, "widen_assign", "var d = 1.5\nd = 2\nprint(d)", hx(2.0))
ok(G, "widen_assign_expr", "var d = 1.5\nval i = 3\nd = i * 2\nprint(d)", hx(6.0))
ok(G, "widen_assign_then_arith", "var d = 1.5\nd = 2\nval e = d + 0.5\nprint(e)", hx(2.5))
ok(G, "widen_decl_call", "val d: Double = readInt()\nprint(d)", hx(7.0), stdin="7\n")
ok(G, "widen_assign_call", "var d = 1.5\nd = readInt()\nprint(d)", hx(7.0), stdin="7\n")
ok(G, "infer_from_var", 'val a = "x"\nval b = a\nprint(b)', "x")
ok(G, "infer_unit", "val u = ()\nprint(u)", "()")
ok(G, "infer_unit_call", "val u = print(1)\nprint(u)", "1()")
ok(G, "var_update", "var x = 1\nx = x + 2\nx = x * x\nprint(x)", "9")
ok(G, "assign_string", 'var s = "a"\ns = s + "b"\nprint(s)', "ab")
ok(G, "assign_unit", "var u = ()\nu = ()\nprint(u)", "()")
ok(G, "assign_from_call", 'var s = "a"\ns = toStr(5)\nprint(s)', "5")
ok(G, "shadow_if", "val x = 1\nif x == 1 then\n  val x = 2\n  print(x)\nelse\n  print(0)\nprint(x)", "21")
ok(G, "shadow_type", 'val x = 1\nif x == 1 then\n  val x = "s"\n  print(x)\nelse\n  print(0)\nprint(x)', "s1")
ok(G, "shadow_else", "val x = 1\nif x == 2 then\n  print(0)\nelse\n  val x = 3\n  print(x)\nprint(x)", "31")
ok(G, "shadow_both_branches", "val x = 1\nif x == 1 then\n  val x = 2\n  print(x)\nelse\n  val x = 3\n"
   "  print(x)\nprint(x)", "21")
ok(G, "shadow_while", "var i = 0\nval x = 9\nwhile i < 2 do\n  val x = i\n  print(x)\n  i = i + 1\n"
   "print(x)", "019")
ok(G, "shadow_deep", "val x = 1\nif 1 < 2 then\n  val x = 2\n  if 1 < 2 then\n    val x = 3\n"
   "    print(x)\n  else\n    print(0)\n  print(x)\nelse\n  print(0)\nprint(x)", "321")
ok(G, "outer_assign_in_if", "var x = 1\nif x == 1 then\n  x = 2\nelse\n  x = 3\nprint(x)", "2")
ok(G, "outer_assign_in_while", "var x = 0\nvar i = 0\nwhile i < 3 do\n  x = x + i\n  i = i + 1\nprint(x)", "3")
ok(G, "shadow_uses_outer_after", "var x = 1\nif 1 < 2 then\n  var x = 5\n  x = 6\nelse\n  print(0)\n"
   "x = x + 1\nprint(x)", "2")
ok(G, "same_name_sibling_blocks", "if 1 < 2 then\n  val t = 1\n  print(t)\nelse\n  print(0)\n"
   "if 1 < 2 then\n  val t = 2\n  print(t)\nelse\n  print(0)", "12")
ok(G, "decl_in_loop_each_iter", "var i = 0\nwhile i < 3 do\n  var t = i\n  t = t * 2\n  print(t)\n"
   "  i = i + 1", "024")
ok(G, "semicolons", "val a = 1; val b = 2\nprint(a); print(b)", "12")
ok(G, "semicolons_three", "var a = 1; a = a + 1; print(a)", "2")
add(G, "semicolon_trailing", main_of("print(1);\nprint(2)"), (0, 2), "12")
ok(G, "many_vars", "\n".join(f"val v{i} = {i}" for i in range(50)) + "\nprint(v49)", "49")

# ---------------------------------------------------------------- control flow
G = "12_control"
ok(G, "if_true", "if 1 < 2 then\n  print(1)\nelse\n  print(2)", "1")
ok(G, "if_false", "if 2 < 1 then\n  print(1)\nelse\n  print(2)", "2")
ok(G, "if_unit_body", 'if 1 < 2 then\n  print("a")\nelse\n  ()', "a")
ok(G, "if_unit_body_else", 'if 2 < 1 then\n  ()\nelse\n  print("b")', "b")
ok(G, "if_both_unit", "if 1 < 2 then\n  ()\nelse\n  ()\nprint(1)", "1")
ok(G, "if_nested_tt", "if 1 < 2 then\n  if 1 < 2 then\n    print(1)\n  else\n    print(2)\nelse\n"
   "  print(3)", "1")
ok(G, "if_nested_tf", "if 1 < 2 then\n  if 2 < 1 then\n    print(1)\n  else\n    print(2)\nelse\n"
   "  print(3)", "2")
ok(G, "if_nested_else", "if 2 < 1 then\n  print(1)\nelse\n  if 1 < 2 then\n    print(2)\n"
   "  else\n    print(3)", "2")
ok(G, "if_sequence", "if 1 < 2 then\n  print(1)\nelse\n  print(0)\nif 2 < 1 then\n  print(0)\n"
   "else\n  print(2)\nprint(3)", "123")
ok(G, "while_zero_iter", "var i = 5\nwhile i < 3 do\n  print(i)\n  i = i + 1\nprint(9)", "9")
ok(G, "while_count", "var i = 0\nwhile i < 3 do\n  print(i)\n  i = i + 1", "012")
ok(G, "while_down", "var i = 3\nwhile i > 0 do\n  print(i)\n  i = i - 1", "321")
ok(G, "while_nested", "var i = 0\nwhile i < 2 do\n  var j = 0\n  while j < 2 do\n    print(i)\n"
   "    print(j)\n    j = j + 1\n  i = i + 1", "00011011")
ok(G, "while_in_if", "var i = 0\nif i == 0 then\n  while i < 2 do\n    print(i)\n    i = i + 1\n"
   "else\n  print(9)", "01")
ok(G, "if_in_while", 'var i = 0\nwhile i < 4 do\n  val h = i / 2 * 2\n  if h == i then\n    print("e")\n'
   '  else\n    print("o")\n  i = i + 1', "eoeo")
ok(G, "while_dbl_cond", "var d = 0.0\nvar n = 0\nwhile d < 1 do\n  d = d + 0.25\n  n = n + 1\nprint(n)", "4")
ok(G, "while_str_grow", 'var s = "a"\nvar n = 0\nwhile s != "aaaa" do\n  s = s + "a"\n  n = n + 1\nprint(n)', "3")
ok(G, "while_100k", "var i = 0\nwhile i < 20000 do\n  i = i + 1\nprint(i)", "20000")
deep_if = "var i = 0\n"
for k in range(20):
    deep_if += "  " * k + "if i < 1 then\n"
deep_if += "  " * 20 + "print(7)\n"
for k in range(19, -1, -1):
    deep_if += "  " * k + "else\n" + "  " * (k + 1) + "print(0)\n"
ok(G, "if_depth_20", deep_if, "7")
many_ifs = "".join(f"if {i} == {i} then\n  print(1)\nelse\n  print(0)\n" for i in range(300))
ok(G, "if_300_sequential", many_ifs, "1" * 300)

# ---------------------------------------------------------------- functions
G = "13_functions"
ok(G, "spec_factorial_example", """
    val n = readInt()
    val result = factorial(n)
    print(result)
    print("\\n")
""", "120\n", """
    def factorial(n: Int): Int =
      var result = 1
      var i: Int = n // uvedeni typu je nepovinne
      while i > 1 do
        result = result * i
        i = i - 1
      result // posledni vyraz v tele funkce urci navratovou hodnotu
""", stdin="5\n")
ok(G, "call_stmt", "f()", "x", 'def f(): Unit = print("x")')
ok(G, "call_stmt_block", "f()\nf()", "xyxy", 'def f(): Unit =\n  print("x")\n  print("y")')
ok(G, "one_line_int", "val x = f()\nprint(x)", "5", "def f(): Int = 5")
ok(G, "one_line_expr", "val x = f(2)\nprint(x)", "7", "def f(a: Int): Int = a + 5")
ok(G, "one_line_var", "val x = f(2)\nprint(x)", "2", "def f(a: Int): Int = a")
ok(G, "one_line_str", 'val x = f()\nprint(x)', "s", 'def f(): String = "s"')
ok(G, "one_line_call", "val x = f()\nprint(x)", "1", "def f(): Int = g()\ndef g(): Int = 1")
ok(G, "one_line_call_args", "val x = f(4)\nprint(x)", "4", "def f(a: Int): Int = g(a)\ndef g(b: Int): Int = b")
ok(G, "one_line_widen", "val x = f()\nprint(x)", hx(1.0), "def f(): Double = 1")
ok(G, "one_line_call_widen", "val x = f()\nprint(x)", hx(1.0), "def f(): Double = g()\ndef g(): Int = 1")
ok(G, "last_expr_literal", "val x = f()\nprint(x)", "5", "def f(): Int =\n  5")
ok(G, "last_expr_var", "val x = f()\nprint(x)", "3", "def f(): Int =\n  val a = 3\n  a")
ok(G, "last_expr_op", "val x = f()\nprint(x)", "4", "def f(): Int =\n  val a = 3\n  a + 1")
ok(G, "last_expr_paren", "val x = f()\nprint(x)", "9", "def f(): Int =\n  val a = 3\n  (a) * 3")
ok(G, "last_expr_unary", "val x = f()\nprint(x)", "-3", "def f(): Int =\n  val a = 3\n  -a")
ok(G, "last_expr_str", 'val x = f()\nprint(x)', "ab", 'def f(): String =\n  val a = "a"\n  a + "b"')
ok(G, "last_expr_widen", "val x = f()\nprint(x)", hx(3.0), "def f(): Double =\n  val a = 3\n  a")
ok(G, "last_call", "val x = f()\nprint(x)", "1", "def f(): Int =\n  val a = 3\n  g()\ndef g(): Int = 1")
ok(G, "last_call_widen", "val x = f()\nprint(x)", hx(1.0),
   "def f(): Double =\n  val a = 3\n  g()\ndef g(): Int = 1")
ok(G, "middle_expr_not_returned", "val x = f()\nprint(x)", "2", "def f(): Int =\n  1\n  2")
ok(G, "return_literal", "val x = f()\nprint(x)", "5", "def f(): Int =\n  return 5")
ok(G, "return_expr", "val x = f(2)\nprint(x)", "4", "def f(a: Int): Int =\n  return a * 2")
ok(G, "return_call", "val x = f()\nprint(x)", "1", "def f(): Int =\n  return g()\ndef g(): Int = 1")
ok(G, "return_widen", "val x = f()\nprint(x)", hx(1.0), "def f(): Double =\n  return 1")
ok(G, "return_widen_var", "val x = f(2)\nprint(x)", hx(2.0), "def f(a: Int): Double =\n  return a")
ok(G, "return_call_widen", "val x = f()\nprint(x)", hx(1.0),
   "def f(): Double =\n  return g()\ndef g(): Int = 1")
ok(G, "return_stops", "f()", "1", "def f(): Unit =\n  print(1)\n  return\n  print(2)")
ok(G, "return_in_if", "val x = f(0)\nval y = f(5)\nprint(x)\nprint(y)", "12", """
    def f(a: Int): Int =
      if a < 1 then
        return 1
      else
        return 2
""")
ok(G, "return_in_while", "val x = f(5)\nprint(x)", "3", """
    def f(n: Int): Int =
      var i = 0
      while i < n do
        if i == 3 then
          return i
        else
          i = i + 1
      return 0
""")
ok(G, "return_from_nested_keeps_frames", "val x = f()\nval y = f()\nprint(x)\nprint(y)", "11", """
    def f(): Int =
      var i = 0
      while i < 3 do
        var j = 0
        while j < 3 do
          if j == 1 then
            return j
          else
            j = j + 1
        i = i + 1
      return 0
""")
ok(G, "params_order", "val x = f(10, 3)\nprint(x)", "7", "def f(a: Int, b: Int): Int = a - b")
ok(G, "params_three_types", 'f(1, 1.5, "s")', "1" + hx(1.5) + "s",
   "def f(a: Int, b: Double, c: String): Unit =\n  print(a)\n  print(b)\n  print(c)")
ok(G, "param_unit", "f(())", "()", "def f(u: Unit): Unit = print(u)")
ok(G, "param_widen_literal", "val x = f(1)\nprint(x)", hx(1.5),
   "def f(a: Double): Double = a + 0.5")
ok(G, "param_widen_var", "val i = 1\nval x = f(i)\nprint(x)", hx(1.5),
   "def f(a: Double): Double = a + 0.5")
ok(G, "param_widen_second", 'f("s", 2)', "s" + hx(2.0),
   "def f(a: String, b: Double): Unit =\n  print(a)\n  print(b)")
ok(G, "param_shadow_in_body", "val x = f(1)\nprint(x)", "2",
   "def f(a: Int): Int =\n  if a == 1 then\n    val a = 2\n    return a\n  else\n    return a")
add(G, "forward_call_main_first", HEAD + "@main\ndef main(): Unit =\n  val x = twice(3)\n"
    "  print(x)\ndef twice(n: Int): Int = n * 2\n", 0, "6")
add(G, "main_in_middle", HEAD + "def a(): Int = 1\n@main def main(): Unit =\n  val x = a()\n"
    "  val y = b()\n  print(x)\n  print(y)\ndef b(): Int = 2\n", 0, "12")
add(G, "main_custom_name", HEAD + "@main\ndef start(): Unit =\n  print(1)\n", 0, "1")
add(G, "main_annot_same_line", HEAD + "@main def run(): Unit = print(1)\n", 0, "1")
add(G, "function_named_main_not_entry", HEAD + "def main(): Unit = print(0)\n@main\n"
    "def run(): Unit =\n  print(1)\n", 0, "1")
add(G, "main_inferred_unit", HEAD + "@main\ndef main() =\n  print(1)\n", 0, "1")
add(G, "main_inferred_unit_one_line", HEAD + "@main def main() = print(1)\n", 0, "1")
ok(G, "recursion_fact", "val x = fact(5)\nprint(x)", "120", """
    def fact(n: Int): Int =
      if n < 2 then
        return 1
      else
        val m = n - 1
        val r = fact(m)
        return n * r
""")
ok(G, "recursion_fib", "val x = fib(12)\nprint(x)", "144", """
    def fib(n: Int): Int =
      if n < 2 then
        return n
      else
        val a = n - 1
        val b = n - 2
        val x = fib(a)
        val y = fib(b)
        return x + y
""")
ok(G, "recursion_locals_keep", "val x = f(3)\nprint(x)", "6", """
    def f(n: Int): Int =
      val own = n
      if n == 0 then
        return 0
      else
        val m = n - 1
        val r = f(m)
        return own + r
""")
ok(G, "recursion_deep", "val x = f(2000)\nprint(x)", "2000", """
    def f(n: Int): Int =
      if n == 0 then
        return 0
      else
        val m = n - 1
        val r = f(m)
        return r + 1
""")
ok(G, "mutual_recursion", "val x = ev(10)\nval y = ev(7)\nprint(x)\nprint(y)", "10", """
    def ev(n: Int): Int =
      if n == 0 then
        return 1
      else
        val m = n - 1
        return od(m)
    def od(n: Int): Int =
      if n == 0 then
        return 0
      else
        val m = n - 1
        return ev(m)
""")
ok(G, "locals_not_clobbered_by_call", "val a = 1\nval b = f()\nprint(a)\nprint(b)", "12", """
    def f(): Int =
      val a = 2
      return a
""")
ok(G, "many_params", "val x = f(1, 2, 3, 4, 5, 6)\nprint(x)", "123456", """
    def f(a: Int, b: Int, c: Int, d: Int, e: Int, g: Int): Int =
      return a * 100000 + b * 10000 + c * 1000 + d * 100 + e * 10 + g
""")
ok(G, "multiline_params", "val x = f(1, 2)\nprint(x)", "3",
   "def f(\n    a: Int,\n    b: Int,\n  ): Int = a + b")
ok(G, "user_fn_named_like_label", "val x = else_0()\nif x == 1 then\n  print(1)\nelse\n  print(2)",
   "1", "def else_0(): Int = 1")
ok(G, "user_fn_named_while_label", "var i = 0\nwhile i < 1 do\n  i = while_start_0()\nprint(i)", "1",
   "def while_start_0(): Int = 1")
ok(G, "user_fn_named_readLine_end", "val s = readLine()\nreadLine_end()\nprint(s)", "ok\n",
   'def readLine_end(): Unit = ()', stdin="ok\n")

# ---------------------------------------------------------------- return rules
G = "14_return_rules"
ok(G, "infer_int", "val y = g()\nprint(y)", "5", "def g() = 5")
ok(G, "infer_str", 'val y = g()\nprint(y)', "s", 'def g() = "s"')
ok(G, "infer_dbl", "val y = g()\nprint(y)", hx(1.5), "def g() = 1.5")
ok(G, "infer_expr", "val y = g(2)\nprint(y)", "6", "def g(a: Int) = a * 3")
ok(G, "infer_from_call", "val y = g()\nprint(y)", "1", "def g() = h()\ndef h(): Int = 1")
ok(G, "infer_chain_forward", "val y = a()\nprint(y)", "1", "def a() = b()\ndef b() = c()\ndef c() = 1")
ok(G, "infer_chain_backward", "val y = a()\nprint(y)", "1", "def c() = 1\ndef b() = c()\ndef a() = b()")
ok(G, "infer_block_last_expr", "val y = g()\nprint(y)", "4", "def g() =\n  val a = 3\n  a + 1")
ok(G, "infer_block_last_call", "val y = g()\nprint(y)", "1", "def g() =\n  val a = 3\n  h()\ndef h(): Int = 1")
ok(G, "infer_called_before_def", "val y = g()\nval z: Int = y\nprint(z)", "5", "def g() = 5")
ok(G, "infer_used_in_expr_after", "val y = g()\nval z = y + 1\nprint(z)", "6", "def g() = 5")
ok(G, "infer_unit_print", "f()", "1", "def f() = print(1)")
ok(G, "infer_unit_block", "f()", "12", "def f() =\n  print(1)\n  print(2)")
ok(G, "infer_unit_value", "val u = f()\nprint(u)", "1()", "def f() =\n  print(1)")
ok(G, "infer_unit_literal", "val u = f()\nprint(u)", "()", "def f() = ()")
for name, helpers in [
        ("ends_val", "def f() =\n  val a = 1"),
        ("ends_var", "def f() =\n  var a = 1"),
        ("ends_assign", "def f() =\n  var a = 1\n  a = 2"),
        ("ends_while", "def f() =\n  var a = 0\n  while a < 1 do\n    a = a + 1")]:
    ok(G, "infer_unit_" + name, "val u = f()\nprint(u)", "()", helpers)
ok(G, "infer_unit_if", "f(0)\nf(5)", "ab", """
    def f(a: Int) =
      if a < 1 then
        print("a")
      else
        print("b")
""")
ok(G, "unit_discards_value", "val u = f()\nprint(u)", "()", "def f(): Unit = 5")
ok(G, "unit_discards_block", "val u = f()\nprint(u)", "()", "def f(): Unit =\n  1 + 2")
ok(G, "unit_discards_var", "val u = f()\nprint(u)", "()", "def f(): Unit =\n  val a = 1\n  a")
ok(G, "unit_discards_call", "val u = f()\nprint(u)", "()", "def f(): Unit = g()\ndef g(): Int = 1")
ok(G, "unit_discards_call_block", "val u = f()\nprint(u)", "()",
   "def f(): Unit =\n  g()\ndef g(): Int = 1")
ok(G, "unit_discards_return", "val u = f()\nprint(u)", "()", "def f(): Unit =\n  return 5")
ok(G, "unit_discards_return_call", "val u = f()\nprint(u)", "()",
   "def f(): Unit =\n  return g()\ndef g(): Int = 1")
ok(G, "unit_evaluates_value", "val u = f()\nprint(u)", "x()",
   'def f(): Unit = g()\ndef g(): Int =\n  print("x")\n  return 1')
ok(G, "unit_ends_if_values", "val u = f(1)\nprint(u)", "()", """
    def f(a: Int): Unit =
      if a < 1 then
        1
      else
        2
""")
ok(G, "unit_bare_return", "f()\nprint(1)", "1", "def f(): Unit =\n  return")
ok(G, "unit_bare_return_in_if", "f(0)\nf(5)", "b", """
    def f(a: Int): Unit =
      if a < 1 then
        return
      else
        print("b")
""")
ok(G, "if_all_return", "val x = f(0)\nprint(x)", "1", """
    def f(a: Int): Int =
      if a < 1 then
        return 1
      else
        return 2
""")
ok(G, "if_nested_all_return", "val x = f(5)\nprint(x)", "3", """
    def f(a: Int): Int =
      if a < 1 then
        return 1
      else
        if a < 3 then
          return 2
        else
          return 3
""")
ok(G, "return_then_last_expr", "val x = f(0)\nval y = f(5)\nprint(x)\nprint(y)", "12", """
    def f(a: Int): Int =
      if a < 1 then
        return 1
      else
        print("")
      2
""")
for name, helpers in [
        ("ret_type_literal", 'def f(): Int = "a"'),
        ("ret_type_stmt", 'def f(): Int =\n  return "a"'),
        ("ret_type_last", 'def f(): Int =\n  "a"'),
        ("ret_dbl_to_int", "def f(): Int =\n  return 1.5"),
        ("ret_dbl_to_int_one_line", "def f(): Int = 1.5"),
        ("ret_call_type", 'def f(): Int = g()\ndef g(): String = "a"'),
        ("ret_call_type_stmt", 'def f(): Int =\n  return g()\ndef g(): String = "a"'),
        ("ret_unit_to_int", "def f(): Int = ()"),
        ("ret_bool", "def f(): Int = 1 < 2"),
        ("ret_bool_stmt", "def f(): Int =\n  return 1 < 2"),
        ("ret_bool_inferred", "def f() = 1 < 2"),
        ("ret_bool_last", "def f() =\n  1 < 2"),
        ("ret_str_to_dbl", 'def f(): Double = "a"'),
        ("ends_decl", "def f(): Int =\n  val x = 1"),
        ("ends_assign", "def f(): Int =\n  var x = 1\n  x = 2"),
        ("ends_while", "def f(): Int =\n  var x = 1\n  while x < 1 do\n    x = 2"),
        ("ends_while_with_return", "def f(): Int =\n  var x = 1\n  while x < 2 do\n    return 1"),
        ("ends_if_values", "def f(a: Int): Int =\n  if a < 1 then\n    1\n  else\n    2"),
        ("ends_if_one_return", "def f(a: Int): Int =\n  if a < 1 then\n    return 1\n  else\n    print(2)"),
        ("ends_if_nested_missing", "def f(a: Int): Int =\n  if a < 1 then\n    return 1\n  else\n"
         "    if a < 2 then\n      return 2\n    else\n      print(1)"),
        ("infer_if_values", "def f(a: Int) =\n  if a < 1 then\n    1\n  else\n    2"),
        ("infer_if_mixed", "def f(a: Int) =\n  if a < 1 then\n    print(1)\n  else\n    2"),
        ("return_then_unit_last", "def f(): Int =\n  return 1\n  print(2)"),
        ("unreachable_bad_return", 'def f(): Int =\n  return 1\n  return "a"'),
        ("return_without_type", "def f() =\n  return 1"),
        ("return_nested_without_type", "def f(a: Int) =\n  if a < 1 then\n    return 1\n  else\n    return 2"),
        ("bare_return_without_type", "def f() =\n  return"),
        ("recursion_without_type", "def f(n: Int) = f(n)"),
        ("recursion_block_without_type", "def f(n: Int) =\n  val r = f(n)\n  r"),
        ("indirect_without_type", "def a(n: Int) = b(n)\ndef b(n: Int) = a(n)")]:
    err(G, "e_" + name, 6, "print(1)", helpers)
err(G, "e_bare_return_in_int", (6, 10), "print(1)", "def f(): Int =\n  return")
err(G, "e_use_inferred_type", 6, 'val x: String = g()', "def g() = 5")
err(G, "e_inferred_unit_as_int", 6, "val x: Int = g()", "def g() = print(1)")
err(G, "e_inferred_wrong_assign", 6, 'var x = "a"\nx = g()', "def g() = 5")

# ---------------------------------------------------------------- builtins
G = "15_builtins"
ok(G, "print_int", "print(42)", "42")
ok(G, "print_dbl", "print(1.5)", hx(1.5))
ok(G, "print_str", 'print("a b")', "a b")
ok(G, "print_unit", "print(())", "()")
ok(G, "print_var_unit", "val u = ()\nprint(u)", "()")
ok(G, "print_empty_str", 'print("")\nprint(1)', "1")
for name, stdin, value in [
        ("simple", "42\n", "42"), ("negative", "-5\n", "-5"), ("zero", "0\n", "0"),
        ("no_newline", "7", "7"), ("eof", "", "0"), ("empty_line", "\n", "0"),
        ("garbage", "abc\n", "0"), ("trailing_garbage", "12x\n", "0"),
        ("leading_space", " 42\n", "0"), ("trailing_space", "42 \n", "0"),
        ("underscore", "4_2\n", "0"), ("float", "1.5\n", "0")]:
    ok(G, "readInt_" + name, "val x = readInt()\nprint(x)", value, stdin=stdin)
ok(G, "readInt_two_lines", "val a = readInt()\nval b = readInt()\nprint(a)\nprint(b)", "12", stdin="1\n2\n")
ok(G, "readInt_bad_then_good", "val a = readInt()\nval b = readInt()\nprint(a)\nprint(b)", "02", stdin="x\n2\n")
for name, stdin, value in [
        ("simple", "1.5\n", 1.5), ("negative", "-2.5\n", -2.5), ("int_form", "3\n", 3.0),
        ("exp", "1e2\n", 100.0), ("eof", "", 0.0), ("garbage", "abc\n", 0.0),
        ("leading_space", " 1.5\n", 0.0), ("empty_line", "\n", 0.0)]:
    ok(G, "readDouble_" + name, "val x = readDouble()\nprint(x)", hx(value), stdin=stdin)
for name, stdin, value in [
        ("simple", "abc\n", "abc\n"), ("no_newline", "abc", "abc"), ("eof", "", ""),
        ("empty_line", "\n", "\n"), ("spaces", " a b \n", " a b \n")]:
    ok(G, "readLine_" + name, "val x = readLine()\nprint(x)", value, stdin=stdin)
ok(G, "readLine_two", "val a = readLine()\nval b = readLine()\nprint(b)\nprint(a)", "b\na\n", stdin="a\nb\n")
ok(G, "readLine_after_eof", "val a = readLine()\nval b = readLine()\nprint(a)\nprint(b)", "a\n", stdin="a\n")
ok(G, "read_mixed", "val a = readInt()\nval b = readLine()\nval c = readDouble()\nprint(a)\nprint(b)\n"
   "print(c)", "1x\n" + hx(2.5), stdin="1\nx\n2.5\n")
ok(G, "toStr_int", "val s = toStr(42)\nprint(s)", "42")
ok(G, "toStr_neg", "val i = 0 - 42\nval s = toStr(i)\nprint(s)", "-42")
ok(G, "toStr_dbl", "val s = toStr(1.5)\nprint(s)", "1.50")
ok(G, "toStr_dbl_round", "val s = toStr(1.999)\nprint(s)", "2.00")
ok(G, "toStr_dbl_int_val", "val d: Double = 2\nval s = toStr(d)\nprint(s)", "2.00")
ok(G, "toStr_str", 'val s = toStr("a")\nprint(s)', "a")
ok(G, "toStr_unit", "val s = toStr(())\nprint(s)", "()")
ok(G, "toStr_concat", 'val s = toStr(1)\nval t = s + "x"\nprint(t)', "1x")
ok(G, "toInt_trunc", "val x = toInt(3.9)\nprint(x)", "3")
ok(G, "toInt_neg", "val d = 0.0 - 3.9\nval x = toInt(d)\nprint(x)", "-3")
ok(G, "toInt_exact", "val x = toInt(2.0)\nprint(x)", "2")
ok(G, "toInt_widen_int_arg", "val x = toInt(1)\nprint(x)", "1")
ok(G, "strLength", 'val x = strLength("abcd")\nprint(x)', "4")
ok(G, "strLength_empty", 'val x = strLength("")\nprint(x)', "0")
ok(G, "strLength_escape", r'val x = strLength("a\nb")' + "\nprint(x)", "3")
ok(G, "subStr_mid", 'val x = subStr("abcd", 1, 3)\nprint(x)', "bc")
ok(G, "subStr_whole", 'val x = subStr("abcd", 0, 4)\nprint(x)', "abcd")
ok(G, "subStr_first", 'val x = subStr("abcd", 0, 1)\nprint(x)', "a")
ok(G, "subStr_last", 'val x = subStr("abcd", 3, 4)\nprint(x)', "d")
ok(G, "subStr_empty_range", 'val x = subStr("abcd", 2, 2)\nprint(x)\nprint(1)', "1")
ok(G, "subStr_twice_labels", 'val a = subStr("abcd", 0, 2)\nval b = subStr("abcd", 2, 4)\nprint(b)\nprint(a)', "cdab")
ok(G, "subStr_space", 'val x = subStr("a b c", 1, 4)\nprint(x)', " b ")
ok(G, "ord_first", 'val x = ord("ABC", 0)\nprint(x)', "65")
ok(G, "ord_last", 'val x = ord("ABC", 2)\nprint(x)', "67")
ok(G, "ord_past_end", 'val x = ord("ABC", 3)\nprint(x)', "-1")
ok(G, "ord_negative", 'val i = 0 - 1\nval x = ord("ABC", i)\nprint(x)', "-1")
ok(G, "ord_empty", 'val x = ord("", 0)\nprint(x)', "-1")
ok(G, "chr_A", "val x = chr(65)\nprint(x)", "A")
ok(G, "chr_newline", "val x = chr(10)\nprint(x)", "\n")
ok(G, "chr_space", "val x = chr(32)\nprint(x)", " ")
ok(G, "chr_127", "val x = chr(127)\nprint(x)", "\x7f")
ok(G, "chr_ord_roundtrip", 'val a = ord("z", 0)\nval b = chr(a)\nprint(b)', "z")

# ---------------------------------------------------------------- runtime errors
G = "16_runtime"
run_err(G, "int_div_zero", 57, "val x = 1 / 0\nprint(x)")
run_err(G, "int_div_zero_var", 57, "val z = 0\nval x = 5 / z\nprint(x)")
run_err(G, "dbl_div_zero", 57, "val x = 1.0 / 0.0\nprint(x)")
run_err(G, "mixed_div_zero", 57, "val x = 1.5 / 0\nprint(x)")
run_err(G, "output_before_error", 57, 'print("a")\nval x = 1 / 0\nprint("b")', out="a")
for name, args in [("start_neg", "m, 1"), ("end_neg", "0, m"), ("start_gt_end", "2, 1"),
                   ("start_eq_len", "3, 3"), ("start_gt_len", "5, 6"), ("end_gt_len", "0, 4")]:
    run_err(G, "subStr_" + name, 40, f'val m = 0 - 1\nval x = subStr("abc", {args})\nprint(x)')
run_err(G, "subStr_empty_string", 40, 'val x = subStr("", 0, 0)\nprint(x)')
run_err(G, "chr_negative", 58, "val c = 0 - 1\nval x = chr(c)\nprint(x)")
run_err(G, "chr_128", 58, "val x = chr(128)\nprint(x)")
run_err(G, "error_in_function", 57, "val x = f(0)\nprint(x)", helpers="def f(a: Int): Int = 1 / a")

# ---------------------------------------------------------------- data stack
G = "17_stack"
ok(G, "stmt_call_in_loop", "var i = 0\nwhile i < 500 do\n  f()\n  i = i + 1\nprint(i)", "500",
   "def f(): Int = 1")
ok(G, "stmt_print_then_expr", "print(1)\nval x = 2 + 3\nprint(x)", "15")
ok(G, "value_after_stmt_calls", "val x = f()\nprint(x)", "9", """
    def f(): Int =
      g()
      g()
      return 9
    def g(): Int = 1
""")
ok(G, "unit_call_result_clean", "val u = f()\nprint(u)", "1()", """
    def f(): Unit =
      print(1)
""")
ok(G, "expr_stmt_in_function", "val x = f()\nprint(x)", "7", """
    def f(): Int =
      1 + 1
      val a = 7
      a
""")
ok(G, "concat_after_calls", 'val a = toStr(1)\nval b = toStr(2)\nval c = a + b\nprint(c)', "12")

# ---------------------------------------------------------------- spacing, limits
G = "01_lexer"
for name, expr, value in [
        ("nospace_add", "1+2", "3"), ("nospace_sub", "3-1", "2"), ("nospace_mul", "2*3", "6"),
        ("nospace_div", "6/2", "3"), ("nospace_neg", "3+-1", "2"), ("nospace_subneg", "3--1", "4"),
        ("nospace_paren", "(1+2)*(3+4)", "21")]:
    ok(G, name, f"val x={expr}\nprint(x)", value)
ok(G, "nospace_lt_neg", "val a=1\nif a<-1 then\n  print(1)\nelse\n  print(0)", "0")
ok(G, "nospace_eq_neg", "val a=-1\nif a==-1 then\n  print(1)\nelse\n  print(0)", "1")
ok(G, "nospace_ne", "val a=1\nif a!=2 then\n  print(1)\nelse\n  print(0)", "1")
ok(G, "comment_after_then", "if 1 < 2 then // c\n  print(1)\nelse // d\n  print(2)", "1")
ok(G, "block_comment_before_stmt", "/* c */ print(1)", "1")
add(G, "prolog_comment_same_line", "import ifj26.* // x\n@main\ndef main(): Unit =\n  print(1)\n", 0, "1")
add(G, "main_annot_comment", HEAD + "@main // x\n\n// y\ndef main(): Unit =\n  print(1)\n", 0, "1")
add(G, "blank_before_prolog", "\n\n   \n" + HEAD + "@main\ndef main(): Unit =\n  print(1)\n", 0, "1")
add(G, "ml_string_then_stmt", HEAD + '@main\ndef main(): Unit =\n  val x = """a\nb""" ; print(x)\n'
    "  print(1)\n", 0, "a\nb1")
ok(G, "int_min", "val x = -2147483648\nprint(x)", "-2147483648")
G = "15_builtins"
for name, stdin, value in [
        ("max", "2147483647\n", "2147483647"), ("min", "-2147483648\n", "-2147483648"),
        ("over", "2147483648\n", "0"), ("under", "-2147483649\n", "0"),
        ("huge", "99999999999999999999\n", "0")]:
    ok(G, "readInt_range_" + name, "val x = readInt()\nprint(x)", value, stdin=stdin)
G = "06_types"
err(G, "unknown_type_name", (2, 3), "val x: Foo = 1")

# ---------------------------------------------------------------- FUNEXP
G = "ext_funexp"
GV = "def getValue(): Int = 5"
ok(G, "screen_val_plain", "val x = 2 + 3 * 4\nprint(x)", "14", GV)
ok(G, "screen_val_call", "val x = getValue()\nprint(x)", "5", GV)
ok(G, "screen_print_call", "print(getValue())", "5", GV)
ok(G, "screen_val_call_plus", "val x = getValue() + 1\nprint(x)", "6", GV)
ok(G, "screen_print_mul", "print(2 * getValue())", "10", GV)
ok(G, "screen_return_plus", "val x = f()\nprint(x)", "6", GV + "\ndef f(): Int =\n  return getValue() + 1")
ok(G, "plus_call", "val x = 1 + getValue()\nprint(x)", "6", GV)
ok(G, "call_plus_call", "val x = getValue() + getValue()\nprint(x)", "10", GV)
ok(G, "prec_call", "val x = 1 + getValue() * 2\nprint(x)", "11", GV)
ok(G, "paren_call", "val x = (getValue() + 1) * 2\nprint(x)", "12", GV)
ok(G, "paren_around_call", "val x = (getValue())\nprint(x)", "5", GV)
ok(G, "unary_call", "val x = -getValue()\nprint(x)", "-5", GV)
ok(G, "sub_order", "val x = 10 - getValue()\nprint(x)", "5", GV)
ok(G, "div_order", "val x = 10 / getValue()\nprint(x)", "2", GV)
ok(G, "cond_if", "if getValue() > 1 then\n  print(1)\nelse\n  print(0)", "1", GV)
ok(G, "cond_while", "var i = 0\nwhile i < getValue() do\n  i = i + 1\nprint(i)", "5", GV)
ok(G, "assign", "var x = 1\nx = x + getValue()\nprint(x)", "6", GV)
ok(G, "last_expr", "val x = f()\nprint(x)", "6", GV + "\ndef f(): Int =\n  getValue() + 1")
ok(G, "one_line", "val x = f()\nprint(x)", "6", GV + "\ndef f(): Int = getValue() + 1")
ok(G, "expr_stmt", "getValue() + 1\nprint(2)", "2", GV)
ok(G, "arg_expr", "val x = inc(2 + 3)\nprint(x)", "6", "def inc(a: Int): Int = a + 1")
ok(G, "arg_call", "val x = inc(inc(1))\nprint(x)", "3", "def inc(a: Int): Int = a + 1")
ok(G, "arg_call_expr", "val x = inc(inc(1) * 2) + 1\nprint(x)", "6", "def inc(a: Int): Int = a + 1")
ok(G, "two_args_exprs", "val x = sub(2 * 5, inc(2))\nprint(x)", "7",
   "def inc(a: Int): Int = a + 1\ndef sub(a: Int, b: Int): Int = a - b")
ok(G, "arg_eval_left_to_right", "val x = sub(a(), b())\nprint(x)", "ab-1", """
    def a(): Int =
      print("a")
      return 1
    def b(): Int =
      print("b")
      return 2
    def sub(x: Int, y: Int): Int = x - y
""")
ok(G, "operand_left_to_right", "val x = a() - b()\nprint(x)", "ab-1", """
    def a(): Int =
      print("a")
      return 1
    def b(): Int =
      print("b")
      return 2
""")
ok(G, "widen_operand", "val x = getValue() + 0.5\nprint(x)", hx(5.5), GV)
ok(G, "widen_operand_left", "val x = 0.5 + getValue()\nprint(x)", hx(5.5), GV)
ok(G, "widen_arg_expr", "val x = half(1 + 2)\nprint(x)", hx(1.5), "def half(a: Double): Double = a / 2")
ok(G, "widen_call_arg", "val x = half(getValue())\nprint(x)", hx(2.5),
   GV + "\ndef half(a: Double): Double = a / 2")
ok(G, "concat_calls", 'val x = s() + "-" + s()\nprint(x)', "a-a", 'def s(): String = "a"')
ok(G, "spec_example", 'val result = 2 * factorial(readInt() + 1)\nprint(toStr(result + strLength(subStr("abcd", 1, 3))))',
   "242", """
    def factorial(n: Int): Int =
      if n < 2 then
        return 1
      else
        return n * factorial(n - 1)
""", stdin="4\n")
ok(G, "recursive_in_expr", "val x = fib(10)\nprint(x)", "55", """
    def fib(n: Int): Int =
      if n < 2 then
        return n
      else
        return fib(n - 1) + fib(n - 2)
""")
ok(G, "builtin_in_expr", 'val x = strLength("abc") + ord("A", 0)\nprint(x)', "68")
ok(G, "print_concat_toStr", 'print("v=" + toStr(1 + 2))', "v=3")
ok(G, "unit_call_stmt_inside", "val x = f() + 1\nprint(x)", "x2", """
    def f(): Int =
      print("x")
      print("")
      return 1
""")
ok(G, "multiline_args_in_expr", "val x = 1 + add(\n    1,\n    2,\n  ) * 2\nprint(x)", "7",
   "def add(a: Int, b: Int): Int = a + b")
ok(G, "nested_paren_args", "val x = add((1 + 2) * 3, (4))\nprint(x)", "13",
   "def add(a: Int, b: Int): Int = a + b")
ok(G, "unit_arg_nested", "print(id(()))", "()", "def id(u: Unit): Unit = u")
err(G, "e_undefined_in_expr", 3, "val x = nope() + 1")
err(G, "e_var_called", 3, "val a = 1\nval x = a() + 1")
err(G, "e_arg_count", 5, "val x = getValue(1) + 1", GV)
err(G, "e_arg_type", 5, 'val x = inc("a") + 1', "def inc(a: Int): Int = a + 1")
err(G, "e_unit_in_expr", 6, "val x = print(1) + 1")
err(G, "e_string_plus_int", 6, 'val x = getValue() + "a"', GV)
err(G, "e_result_type", 6, 'val x: String = getValue() + 1', GV)
err(G, "e_bool_arg", 6, "print(1 < getValue())", GV)
err(G, "e_call_no_close", (2, 5), "val x = getValue( + 1", GV)
err(G, "e_call_then_term", 2, "val x = getValue() 1", GV)
err(G, "e_same_line_comma", 2, "val x = inc(1,) + 1", "def inc(a: Int): Int = a + 1")


# ---------------------------------------------------------------- random
class _Overflow(Exception):
    pass


LIMIT = 2**31 - 1


def _check(value):
    if isinstance(value, int) and abs(value) > LIMIT:
        raise _Overflow()
    return value


def _idiv(a, b):
    q = abs(a) // abs(b)
    return q if (a < 0) == (b < 0) else -q


def _literal(rng, want_double):
    if want_double:
        value = rng.choice([.5, .25, 1.5, 2.0, 3.75, 10.0, 0.125])
        return repr(value), value
    value = rng.randint(0, 20)
    return str(value), value


def _expr(rng, depth, env, doubles):
    """Returns (text, precedence, value); 0=atom 1=unary 2=mul 3=add."""
    if depth == 0 or rng.random() < .2:
        if env and rng.random() < .5:
            name = rng.choice(sorted(env))
            return name, 0, env[name]
        text, value = _literal(rng, doubles and rng.random() < .4)
        return text, 0, value
    choice = rng.random()
    if choice < .12:
        text, prec, value = _expr(rng, depth - 1, env, doubles)
        inner = f"({text})" if prec > 1 else text
        return "- " + inner if inner.startswith("-") else "-" + inner, 1, -value
    if choice < .2:
        text, _, value = _expr(rng, depth - 1, env, doubles)
        return f"({text})", 0, value
    op = rng.choice("+-*/")
    prec = 2 if op in "*/" else 3
    lt, lp, lv = _expr(rng, depth - 1, env, doubles)
    rt, rp, rv = _expr(rng, depth - 1, env, doubles)
    if op == "/" and rv == 0:
        op, prec = "+", 3
    left = f"({lt})" if lp > prec else lt
    right = f"({rt})" if rp >= prec else rt
    is_double = isinstance(lv, float) or isinstance(rv, float)
    if op == "+":
        value = lv + rv
    elif op == "-":
        value = lv - rv
    elif op == "*":
        value = lv * rv
    else:
        value = float(lv) / float(rv) if is_double else _idiv(lv, rv)
    _check(value)
    if is_double and abs(value) > 1e200:
        raise _Overflow()
    return f"{left} {op} {right}", prec, float(value) if is_double else value


def _fuzz_expressions(rng, count, doubles):
    made = 0
    while made < count:
        env = {"a": rng.randint(-9, 9), "b": rng.randint(1, 9)}
        if doubles:
            env["c"] = rng.choice([.5, 1.5, 2.25])
        try:
            text, _, value = _expr(rng, rng.randint(2, 5), env, doubles)
        except _Overflow:
            continue
        decl = "".join(f"val {k} = {'0 - ' + str(-v) if isinstance(v, int) and v < 0 else v}\n"
                       for k, v in env.items())
        out = hx(value) if isinstance(value, float) else str(value)
        kind = "dbl" if doubles else "int"
        add(f"fuzz_expr_{kind}", f"{made:03d}", main_of(f"{decl}val r = {text}\nprint(r)"), 0, out)
        made += 1


def _fuzz_relations(rng, count):
    ops = {"<": lambda a, b: a < b, "<=": lambda a, b: a <= b, ">": lambda a, b: a > b,
           ">=": lambda a, b: a >= b, "==": lambda a, b: a == b, "!=": lambda a, b: a != b}
    made = 0
    while made < count:
        env = {"a": rng.randint(0, 5), "b": rng.choice([.5, 2.0, 3.0])}
        try:
            lt, _, lv = _expr(rng, 2, env, True)
            rt, _, rv = _expr(rng, 2, env, True)
        except _Overflow:
            continue
        op = rng.choice(sorted(ops))
        decl = f"val a = {env['a']}\nval b = {env['b']}\n"
        body = f"{decl}if {lt} {op} {rt} then\n  print(1)\nelse\n  print(0)"
        add("fuzz_relations", f"{made:03d}", main_of(body), 0, "1" if ops[op](lv, rv) else "0")
        made += 1


def _int_expr(rng, depth, names):
    """Returns the same integer expression as IFJ26 text and Python text."""
    if depth == 0 or rng.random() < .35:
        if names and rng.random() < .65:
            name = rng.choice(names)
            return name, name, 0
        value = rng.randint(0, 9)
        return str(value), str(value), 0
    op = rng.choice("+-*/")
    prec = 2 if op in "*/" else 3
    lt, lp, lprec = _int_expr(rng, depth - 1, names)
    if op == "/":
        value = rng.randint(1, 5)
        rt, rp, rprec = str(value), str(value), 0
    else:
        rt, rp, rprec = _int_expr(rng, depth - 1, names)
    left = f"({lt})" if lprec > prec else lt
    right = f"({rt})" if rprec >= prec else rt
    if op == "/":
        return f"{left} / {right}", f"_c(_idiv({lp}, {rp}))", prec
    return f"{left} {op} {right}", f"_c(({lp}) {op} ({rp}))", prec


_COMPARE = ["<", "<=", ">", ">=", "==", "!="]


class _Gen:
    def __init__(self, rng):
        self.rng, self.counter = rng, 0

    def fresh(self, prefix):
        self.counter += 1
        return f"{prefix}{self.counter}"

    def block(self, depth, names, mutable, funcs, level):
        ifj, py = [], []
        names, mutable = list(names), list(mutable)
        for _ in range(self.rng.randint(1, 4)):
            self.stmt(depth, names, mutable, funcs, level, ifj, py)
        return ifj, py

    def stmt(self, depth, names, mutable, funcs, level, ifj, py):
        rng = self.rng
        pad, ppad = "  " * level, "    " * level
        kind = rng.random()
        if kind < .22:
            t, p, _ = _int_expr(rng, 3, names)
            name = self.fresh("t")
            ifj.append(f"{pad}{'var' if rng.random() < .5 else 'val'} {name} = {t}")
            py.append(f"{ppad}{name} = {p}")
            names.append(name)
        elif kind < .45 and mutable:
            name = rng.choice(mutable)
            t, p, _ = _int_expr(rng, 3, names)
            ifj.append(f"{pad}{name} = {t}")
            py.append(f"{ppad}{name} = {p}")
        elif kind < .6 and names:
            name = rng.choice(names)
            ifj.append(f"{pad}print({name})")
            py.append(f"{ppad}_o({name})")
        elif kind < .7 and funcs and names:
            func, arity = rng.choice(funcs)
            args = [rng.choice(names) for _ in range(arity)]
            name = self.fresh("r")
            ifj.append(f"{pad}val {name} = {func}({', '.join(args)})")
            py.append(f"{ppad}{name} = {func}({', '.join(args)})")
            names.append(name)
        elif kind < .85 and depth > 0:
            lt, lp, _ = _int_expr(rng, 2, names)
            rt, rp, _ = _int_expr(rng, 2, names)
            op = rng.choice(_COMPARE)
            then_i, then_p = self.block(depth - 1, names, mutable, funcs, level + 1)
            else_i, else_p = self.block(depth - 1, names, mutable, funcs, level + 1)
            ifj += [f"{pad}if {lt} {op} {rt} then"] + then_i + [f"{pad}else"] + else_i
            py += [f"{ppad}if ({lp}) {op} ({rp}):"] + then_p + [f"{ppad}else:"] + else_p
        elif depth > 0:
            counter = self.fresh("k")
            bound = rng.randint(0, 4)
            body_i, body_p = self.block(depth - 1, names + [counter], mutable,
                                        funcs, level + 1)
            step_i = f"{pad}  {counter} = {counter} + 1"
            step_p = f"{ppad}    {counter} = {counter} + 1"
            ifj += [f"{pad}var {counter} = 0", f"{pad}while {counter} < {bound} do"] + body_i + [step_i]
            py += [f"{ppad}{counter} = 0", f"{ppad}while {counter} < {bound}:"] + body_p + [step_p]
        else:
            name = rng.choice(names) if names else None
            if name:
                ifj.append(f"{pad}print({name})")
                py.append(f"{ppad}_o({name})")
            else:
                ifj.append(f"{pad}print(0)")
                py.append(f"{ppad}_o(0)")

    def function(self, name, arity, funcs, implicit):
        params = [f"p{i}" for i in range(arity)]
        header_i = f"def {name}({', '.join(p + ': Int' for p in params)}): Int ="
        header_p = f"def {name}({', '.join(params)}):"
        var = self.fresh("l")
        body_i, body_p = self.block(1, params + [var], [var], funcs, 1)
        t, p, _ = _int_expr(self.rng, 2, params + [var])
        last_i = f"  {t}" if implicit else f"  return {t}"
        return ([header_i, f"  var {var} = 1"] + body_i + [last_i],
                [header_p, f"    {var} = 1"] + body_p + [f"    return {p}"])


def _fuzz_programs(rng, count, group, with_funcs, implicit=False):
    made = attempts = 0
    while made < count and attempts < count * 60:
        attempts += 1
        gen = _Gen(rng)
        funcs, ifj, py = [], [], []
        if with_funcs:
            for index in range(rng.randint(1, 3)):
                arity = rng.randint(0, 3)
                name = f"fn{index}"
                fi, fp = gen.function(name, arity, list(funcs), implicit)
                ifj += fi
                py += fp
                funcs.append((name, arity))
        body_i, body_p = gen.block(3, ["a", "b"], ["a", "b"], funcs, 1)
        source = HEAD + "\n".join(ifj) + ("\n" if ifj else "") + "@main\ndef main(): Unit =\n" \
            "  var a = 3\n  var b = 7\n" + "\n".join(body_i) + "\n"
        code = "\n".join(py + ["def _main():", "    a = 3", "    b = 7"] + body_p + ["_main()"])
        output = []
        scope = {"_c": _check, "_idiv": _idiv, "_o": lambda v: output.append(str(v))}
        try:
            exec(compile(code, "fuzz", "exec"), scope)
        except (_Overflow, RecursionError, UnboundLocalError, NameError):
            continue
        add(group, f"{made:03d}", source, 0, "".join(output))
        made += 1


def add_fuzz(seed=26, scale=1):
    """Adds deterministic random programs with outputs from a Python model."""
    rng = random.Random(seed)
    _fuzz_expressions(rng, 60 * scale, False)
    _fuzz_expressions(rng, 60 * scale, True)
    _fuzz_relations(rng, 40 * scale)
    _fuzz_programs(rng, 40 * scale, "fuzz_control", False)
    _fuzz_programs(rng, 40 * scale, "fuzz_functions", True)
    _fuzz_programs(rng, 20 * scale, "fuzz_implicit_ret", True, implicit=True)
