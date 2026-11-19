#!/usr/bin/env bash
#
# AI-generated code (ChatGPT).
# Prompt: "udelej mi do jednoho souboru automaticke testy pro muj IFJ26
# parser v jazyce C. Otestuj syntaxi, dvoupruchodove zpracovani funkci,
# volani funkci, pocty a typy argumentu i chybove navratove kody.
# U kazdeho testu vypis, jestli prosel, a shrn vysledky."
#
# Testy předpokládají, že překladač čte program ze stdin a vrací:
#   0  - úspěch
#   2  - syntaktická chyba
#   3  - nedefinovaná funkce / symbol dle dané části implementace
#   4  - duplicitní definice funkce
#   5  - nesprávný počet nebo typ argumentů funkce
# Některé případy mohou odhalit dosud neimplementované části parseru.
# Testy nejsou potvrzením úplné shody s oficiálním zadáním IFJ26.
#
# Spuštění z kořene projektu: bash test_parser.sh
# Volitelně lze předat cestu k překladači: bash test_parser.sh ./ifj

set -u

COMPILER="${1:-./ifj}"
if [[ ! -x "$COMPILER" ]]; then
    printf 'Chyba: překladač "%s" neexistuje nebo není spustitelný.\n' "$COMPILER" >&2
    printf 'Nejdříve jej přelož (například pomocí gcc).\n' >&2
    exit 1
fi

TMPDIR_TEST="$(mktemp -d)" || exit 1
trap 'rm -rf "$TMPDIR_TEST"' EXIT

passed=0
failed=0
total=0

run_test() {
    local name="$1"
    local expected="$2"
    local source="$3"
    local actual

    total=$((total + 1))
    printf '%s\n' "$source" > "$TMPDIR_TEST/input.ifj"

    "$COMPILER" < "$TMPDIR_TEST/input.ifj" > "$TMPDIR_TEST/stdout" 2> "$TMPDIR_TEST/stderr"
    actual=$?

    if [[ "$actual" -eq "$expected" ]]; then
        passed=$((passed + 1))
        printf '[OK]   %-38s očekáváno=%d, skutečně=%d\n' "$name" "$expected" "$actual"
    else
        failed=$((failed + 1))
        printf '[FAIL] %-38s očekáváno=%d, skutečně=%d\n' "$name" "$expected" "$actual"
        printf '       Vstup:\n'
        sed 's/^/       | /' "$TMPDIR_TEST/input.ifj"
        if [[ -s "$TMPDIR_TEST/stderr" ]]; then
            printf '       stderr:\n'
            sed 's/^/       | /' "$TMPDIR_TEST/stderr"
        fi
        if [[ -s "$TMPDIR_TEST/stdout" ]]; then
            printf '       stdout:\n'
            sed 's/^/       | /' "$TMPDIR_TEST/stdout"
        fi
    fi
}

# Základní syntaktické testy
run_test 'Jednoduchá funkce' 0 'import ifj26.*
def foo(): Int = 10'

run_test 'Dvě funkce' 0 'import ifj26.*
def foo(): Int = 10
def bar(): Int = 20'

run_test 'Chybějící import' 2 'def foo(): Int = 10'

run_test 'Chybějící závorka v definici' 2 'import ifj26.*
def foo(: Int = 10'

# Dva průchody: volaná funkce se může definovat až níže
run_test 'Volání později definované funkce' 0 'import ifj26.*
def foo(): Int = bar()
def bar(): Int = 10'

run_test 'Volání nedefinované funkce' 3 'import ifj26.*
def foo(): Int = missing()'

run_test 'Duplicitní definice funkce' 4 'import ifj26.*
def foo(): Int = 1
def foo(): Int = 2'

# Argumenty funkcí
run_test 'Dva správné argumenty' 0 'import ifj26.*
def foo(): Int = add(10, 20)
def add(a: Int, b: Int): Int = a + b'

run_test 'Volání bez argumentů' 0 'import ifj26.*
def foo(): Int = bar()
def bar(): Int = 42'

run_test 'Příliš málo argumentů' 5 'import ifj26.*
def foo(): Int = add(10)
def add(a: Int, b: Int): Int = a + b'

run_test 'Příliš mnoho argumentů' 5 'import ifj26.*
def foo(): Int = add(10, 20, 30)
def add(a: Int, b: Int): Int = a + b'

run_test 'Nesprávný typ prvního argumentu' 5 'import ifj26.*
def foo(): Int = add("hello", 20)
def add(a: Int, b: Int): Int = a + b'

run_test 'Nesprávný typ druhého argumentu' 5 'import ifj26.*
def foo(): Int = add(10, "hello")
def add(a: Int, b: Int): Int = a + b'

run_test 'Nula místo jednoho argumentu' 5 'import ifj26.*
def foo(): Int = identity()
def identity(a: Int): Int = a'

printf '\nVýsledky: %d/%d prošlo, %d selhalo.\n' "$passed" "$total" "$failed"
if [[ "$failed" -ne 0 ]]; then
    exit 1
fi
exit 0
