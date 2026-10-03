#!/bin/sh
set -u

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
SRC_DIR="${1:-$ROOT/../src}"
BUILD_DIR="$ROOT/.build"
BIN="$BUILD_DIR/scanner_exact_test"

GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[0;33m'
CYAN='\033[0;36m'
RESET='\033[0m'

CC="${CC:-gcc}"

mkdir -p "$BUILD_DIR"

printf "${CYAN}Compiling scanner test driver...${RESET}\n"
if ! "$CC" -std=c11 -Wall -Wextra -Werror -pedantic \
    -I"$SRC_DIR" \
    "$SRC_DIR/scanner.c" "$ROOT/scanner_test_driver.c" \
    -o "$BIN"; then
    printf "${RED}Compilation failed.${RESET}\n"
    exit 1
fi

pass=0
fail=0
total=0

for input in "$ROOT"/cases/*/*.ifj26; do
    [ -f "$input" ] || continue

    base="${input%.ifj26}"
    expected="$base.expected"
    rc_file="$base.rc"
    name="${input#"$ROOT"/cases/}"

    if [ ! -f "$expected" ] || [ ! -f "$rc_file" ]; then
        printf "${RED}BROKEN TEST %-45s missing .expected or .rc${RESET}\n" "$name"
        fail=$((fail + 1))
        continue
    fi

    expected_rc=$(cat "$rc_file")
    actual="$BUILD_DIR/actual.out"
    diff_file="$BUILD_DIR/diff.out"

    "$BIN" "$input" > "$actual" 2> "$BUILD_DIR/stderr.out"
    actual_rc=$?

    total=$((total + 1))

    rc_ok=0
    out_ok=0

    if [ "$actual_rc" -eq "$expected_rc" ]; then
        rc_ok=1
    fi

    if diff -u "$expected" "$actual" > "$diff_file"; then
        out_ok=1
    fi

    if [ "$rc_ok" -eq 1 ] && [ "$out_ok" -eq 1 ]; then
        printf "${GREEN}PASS${RESET}  %-45s\n" "$name"
        pass=$((pass + 1))
    else
        printf "${RED}FAIL${RESET}  %-45s" "$name"
        if [ "$rc_ok" -ne 1 ]; then
            printf " ${YELLOW}[rc expected=%s got=%s]${RESET}" "$expected_rc" "$actual_rc"
        fi
        printf "\n"

        if [ "$out_ok" -ne 1 ]; then
            printf "${YELLOW}Token stream differs:${RESET}\n"
            cat "$diff_file"
        fi

        if [ -s "$BUILD_DIR/stderr.out" ]; then
            printf "${YELLOW}stderr:${RESET}\n"
            cat "$BUILD_DIR/stderr.out"
        fi

        fail=$((fail + 1))
    fi
done

printf "\n"
if [ "$fail" -eq 0 ]; then
    printf "${GREEN}Result: %d/%d passed, 0 failed.${RESET}\n" "$pass" "$total"
else
    printf "${RED}Result: %d/%d passed, %d failed.${RESET}\n" "$pass" "$total" "$fail"
fi

exit "$fail"
