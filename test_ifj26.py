#!/usr/bin/env python3
"""AI-assisted black-box tests for base IFJ26; Python standard library only."""

import argparse
from collections import Counter, defaultdict
from dataclasses import dataclass
import difflib
import json
import math
from pathlib import Path
import shlex
import shutil
import subprocess
import sys
import tempfile
import textwrap
import time


@dataclass(frozen=True)
class Case:
    name: str
    group: str
    source: str
    compile_rc: int = 0
    stdout: bytes = b""
    stdin: bytes = b""
    runtime_rc: int = 0


def program(body, helpers=""):
    """Wrap a body in a valid base-language program."""
    helpers = textwrap.dedent(helpers).strip("\n")
    body = textwrap.indent(textwrap.dedent(body).strip("\n"), "    ")
    return ("import ifj26.*\n" + (helpers + "\n" if helpers else "")
            + "@main def run(): Unit =\n" + body + "\n")


def cases(strict_zeros=False):
    # Calls have term arguments: these tests do not require FUNEXP.
    tests = [
        Case("print_exact", "output", program('print("OK")'), stdout=b"OK"),
        Case("print_newline", "output", program(r'print("OK\n")'), stdout=b"OK\n"),
        Case("unit", "output", program("print(())"), stdout=b"()"),
        Case("arithmetic_precedence", "expressions",
             program("val x = 2 + 3 * 4\nprint(x)"), stdout=b"14"),
        Case("parentheses", "expressions",
             program("val x = (2 + 3) * 4\nprint(x)"), stdout=b"20"),
        Case("integer_division", "expressions",
             program("val x = (0 - 5) / 2\nprint(x)"), stdout=b"-2"),
        Case("int_to_double", "expressions",
             program("val x: Double = 2\nval s = toStr(x)\nprint(s)"), stdout=b"2.00"),
        Case("string_concat", "strings",
             program('val x = "A" + "B"\nprint(x)'), stdout=b"AB"),
        Case("string_escapes", "strings",
             program(r'print("A\tB\n\"C\"\\")'), stdout=b'A\tB\n"C"\\'),
        Case("multiline_literal", "strings",
             'import ifj26.*\n@main def run(): Unit =\n'
             '    val x = """A\nB"""\n    print(x)\n', stdout=b"A\nB"),
        Case("seven_quotes", "strings",
             program('val x = ' + '"' * 7 + '\nprint(x)'), stdout=b'"'),
        Case("nested_comments", "layout",
             program("val x = 1 /* outer /* inner */ outer */ + 2\nprint(x)"),
             stdout=b"3"),
        Case("semicolon", "layout", program("val x = 7; print(x)"), stdout=b"7"),
        Case("no_final_newline", "layout", program('print("EOF")').rstrip("\n"),
             stdout=b"EOF"),
        Case("multiline_argument", "layout",
             program('print(\n    "OK",\n    )'), stdout=b"OK"),
        Case("numeric_separators", "numbers",
             program("val x = 1__000\nprint(x)"), stdout=b"1000"),
        Case("exponent_leading_zeros", "numbers",
             program("val x = 2e001\nval s = toStr(x)\nprint(s)"), stdout=b"20.00"),
        Case("assignment", "variables",
             program("var x = 1\nx = x + 2\nprint(x)"), stdout=b"3"),
        Case("shadowing", "scopes", program('''
            val x = 1
            if x == 1 then
                val x = 2
                print(x)
            else
                print(0)
            print(x)
        '''), stdout=b"21"),
        Case("while", "control", program('''
            var x = 0
            while x < 3 do
                print(x)
                x = x + 1
        '''), stdout=b"012"),
        Case("forward_call", "functions",
             program("val x = twice(3)\nprint(x)")
             + "def twice(n: Int): Int = n * 2\n", stdout=b"6"),
        Case("recursion", "functions",
             program("val x = fact(5)\nprint(x)", '''
                 def fact(n: Int): Int =
                     if n == 0 then
                         return 1
                     else
                         val previous = n - 1
                         val result = fact(previous)
                         return n * result
             '''), stdout=b"120"),
        Case("builtin_strings", "builtins", program('''
            val a = strLength("abcd")
            val b = subStr("abcd", 1, 3)
            val c = ord("ABC", 1)
            val d = chr(65)
            print(a)
            print(b)
            print(c)
            print(d)
        '''), stdout=b"4bc66A"),
        Case("read_int", "input", program("val x = readInt()\nprint(x)"),
             stdin=b"42\n", stdout=b"42"),
        Case("read_int_invalid", "input", program("val x = readInt()\nprint(x)"),
             stdin=b"abc\n", stdout=b"0"),
        Case("read_line", "input", program("val x = readLine()\nprint(x)"),
             stdin=b"hello\n", stdout=b"hello\n"),
        Case("unknown_character", "lexical", program("val x = ?"), 1),
        Case("invalid_escape", "lexical", program(r'print("\q")'), 1),
        Case("unclosed_string", "lexical", program('print("abc)'), 1),
        Case("unclosed_comment", "lexical", program("/* unclosed"), 1),
        Case("tab_indentation", "lexical",
             'import ifj26.*\n@main def run(): Unit =\n\tprint(1)\n', 1),
        Case("unfinished_exponent", "lexical", program("val x = 1e+"), 1),
        Case("integer_leading_zero", "lexical", program("val x = 01"), 1),
        Case("missing_prolog", "syntax", '@main def run(): Unit = print(1)\n', 2),
        Case("missing_assignment", "syntax", program("val x 1"), 2),
        Case("unfinished_expression", "syntax", program("val x = 1 +"), 2),
        Case("missing_right_paren", "syntax", program("print(1"), 2),
        Case("same_line_trailing_comma", "syntax", program("print(1,)"), 2),
        Case("undefined_variable", "undefined", program("print(missing)"), 3),
        Case("undefined_function", "undefined", program("missing()"), 3),
        Case("missing_main", "undefined", "import ifj26.*\ndef f(): Int = 1\n", 3),
        Case("out_of_scope", "scopes", program('''
            if 1 == 1 then
                val hidden = 1
            else
                print(0)
            print(hidden)
        '''), 3),
        Case("duplicate_variable", "redefinition", program("val x = 1\nval x = 2"), 4),
        Case("duplicate_function", "redefinition",
             program("print(1)", "def f(): Int = 1\ndef f(): Int = 2"), 4),
        Case("argument_count", "arguments", program("print()"), 5),
        Case("argument_type", "arguments", program("strLength(1)"), 5),
        Case("main_parameter", "arguments",
             'import ifj26.*\n@main def run(x: Int): Unit = print(x)\n', 5),
        Case("initializer_type", "types", program('val x: Int = "text"'), 6),
        Case("assignment_type", "types", program('var x = 1\nx = "text"'), 6),
        Case("expression_type", "types", program('val x = "text" - 1'), 6),
        Case("return_type", "types",
             program("print(1)", 'def f(): Int = "text"'), 6),
        Case("main_return_type", "types",
             "import ifj26.*\n@main def run(): Int = 1\n", 6),
        Case("immutable_assignment", "immutable", program("val x = 1\nx = 2"), 7),
        Case("parameter_assignment", "immutable",
             program("print(1)", "def f(x: Int): Unit =\n    x = 2"), 7),
        Case("substring_bounds", "runtime", program('subStr("abc", 3, 3)'),
             runtime_rc=40),
        Case("division_by_zero", "runtime", program("val x = 1 / 0\nprint(x)"),
             runtime_rc=57),
    ]
    # Optional team policy: the supplied PDF does not explicitly settle these.
    if strict_zeros:
        for literal in ("01.5", "01e2", "0_1.5"):
            tests.append(Case("leading_zero_" + literal.replace(".", "_"),
                              "team_policy", program("val x = " + literal), 1))
    return tests


def command(text):
    """Parse an argv list without a shell; resolve relative executable paths."""
    parts = shlex.split(text)
    if not parts:
        raise ValueError("Prázdný příkaz.")
    executable = shutil.which(parts[0])
    if executable is None:
        raise ValueError("Nenalezený nebo nespustitelný program: " + parts[0])
    parts[0] = str(Path(executable).resolve())
    return parts


def execute(argv, data, timeout, folder, prefix):
    start = time.monotonic()
    try:
        proc = subprocess.run(argv, input=data, stdout=subprocess.PIPE,
                              stderr=subprocess.PIPE, timeout=timeout)
        rc, out, err = proc.returncode, proc.stdout, proc.stderr
        problem = ""
    except subprocess.TimeoutExpired as exc:
        rc, out, err = None, exc.stdout or b"", exc.stderr or b""
        problem = "Překročen časový limit."
    except OSError as exc:
        rc, out, err = None, b"", str(exc).encode("utf-8")
        problem = "Proces se nepodařilo spustit."
    (folder / (prefix + ".stdout")).write_bytes(out)
    (folder / (prefix + ".stderr")).write_bytes(err)
    return rc, out, problem, time.monotonic() - start


def output_difference(expected, actual):
    # Byte comparison is authoritative; repr also reveals invisible characters.
    offset = next((i for i, (a, b) in enumerate(zip(expected, actual)) if a != b),
                  min(len(expected), len(actual)))
    details = (f"První rozdíl na bajtu {offset}; délka očekávaná/skutečná: "
               f"{len(expected)}/{len(actual)}.\n"
               f"Očekáváno: {expected[:200]!r}\nSkutečnost: {actual[:200]!r}\n")
    diff = difflib.unified_diff(
        expected.decode("utf-8", "backslashreplace").splitlines(keepends=True),
        actual.decode("utf-8", "backslashreplace").splitlines(keepends=True),
        fromfile="expected", tofile="actual")
    return details + "".join(diff)[:4000]


def run_case(case, compiler, interpreter, timeout, folder):
    folder.mkdir()
    source = case.source.encode("utf-8")
    (folder / "source.ifj26").write_bytes(source)
    (folder / "input.txt").write_bytes(case.stdin)
    (folder / "expected.stdout").write_bytes(case.stdout)
    result = {"name": case.name, "group": case.group, "status": "FAIL",
              "phase": "compile", "reason": "", "seconds": 0.0,
              "expected_compile_rc": case.compile_rc, "compile_rc": None,
              "expected_runtime_rc": case.runtime_rc, "runtime_rc": None}
    rc, code, problem, elapsed = execute(compiler, source, timeout, folder, "compiler")
    result.update(compile_rc=rc, seconds=elapsed)
    (folder / "program.code").write_bytes(code)
    if problem:
        result["reason"] = problem
    elif rc != case.compile_rc:
        result["reason"] = f"Překladač: očekáván kód {case.compile_rc}, vrácen {rc}."
    elif case.compile_rc != 0:
        result.update(status="PASS", reason="Správný chybový kód překladače.")
    else:
        # This catches empty/debug-only output even without an interpreter.
        lines = [line.split(b"#", 1)[0].strip() for line in code.splitlines()]
        first = next((line for line in lines if line), b"")
        if first.lower() != b".ifjcode26":
            result.update(phase="generated_code", reason="Chybí hlavička .IFJcode26 nebo jí předchází cizí výpis.")
        elif interpreter is None:
            result.update(status="PARTIAL", phase="not_run",
                          reason="Překlad prošel; běh a výstup bez interpretu neověřeny.")
        else:
            argv = [part.replace("{code}", str(folder / "program.code"))
                    .replace("{input}", str(folder / "input.txt")) for part in interpreter]
            if not any("{code}" in part for part in interpreter):
                argv.append(str(folder / "program.code"))
            rc, out, problem, elapsed = execute(argv, case.stdin, timeout, folder, "runtime")
            result.update(phase="runtime", runtime_rc=rc,
                          seconds=result["seconds"] + elapsed)
            if problem:
                result["reason"] = problem
            elif rc != case.runtime_rc:
                result["reason"] = f"Interpret: očekáván kód {case.runtime_rc}, vrácen {rc}."
            elif out != case.stdout:
                result.update(phase="output", reason="Výstup se liší bajt po bajtu.")
                (folder / "output.diff").write_text(output_difference(case.stdout, out), encoding="utf-8")
            else:
                result.update(status="PASS", phase="complete",
                              reason="Správný překlad, běhový kód i přesný výstup.")
    (folder / "result.json").write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding="utf-8")
    return result


def main():
    parser = argparse.ArgumentParser(description="Black-box testy základního IFJ26.")
    parser.add_argument("--compiler", help="Příkaz překladače, například './ifj26'.")
    parser.add_argument("--interpreter", help="Například './ic26int' nebo './ic26int {code}'.")
    parser.add_argument("--timeout", type=float, default=5.0, help="Limit každého procesu v sekundách.")
    parser.add_argument("--out", default="test-results", help="Nadřazený adresář výsledků.")
    parser.add_argument("--filter", default="", help="Část názvu testu nebo skupiny.")
    parser.add_argument("--strict-leading-zeros", action="store_true", help="Přidat tři testy týmového pravidla pro úvodní nuly Double.")
    parser.add_argument("--list", action="store_true", help="Pouze vypsat testy.")
    args = parser.parse_args()
    selected = [c for c in cases(args.strict_leading_zeros)
                if args.filter in c.name or args.filter in c.group]
    if not selected:
        parser.error("Filtru neodpovídá žádný test.")
    if args.list:
        for c in selected:
            print(f"{c.name:30} {c.group:15} compiler={c.compile_rc} runtime={c.runtime_rc}")
        return 0
    if not math.isfinite(args.timeout) or args.timeout <= 0:
        parser.error("--timeout musí být kladné konečné číslo.")
    if not args.compiler:
        parser.error("Zadej --compiler nebo použij --list.")
    try:
        compiler = command(args.compiler)
        interpreter = command(args.interpreter) if args.interpreter else None
        parent = Path(args.out).resolve()
        parent.mkdir(parents=True, exist_ok=True)
        folder = Path(tempfile.mkdtemp(prefix="run-", dir=parent))
    except (ValueError, OSError) as exc:
        parser.error(str(exc))
    print(f"Výsledky: {folder}")
    if interpreter is None:
        print("Bez interpretu: platné programy budou pouze PARTIAL.")
    results = []
    for c in selected:
        result = run_case(c, compiler, interpreter, args.timeout, folder / c.name)
        results.append(result)
        print(f"[{result['status']:7}] {c.name}: {result['reason']}", flush=True)
    counts = Counter(r["status"] for r in results)
    groups = defaultdict(Counter)
    for r in results:
        groups[r["group"]][r["status"]] += 1
    summary = [f"Celkem: {len(results)} | PASS: {counts['PASS']} | FAIL: {counts['FAIL']} | PARTIAL: {counts['PARTIAL']}"]
    for group, count in sorted(groups.items()):
        summary.append(f"{group:15} PASS={count['PASS']:2} FAIL={count['FAIL']:2} PARTIAL={count['PARTIAL']:2}")
    failed_phases = Counter(r["phase"] for r in results if r["status"] == "FAIL")
    if failed_phases:
        summary.append("Selhání podle fáze: " + ", ".join(f"{k}={v}" for k, v in sorted(failed_phases.items())))
    if counts["PARTIAL"]:
        summary.append("Běh části programů nebyl ověřen. PARTIAL není úspěšný end-to-end test.")
    summary.append("Jde o regresní sadu, nikoli důkaz úplné správnosti ani odhad bodového hodnocení.")
    text = "\n".join(summary) + "\n"
    print("\n" + text)
    (folder / "summary.txt").write_text(text, encoding="utf-8")
    (folder / "report.json").write_text(json.dumps({
        "compiler": compiler, "interpreter": interpreter,
        "timeout": args.timeout, "strict_leading_zeros": args.strict_leading_zeros,
        "counts": {s: counts[s] for s in ("PASS", "FAIL", "PARTIAL")},
        "results": results}, ensure_ascii=False, indent=2), encoding="utf-8")
    return 1 if counts["FAIL"] else (2 if counts["PARTIAL"] else 0)


if __name__ == "__main__":
    sys.exit(main())