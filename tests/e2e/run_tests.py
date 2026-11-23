#!/usr/bin/env python3
# Project:    IFJ26
# Filename:   run_tests.py
#
# AI: file written with AI (Claude Code, Claude Opus 5.5)
# prompt: "vytvor testovaci skripty pro nas kod a znovu analyzuj, co kod
#          rozbiji, co a kde upravit"
#
# End-to-end tests: compile -> run -> compare return codes and exact stdout.
#
#   python3 tests/e2e/run_tests.py                     build src/ and test it
#   python3 tests/e2e/run_tests.py --src ../other/src  test another checkout
#   python3 tests/e2e/run_tests.py --group 13          only groups containing 13
#   python3 tests/e2e/run_tests.py --filter fact -v    details of chosen tests
#   python3 tests/e2e/run_tests.py --no-ext            only the base language
#   python3 tests/e2e/run_tests.py --interpreter ic26int   official interpreter
#   python3 tests/e2e/run_tests.py --robust 300        mutated inputs (crashes)
#
# Nothing is written into the repository, the build goes to a temp directory.
import argparse
import collections
import concurrent.futures
import glob
import json
import os
import random
import shlex
import shutil
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import cases as case_module  # noqa: E402
from ic26sim import simulate  # noqa: E402

VALID_COMPILER_CODES = {0, 1, 2, 3, 4, 5, 6, 7, 10}
TIMEOUT = 10


def build(src, out_dir):
    """Builds a plain and a sanitized compiler; returns their paths."""
    sources = sorted(glob.glob(os.path.join(src, "*.c")))
    if not sources:
        sys.exit(f"no C sources in {src}")
    compiler = os.environ.get("CC", "gcc")
    base = [compiler, "-std=c2x", "-D_POSIX_C_SOURCE=200809L", f"-I{src}", "-w"]
    plain, sanitized = os.path.join(out_dir, "ifj26"), os.path.join(out_dir, "ifj26_asan")
    result = subprocess.run(base + ["-O1", "-g", "-o", plain] + sources,
                            capture_output=True, text=True)
    if result.returncode != 0:
        sys.exit("build failed:\n" + result.stderr[-3000:])
    result = subprocess.run(base + ["-O0", "-g", "-fsanitize=address,undefined",
                                    "-fno-omit-frame-pointer", "-o", sanitized] + sources,
                            capture_output=True, text=True)
    return plain, sanitized if result.returncode == 0 else None


def run_process(argv, data, env=None):
    try:
        proc = subprocess.run(argv, input=data, capture_output=True, timeout=TIMEOUT, env=env)
        return proc.returncode, proc.stdout, proc.stderr
    except subprocess.TimeoutExpired:
        return "TIMEOUT", b"", b""


def sanitizer_problem(stderr):
    text = stderr.decode("latin-1")
    if "ERROR: AddressSanitizer" in text:
        kind = text.split("ERROR: AddressSanitizer: ", 1)[1].split()[0]
        where = [line for line in text.splitlines() if " in " in line and "/src/" in line]
        frame = where[0].split(" in ", 1)[1].split("/src/")[0].split()[0] if where else "?"
        path = where[0].rsplit("/src/", 1)[1] if where else "?"
        return f"ASAN {kind} in {frame} ({path})"
    if "runtime error:" in text:
        line = next(l for l in text.splitlines() if "runtime error:" in l)
        return "UBSAN " + line.rsplit("/src/", 1)[-1][:90]
    return ""


def run_case(case, plain, sanitized, interpreter):
    """Returns a result dict with status PASS / FAIL and a failure category."""
    result = {"name": case.name, "group": case.group, "status": "FAIL", "phase": "",
              "reason": "", "leak": False}
    source = case.source.encode("latin-1")
    rc, code, stderr = run_process([plain], source)
    expected = case.rc if isinstance(case.rc, tuple) else (case.rc,)
    if sanitized:
        env = dict(os.environ, ASAN_OPTIONS="detect_leaks=1:exitcode=99",
                   UBSAN_OPTIONS="print_stacktrace=0")
        _, _, san_err = run_process([sanitized], source, env)
        problem = sanitizer_problem(san_err)
        result["leak"] = b"LeakSanitizer" in san_err
        if problem:
            result.update(phase="memory", reason=problem)
            return result
    if rc == "TIMEOUT" or (isinstance(rc, int) and rc < 0):
        result.update(phase="crash", reason=f"compiler: {rc}")
        return result
    if rc not in expected:
        message = stderr.decode("latin-1").strip().splitlines()
        hint = f" [{message[-1][:60]}]" if message else ""
        result.update(phase=f"compile:{'/'.join(map(str, expected))}->{rc}",
                      reason=f"compiler returned {rc}, expected {case.rc}{hint}")
        return result
    if rc != 0:
        result["status"] = "PASS"
        return result
    run_rc, out, message = execute(code, case.stdin, interpreter)
    if run_rc != case.run_rc:
        result.update(phase=f"run_rc:{run_rc}",
                      reason=f"program returned {run_rc}, expected {case.run_rc}: {message[:90]}")
    elif out != case.out:
        result.update(phase="stdout",
                      reason=f"stdout {out[:70]!r}, expected {case.out[:70]!r}")
    else:
        result["status"] = "PASS"
    return result


def execute(code, stdin, interpreter):
    if interpreter is None:
        rc, out, message = simulate(code, stdin)
        return ("TIMEOUT" if rc == -1 else rc), out, message
    with tempfile.NamedTemporaryFile(suffix=".code") as handle:
        handle.write(code)
        handle.flush()
        rc, out, err = run_process(interpreter + [handle.name], stdin)
    return rc, out, err.decode("latin-1").strip()[:120]


def mutate(rng, source):
    tokens = ["(", ")", ",", "=", "==", "+", "-", "*", "/", "<", "\n", "  ", "\"", "\"\"\"",
              "/*", "*/", "//", "if ", "then", "else", "while ", "do", "def ", "val ", "var ",
              "return ", "@main", ":", ";", "Int", "Unit", "()", "0", "1.5e", "x", "f("]
    chars = list(source)
    for _ in range(rng.randint(1, 4)):
        if not chars:
            break
        position = rng.randrange(len(chars))
        action = rng.random()
        if action < .35:
            del chars[position:position + rng.randint(1, 6)]
        elif action < .75:
            chars[position:position] = list(rng.choice(tokens))
        else:
            other = rng.randrange(len(chars))
            chars[position], chars[other] = chars[other], chars[position]
    return "".join(chars)


def robustness(count, sanitized, seed):
    """Mutated programs must end with a valid code and without memory errors."""
    if sanitized is None:
        print("robustness: sanitized build is not available")
        return 0
    rng = random.Random(seed)
    pool = [c.source for c in case_module.CASES if c.rc == 0 and not c.group.startswith("fuzz")]
    env = dict(os.environ, ASAN_OPTIONS="detect_leaks=0:exitcode=98",
               UBSAN_OPTIONS="print_stacktrace=0")
    inputs = [mutate(rng, rng.choice(pool)) for _ in range(count)]

    def check(text):
        rc, _, err = run_process([sanitized], text.encode("latin-1"), env)
        problem = sanitizer_problem(err)
        if problem:
            return problem, text
        if rc == "TIMEOUT":
            return "compiler hang (timeout)", text
        if rc not in VALID_COMPILER_CODES:
            return f"invalid return code {rc}", text
        return None, text

    found = collections.OrderedDict()
    with concurrent.futures.ThreadPoolExecutor(os.cpu_count() or 2) as pool_exec:
        for problem, text in pool_exec.map(check, inputs):
            if problem:
                found.setdefault(problem, []).append(text)
    print(f"\nROBUSTNESS: {count} mutated inputs, {sum(map(len, found.values()))} problems")
    for problem, samples in found.items():
        shortest = min(samples, key=len)
        print(f"  {len(samples):4}x {problem}\n        shortest input ({len(shortest)} B): "
              f"{shortest[:160]!r}")
    return sum(map(len, found.values()))


def main():
    root = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    parser = argparse.ArgumentParser(description="IFJ26 end-to-end tests")
    parser.add_argument("--src", default=os.path.join(root, "src"))
    parser.add_argument("--compiler", help="use this binary instead of building --src")
    parser.add_argument("--interpreter", help="official interpreter, e.g. ./ic26int")
    parser.add_argument("--group", default="", help="substring of the group name")
    parser.add_argument("--filter", default="", help="substring of the test name")
    parser.add_argument("--no-ext", action="store_true", help="skip extension groups")
    parser.add_argument("--no-fuzz", action="store_true", help="skip random programs")
    parser.add_argument("--seed", type=int, default=26)
    parser.add_argument("--scale", type=int, default=1, help="multiplier of random programs")
    parser.add_argument("--robust", type=int, default=0, metavar="N", help="N mutated inputs")
    parser.add_argument("--json", help="write all results to this file")
    parser.add_argument("-v", "--verbose", action="store_true", help="list every failure")
    parser.add_argument("--list", action="store_true", help="only list tests")
    args = parser.parse_args()

    if not args.no_fuzz:
        case_module.add_fuzz(args.seed, args.scale)
    selected = [c for c in case_module.CASES
                if args.group in c.group and args.filter in c.name
                and not (args.no_ext and c.group.startswith("ext_"))]
    if args.list:
        for case in selected:
            print(f"{case.name:48} rc={case.rc} run={case.run_rc}")
        return 0
    if not selected:
        sys.exit("no test matches the filter")

    interpreter = shlex.split(args.interpreter) if args.interpreter else None
    if interpreter and shutil.which(interpreter[0]) is None:
        sys.exit(f"interpreter not found: {interpreter[0]}")
    temp = tempfile.mkdtemp(prefix="ifj26-e2e-")
    try:
        if args.compiler:
            plain, sanitized = os.path.abspath(args.compiler), None
        else:
            plain, sanitized = build(os.path.abspath(args.src), temp)
        print(f"compiler: {plain}\ninterpreter: {args.interpreter or 'ic26sim (not the official ic26int)'}")
        print(f"sanitizers: {'on' if sanitized else 'off'}\ntests: {len(selected)}")
        if interpreter:
            work = lambda c: run_case(c, plain, sanitized, interpreter)
            with concurrent.futures.ThreadPoolExecutor(os.cpu_count() or 2) as pool:
                results = list(pool.map(work, selected))
        else:
            with concurrent.futures.ProcessPoolExecutor(os.cpu_count() or 2) as pool:
                results = list(pool.map(run_case, selected, [plain] * len(selected),
                                        [sanitized] * len(selected), [None] * len(selected),
                                        chunksize=4))
        report(results, args.verbose)
        if args.json:
            with open(args.json, "w", encoding="utf-8") as handle:
                json.dump(results, handle, indent=1, ensure_ascii=False)
        failed = sum(r["status"] != "PASS" for r in results)
        if args.robust:
            failed += robustness(args.robust, sanitized, args.seed)
    finally:
        shutil.rmtree(temp, ignore_errors=True)
    return 1 if failed else 0


def report(results, verbose):
    groups = collections.OrderedDict()
    for result in sorted(results, key=lambda r: r["group"]):
        groups.setdefault(result["group"], []).append(result)
    print(f"\n{'group':22}{'pass':>6}{'total':>7}   failure phases")
    for group, items in groups.items():
        passed = sum(r["status"] == "PASS" for r in items)
        phases = collections.Counter(r["phase"] for r in items if r["status"] != "PASS")
        text = ", ".join(f"{k}x{v}" for k, v in phases.most_common(4))
        print(f"{group:22}{passed:6}{len(items):7}   {text}")
    base = [r for r in results if not r["group"].startswith("ext_")]
    print(f"\nBASE LANGUAGE: {sum(r['status'] == 'PASS' for r in base)} / {len(base)}")
    ext = [r for r in results if r["group"].startswith("ext_")]
    if ext:
        print(f"EXTENSIONS:    {sum(r['status'] == 'PASS' for r in ext)} / {len(ext)}")
    leaks = sum(r["leak"] for r in results)
    print(f"leaks reported in {leaks} / {len(results)} compiler runs")
    if verbose:
        print("\nFAILURES:")
        for result in results:
            if result["status"] != "PASS":
                print(f"  {result['name']}: {result['reason']}")


if __name__ == "__main__":
    sys.exit(main())
