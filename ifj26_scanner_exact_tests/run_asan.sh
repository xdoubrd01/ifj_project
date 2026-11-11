#!/bin/sh
set -u

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
SRC_DIR="${1:-$ROOT/../src}"
BUILD_DIR="$ROOT/.build"
BIN="$BUILD_DIR/scanner_asan_test"

GREEN='\033[0;32m'
RED='\033[0;31m'
RESET='\033[0m'
CC="${CC:-gcc}"

mkdir -p "$BUILD_DIR"

"$CC" -std=c11 -Wall -Wextra -Werror -pedantic -g \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -I"$SRC_DIR" \
    "$SRC_DIR/scanner.c" "$ROOT/scanner_test_driver.c" \
    -o "$BIN" || exit 1

failed=0
for input in "$ROOT"/cases/*/*.ifj26; do
    [ -f "$input" ] || continue
    expected_rc=$(cat "${input%.ifj26}.rc")

    ASAN_OPTIONS=detect_leaks=1 "$BIN" "$input" >/dev/null 2>"$BUILD_DIR/asan.err"
    rc=$?

    # Lexical-error tests are expected to return 1; sanitizer failures usually
    # return another code and write an ASan/UBSan diagnostic.
    if grep -Eq 'AddressSanitizer|UndefinedBehaviorSanitizer|runtime error:|LeakSanitizer' "$BUILD_DIR/asan.err"; then
        printf "${RED}SANITIZER FAIL${RESET} %s\n" "${input#"$ROOT"/cases/}"
        cat "$BUILD_DIR/asan.err"
        failed=$((failed + 1))
    elif [ "$rc" -ne "$expected_rc" ]; then
        printf "${RED}RC FAIL${RESET}        %s (expected %s, got %s)\n" \
            "${input#"$ROOT"/cases/}" "$expected_rc" "$rc"
        failed=$((failed + 1))
    else
        printf "${GREEN}PASS${RESET}           %s\n" "${input#"$ROOT"/cases/}"
    fi
done

exit "$failed"
