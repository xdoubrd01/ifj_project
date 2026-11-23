#!/usr/bin/env python3
# Project:    IFJ26
# Filename:   ic26sim.py
#
# AI: file written with AI (Claude Code, Claude Opus 5.5)
# prompt: "vytvor testovaci skripty pro nas kod a znovu analyzuj, co kod
#          rozbiji" - interpret ic26int nebyl k dispozici, proto simulator
#
# Simulator of IFJcode26 written from section 7 of the specification.
# It is NOT the official ic26int. Use the official one for the final check:
#     python3 tests/e2e/run_tests.py --interpreter /path/to/ic26int
#
# Usage: ic26sim.py program.code < input > output
import re
import sys

NAME = r"[A-Za-z_\-$&%*!?][A-Za-z0-9_\-$&%*!?]*"
VAR_RE = re.compile(r"(GF|LF|TF)@(" + NAME + r")\Z")
LABEL_RE = re.compile(NAME + r"\Z")
INT_RE = re.compile(r"[+-]?[0-9]+\Z")
STRING_RE = re.compile(r"(?:[^\s#\\]|\\[0-9]{3})*\Z")
READ_FLOAT_RE = re.compile(
    r"[+-]?(?:0[xX](?:[0-9a-fA-F]+\.?[0-9a-fA-F]*|\.[0-9a-fA-F]+)"
    r"(?:[pP][+-]?[0-9]+)?|(?:[0-9]+\.?[0-9]*|\.[0-9]+)(?:[eE][+-]?[0-9]+)?)\Z")
INT64_MIN, INT64_MAX = -(2**63), 2**63 - 1
DEFAULT_STEP_LIMIT = 20_000_000


class SimError(Exception):
    """Interpreter error with the return code from specification 7.1."""

    def __init__(self, code, message):
        super().__init__(message)
        self.code = code


class StepLimit(Exception):
    """The program executed too many instructions (probably a hang)."""


class Unset:
    """Value of a variable or array item that was not initialized."""


UNSET = Unset()


class Array:
    def __init__(self, size):
        self.items = [UNSET] * size


class Var:
    __slots__ = ("frame", "name")

    def __init__(self, frame, name):
        self.frame, self.name = frame, name


class Const:
    __slots__ = ("value",)

    def __init__(self, value):
        self.value = value


class Label:
    __slots__ = ("name",)

    def __init__(self, name):
        self.name = name


# operand kinds: v=variable s=symbol l=label L=label literal t=type
OPS = {
    "MOVE": "vs", "CREATEFRAME": "", "PUSHFRAME": "", "POPFRAME": "",
    "DEFVAR": "v", "CALL": "l", "RETURN": "", "PUSHS": "s", "POPS": "v",
    "CLEARS": "", "ADD": "vss", "SUB": "vss", "MUL": "vss", "DIV": "vss",
    "IDIV": "vss", "ADDS": "", "SUBS": "", "MULS": "", "DIVS": "", "IDIVS": "",
    "LT": "vss", "GT": "vss", "EQ": "vss", "LTS": "", "GTS": "", "EQS": "",
    "AND": "vss", "OR": "vss", "NOT": "vs", "ANDS": "", "ORS": "", "NOTS": "",
    "INT2FLOAT": "vs", "FLOAT2INT": "vs", "INT2CHAR": "vs", "STRI2INT": "vss",
    "FLOAT2STR": "vs", "INT2STR": "vs", "INT2FLOATS": "", "FLOAT2INTS": "",
    "INT2CHARS": "", "STRI2INTS": "", "FLOAT2STRS": "", "INT2STRS": "",
    "READ": "vt", "WRITE": "s", "CONCAT": "vss", "STRLEN": "vs",
    "GETCHAR": "vss", "SETCHAR": "vss", "NEWARRAY": "vs", "ARRAYGET": "vvs",
    "ARRAYSET": "vss", "TYPE": "vs", "ISINT": "vs", "TYPES": "", "ISINTS": "",
    "LABEL": "L", "JUMP": "l", "JUMPIFEQ": "lss", "JUMPIFNEQ": "lss",
    "JUMPIFEQS": "l", "JUMPIFNEQS": "l", "EXIT": "s", "BREAK": "",
    "DPRINT": "s",
}


def decode_string(text):
    return re.sub(r"\\([0-9]{3})", lambda m: chr(int(m.group(1))), text)


def parse_symbol(text):
    match = VAR_RE.match(text)
    if match:
        return Var(match.group(1), match.group(2))
    if "@" not in text:
        raise SimError(51, f"bad operand: {text}")
    kind, value = text.split("@", 1)
    if kind == "int" and INT_RE.match(value):
        number = int(value)
        if INT64_MIN <= number <= INT64_MAX:
            return Const(number)
    elif kind == "bool" and value in ("true", "false"):
        return Const(value == "true")
    elif kind == "nil" and value == "nil":
        return Const(None)
    elif kind == "string" and STRING_RE.match(value):
        return Const(decode_string(value))
    elif kind == "float":
        try:
            return Const(float.fromhex(value))
        except ValueError:
            pass
    raise SimError(51, f"bad constant: {text}")


def parse_operand(kind, text):
    if kind == "v":
        match = VAR_RE.match(text)
        if not match:
            raise SimError(51, f"bad variable: {text}")
        return Var(match.group(1), match.group(2))
    if kind == "s":
        return parse_symbol(text)
    if kind == "L":
        if not LABEL_RE.match(text):
            raise SimError(51, f"bad label: {text}")
        return Label(text)
    if kind == "l":
        if LABEL_RE.match(text):
            return Label(text)
        return parse_symbol(text)
    if text not in ("int", "float", "string", "bool"):
        raise SimError(51, f"bad type: {text}")
    return text


def load(code):
    """Parses the program; returns the instruction list and the label map."""
    lines = code.split("\n")
    index = 0
    while index < len(lines) and not lines[index].split("#", 1)[0].strip():
        index += 1
    if index == len(lines) or \
            lines[index].split("#", 1)[0].strip().lower() != ".ifjcode26":
        raise SimError(51, "missing .IFJcode26 header")
    program, labels = [], {}
    for number, raw in enumerate(lines[index + 1:], index + 2):
        parts = raw.split("#", 1)[0].split()
        if not parts:
            continue
        opcode = parts[0].upper()
        if opcode not in OPS:
            raise SimError(51, f"line {number}: unknown instruction {parts[0]}")
        kinds = OPS[opcode]
        if len(parts) - 1 != len(kinds):
            raise SimError(51, f"line {number}: bad operand count: {raw}")
        args = [parse_operand(k, t) for k, t in zip(kinds, parts[1:])]
        if opcode == "LABEL":
            if args[0].name in labels:
                raise SimError(52, f"line {number}: label redefined: {parts[1]}")
            labels[args[0].name] = len(program)
        program.append((opcode, args, number))
    for opcode, args, number in program:
        if opcode != "LABEL" and args and isinstance(args[0], Label) \
                and args[0].name not in labels:
            raise SimError(52, f"line {number}: unknown label {args[0].name}")
    return program, labels


def c_hex_float(value):
    """Formats a double the same way as C printf("%a")."""
    if value != value:
        return "nan"
    if value in (float("inf"), float("-inf")):
        return "inf" if value > 0 else "-inf"
    match = re.match(r"(-?)0x([01])\.([0-9a-f]*)p([+-][0-9]+)", value.hex())
    fraction = match.group(3).rstrip("0")
    dot = "." + fraction if fraction else ""
    return f"{match.group(1)}0x{match.group(2)}{dot}p{match.group(4)}"


def wrap64(value):
    return (value - INT64_MIN) % 2**64 + INT64_MIN


def need(value, *types):
    if type(value) not in types:
        raise SimError(53, f"bad operand type: {type(value).__name__}")
    return value


def numeric_pair(a, b):
    if type(a) is not type(b) or type(a) not in (int, float):
        raise SimError(53, "arithmetic needs two int or two float operands")


def arith(op, a, b):
    numeric_pair(a, b)
    result = a + b if op == "ADD" else a - b if op == "SUB" else a * b
    return wrap64(result) if type(a) is int else result


def divide(a, b):
    need(a, float), need(b, float)
    if b == 0.0:
        raise SimError(57, "division by zero")
    return a / b


def idivide(a, b):
    need(a, int), need(b, int)
    if b == 0:
        raise SimError(57, "division by zero")
    quotient = abs(a) // abs(b)
    return wrap64(quotient if (a < 0) == (b < 0) else -quotient)


def compare(op, a, b):
    if a is None or b is None or type(a) is not type(b) or \
            type(a) not in (int, bool, float, str):
        raise SimError(53, f"{op}: incompatible operands")
    return a < b if op == "LT" else a > b


def equal(a, b):
    if a is None or b is None:
        return a is b
    if type(a) is not type(b):
        raise SimError(53, "EQ: operands of different types")
    return a is b if type(a) is Array else a == b


def float_to_int(value):
    need(value, float)
    if value != value or value in (float("inf"), float("-inf")):
        raise SimError(57, "FLOAT2INT: value out of range")
    return int(value)


def int_to_char(value):
    need(value, int)
    if not 0 <= value <= 127:
        raise SimError(58, "INT2CHAR: value out of range")
    return chr(value)


def str_to_int(text, index):
    need(text, str), need(index, int)
    if not 0 <= index < len(text):
        raise SimError(58, "STRI2INT: index out of range")
    return ord(text[index])


def type_name(value):
    if value is UNSET:
        return ""
    if value is None:
        return "nil"
    return {int: "int", bool: "bool", float: "float", str: "string",
            Array: "array"}[type(value)]


class Machine:
    def __init__(self, program, labels, stdin, limit):
        self.program, self.labels = program, labels
        self.stdin, self.pos = stdin, 0
        self.limit = limit
        self.gf, self.tf, self.frames = {}, None, []
        self.stack, self.calls = [], []
        self.out = []

    def frame(self, name):
        if name == "GF":
            return self.gf
        if name == "LF":
            if not self.frames:
                raise SimError(55, "LF does not exist")
            return self.frames[-1]
        if self.tf is None:
            raise SimError(55, "TF does not exist")
        return self.tf

    def read(self, symbol, allow_unset=False):
        if symbol.__class__ is Const:
            return symbol.value
        frame = self.frame(symbol.frame)
        if symbol.name not in frame:
            raise SimError(54, f"variable {symbol.frame}@{symbol.name} missing")
        value = frame[symbol.name]
        if value is UNSET and not allow_unset:
            raise SimError(56, f"variable {symbol.frame}@{symbol.name} not set")
        return value

    def write_var(self, var, value):
        frame = self.frame(var.frame)
        if var.name not in frame:
            raise SimError(54, f"variable {var.frame}@{var.name} missing")
        frame[var.name] = value

    def pop(self):
        if not self.stack:
            raise SimError(56, "data stack is empty")
        return self.stack.pop()

    def target(self, operand):
        if operand.__class__ is Label:
            name = operand.name
        else:
            name = need(self.read(operand), str)
        if name not in self.labels:
            raise SimError(52, f"unknown label {name}")
        return self.labels[name]

    def read_line(self):
        end = self.stdin.find(b"\n", self.pos)
        if self.pos >= len(self.stdin):
            return None
        stop = len(self.stdin) if end < 0 else end + 1
        line = self.stdin[self.pos:stop]
        self.pos = stop
        return line.decode("latin-1")

    def do_read(self, kind):
        line = self.read_line()
        if line is None:
            return None
        if kind == "string":
            return line
        text = line[:-1] if line.endswith("\n") else line
        if kind == "int":
            if INT_RE.match(text) and INT64_MIN <= int(text) <= INT64_MAX:
                return int(text)
            return None
        if kind == "float":
            if not READ_FLOAT_RE.match(text):
                return None
            lower = text.lower()
            return float.fromhex(text) if "0x" in lower else float(text)
        lowered = text.lower()
        return lowered == "true" if lowered in ("true", "false") else None

    def format_value(self, value):
        if value is None:
            return "nil"
        if type(value) is bool:
            return "true" if value else "false"
        if type(value) is int:
            return str(value)
        if type(value) is float:
            return c_hex_float(value)
        if type(value) is str:
            return value
        raise SimError(53, "WRITE: array operand")

    def run(self):
        pc, steps = 0, 0
        program = self.program
        size = len(program)
        while pc < size:
            steps += 1
            if steps > self.limit:
                raise StepLimit()
            opcode, args, _ = program[pc]
            pc += 1
            jump = self.execute(opcode, args, pc)
            if jump is not None:
                if jump < 0:
                    return -jump - 1
                pc = jump
        return 0

    def execute(self, op, a, pc):
        read, stack = self.read, self.stack
        if op == "LABEL":
            return None
        if op == "PUSHS":
            stack.append(read(a[0]))
        elif op == "POPS":
            self.write_var(a[0], self.pop())
        elif op == "MOVE":
            self.write_var(a[0], read(a[1]))
        elif op == "DEFVAR":
            frame = self.frame(a[0].frame)
            if a[0].name in frame:
                raise SimError(52, f"variable {a[0].name} redefined")
            frame[a[0].name] = UNSET
        elif op == "CREATEFRAME":
            self.tf = {}
        elif op == "PUSHFRAME":
            if self.tf is None:
                raise SimError(55, "TF does not exist")
            self.frames.append(self.tf)
            self.tf = None
        elif op == "POPFRAME":
            if not self.frames:
                raise SimError(55, "LF does not exist")
            self.tf = self.frames.pop()
        elif op == "CALL":
            target = self.target(a[0])
            self.calls.append(pc)
            return target
        elif op == "RETURN":
            if not self.calls:
                raise SimError(56, "call stack is empty")
            return self.calls.pop()
        elif op == "CLEARS":
            stack.clear()
        elif op in ("ADD", "SUB", "MUL"):
            self.write_var(a[0], arith(op, read(a[1]), read(a[2])))
        elif op in ("ADDS", "SUBS", "MULS"):
            right, left = self.pop(), self.pop()
            stack.append(arith(op[:-1], left, right))
        elif op == "DIV":
            self.write_var(a[0], divide(read(a[1]), read(a[2])))
        elif op == "DIVS":
            right, left = self.pop(), self.pop()
            stack.append(divide(left, right))
        elif op == "IDIV":
            self.write_var(a[0], idivide(read(a[1]), read(a[2])))
        elif op == "IDIVS":
            right, left = self.pop(), self.pop()
            stack.append(idivide(left, right))
        elif op in ("LT", "GT"):
            self.write_var(a[0], compare(op, read(a[1]), read(a[2])))
        elif op in ("LTS", "GTS"):
            right, left = self.pop(), self.pop()
            stack.append(compare(op[:-1], left, right))
        elif op == "EQ":
            self.write_var(a[0], equal(read(a[1]), read(a[2])))
        elif op == "EQS":
            right, left = self.pop(), self.pop()
            stack.append(equal(left, right))
        elif op in ("AND", "OR"):
            x, y = need(read(a[1]), bool), need(read(a[2]), bool)
            self.write_var(a[0], (x and y) if op == "AND" else (x or y))
        elif op in ("ANDS", "ORS"):
            y, x = need(self.pop(), bool), need(self.pop(), bool)
            stack.append((x and y) if op == "ANDS" else (x or y))
        elif op == "NOT":
            self.write_var(a[0], not need(read(a[1]), bool))
        elif op == "NOTS":
            stack.append(not need(self.pop(), bool))
        elif op == "INT2FLOAT":
            self.write_var(a[0], float(need(read(a[1]), int)))
        elif op == "INT2FLOATS":
            stack.append(float(need(self.pop(), int)))
        elif op == "FLOAT2INT":
            self.write_var(a[0], float_to_int(read(a[1])))
        elif op == "FLOAT2INTS":
            stack.append(float_to_int(self.pop()))
        elif op == "INT2CHAR":
            self.write_var(a[0], int_to_char(read(a[1])))
        elif op == "INT2CHARS":
            stack.append(int_to_char(self.pop()))
        elif op == "STRI2INT":
            self.write_var(a[0], str_to_int(read(a[1]), read(a[2])))
        elif op == "STRI2INTS":
            index, text = self.pop(), self.pop()
            stack.append(str_to_int(text, index))
        elif op == "FLOAT2STR":
            self.write_var(a[0], "%.2f" % need(read(a[1]), float))
        elif op == "FLOAT2STRS":
            stack.append("%.2f" % need(self.pop(), float))
        elif op == "INT2STR":
            self.write_var(a[0], str(need(read(a[1]), int)))
        elif op == "INT2STRS":
            stack.append(str(need(self.pop(), int)))
        elif op == "READ":
            self.write_var(a[0], self.do_read(a[1]))
        elif op == "WRITE":
            self.out.append(self.format_value(read(a[0])))
        elif op == "CONCAT":
            self.write_var(a[0], need(read(a[1]), str) + need(read(a[2]), str))
        elif op == "STRLEN":
            self.write_var(a[0], len(need(read(a[1]), str)))
        elif op == "GETCHAR":
            text, index = need(read(a[1]), str), need(read(a[2]), int)
            if not 0 <= index < len(text):
                raise SimError(58, "GETCHAR: index out of range")
            self.write_var(a[0], text[index])
        elif op == "SETCHAR":
            text = need(read(a[0]), str)
            index, char = need(read(a[1]), int), need(read(a[2]), str)
            if not 0 <= index < len(text) or not char:
                raise SimError(58, "SETCHAR: bad index or empty string")
            self.write_var(a[0], text[:index] + char[0] + text[index + 1:])
        elif op == "NEWARRAY":
            size = need(read(a[1]), int)
            if size <= 0:
                raise SimError(57, "NEWARRAY: size must be positive")
            self.write_var(a[0], Array(size))
        elif op == "ARRAYGET":
            array, index = need(read(a[1]), Array), need(read(a[2]), int)
            if not 0 <= index < len(array.items):
                raise SimError(58, "ARRAYGET: index out of range")
            if array.items[index] is UNSET:
                raise SimError(56, "ARRAYGET: item not set")
            self.write_var(a[0], array.items[index])
        elif op == "ARRAYSET":
            array, index = need(read(a[0]), Array), need(read(a[1]), int)
            if not 0 <= index < len(array.items):
                raise SimError(58, "ARRAYSET: index out of range")
            array.items[index] = read(a[2])
        elif op == "TYPE":
            self.write_var(a[0], type_name(read(a[1], allow_unset=True)))
        elif op == "TYPES":
            stack.append(type_name(self.pop()))
        elif op == "ISINT":
            value = need(read(a[1]), int, float)
            self.write_var(a[0], type(value) is int or value == int(value))
        elif op == "ISINTS":
            value = need(self.pop(), int, float)
            stack.append(type(value) is int or value == int(value))
        elif op == "JUMP":
            return self.target(a[0])
        elif op in ("JUMPIFEQ", "JUMPIFNEQ"):
            target = self.target(a[0])
            same = equal(read(a[1]), read(a[2]))
            if same == (op == "JUMPIFEQ"):
                return target
        elif op in ("JUMPIFEQS", "JUMPIFNEQS"):
            target = self.target(a[0])
            right, left = self.pop(), self.pop()
            if equal(left, right) == (op == "JUMPIFEQS"):
                return target
        elif op == "EXIT":
            code = need(read(a[0]), int)
            if not 0 <= code <= 49:
                raise SimError(57, "EXIT: value out of range")
            return -code - 1
        return None


def simulate(code, stdin=b"", limit=DEFAULT_STEP_LIMIT):
    """Runs the program; returns (return code, stdout bytes, error message)."""
    try:
        text = code.decode("latin-1") if isinstance(code, bytes) else code
        program, labels = load(text)
    except SimError as error:
        return error.code, b"", str(error)
    machine = Machine(program, labels, stdin, limit)
    try:
        rc, message = machine.run(), ""
    except SimError as error:
        rc, message = error.code, str(error)
    except StepLimit:
        rc, message = -1, f"step limit {limit} exceeded (hang?)"
    return rc, "".join(machine.out).encode("latin-1"), message


def main():
    if len(sys.argv) != 2:
        sys.stderr.write("usage: ic26sim.py program.code < input\n")
        return 50
    try:
        with open(sys.argv[1], "rb") as handle:
            code = handle.read()
    except OSError as error:
        sys.stderr.write(f"{error}\n")
        return 60
    rc, out, message = simulate(code, sys.stdin.buffer.read())
    sys.stdout.buffer.write(out)
    if message:
        sys.stderr.write(message + "\n")
    return 60 if rc < 0 else rc


if __name__ == "__main__":
    sys.exit(main())
