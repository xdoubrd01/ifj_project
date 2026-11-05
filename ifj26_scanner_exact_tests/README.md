# IFJ26 – komplexní testy scanneru

Tato sada testuje **pouze lexikální analyzátor (`scanner.c` / `scanner.h`)**.
Netestuje parser, sémantiku ani správnost celého programu IFJ26.

Testovací driver volá přímo veřejné API:

- `scanner_init(FILE *)`
- `get_next_token(Token *)`
- `scanner_free()`

a u každého vstupu porovnává:

1. návratový kód scanneru,
2. **přesný typ každého tokenu**,
3. atribut `s_val` u identifikátorů a řetězců,
4. atribut `i_val` u `Int`,
5. atribut `d_val` u `Double`,
6. pořadí `EOL`, `INDENT`, `DEDENT`, `EOF`.

Proto test neprojde jen proto, že scanner vrátí `0`; musí souhlasit celý token stream.

## Umístění

Doporučená struktura projektu:

```text
ifj_project/
├── src/
│   ├── scanner.c
│   └── scanner.h
└── ifj26_scanner_exact_tests/
    ├── cases/
    ├── scanner_test_driver.c
    ├── run_tests.sh
    └── run_asan.sh
```

## Spuštění

Pokud je testovací adresář vedle `src/`:

```bash
cd ~/ifj_project/ifj26_scanner_exact_tests
chmod +x run_tests.sh run_asan.sh
./run_tests.sh
```

Cestu ke zdrojovým souborům lze zadat i explicitně:

```bash
./run_tests.sh ../src
```

Skript si sám přeloží `scanner.c` spolu s vlastním testovacím driverem.
Nepoužívá váš `main.c`, takže výsledek není ovlivněn testovacím mainem parseru.

## Barvy a FAIL

- `PASS` je zeleně.
- `FAIL` je červeně.
- pokud nesedí návratový kód, vypíše očekávanou a skutečnou hodnotu;
- pokud nesedí tokeny, vypíše `diff` očekávaného a skutečného token streamu.

Příklad:

```text
FAIL  lexical_errors/double_suffix_followed_digit.ifj26 [rc expected=1 got=0]
Token stream differs:
...
```

## Sanitizery

Pro kontrolu pádu programu, přístupu mimo paměť, UB a leaků:

```bash
./run_asan.sh
```

nebo:

```bash
./run_asan.sh ../src
```

## Struktura jednoho testu

Každý test má tři soubory:

```text
nazev.ifj26
nazev.expected
nazev.rc
```

- `.ifj26` – vstup scanneru,
- `.expected` – přesně očekávané tokeny,
- `.rc` – očekávaný návratový kód (`0` nebo `1`).

Například pro:

```text
val x = 42
```

může očekávání obsahovat:

```text
TOKEN_KW_VAL
TOKEN_IDENTIFIER("x")
TOKEN_ASSIGN
TOKEN_INT_LITERAL(42)
TOKEN_EOL
TOKEN_EOF
```

## Co sada pokrývá

Celkem obsahuje **82 testů**:

- 11 testů základních tokenů, klíčových slov, identifikátorů a oddělovačů,
- 4 testy validních číselných literálů,
- 6 testů řetězců,
- 7 testů komentářů,
- 5 testů odsazení,
- 3 testy víceřádkových výrazů v závorkách,
- 4 testy whitespace/EOL/EOF,
- 42 samostatných lexikálních chyb.

Podrobnosti jsou v `TEST_MATRIX.md`.

## Důležitá hranice scanner/parser

Některé vstupy jsou syntakticky chybné, ale **lexikálně platné**.
Tyto testy jsou záměrné: ověřují, že scanner nehlásí chybu, kterou má řešit parser.

Například:

```text
(1 + 2
```

na stejném fyzickém řádku je pro scanner pouze posloupnost tokenů
`LPAREN INT PLUS INT EOF`. Chybějící `)` je až problém parseru.

## Rozšíření jazyka

Sada je zaměřená na tokeny, které existují ve vašem současném `scanner.h`.
Rozšiřující klíčová slova jako `Boolean`, `case`, `class`, `extension`, `for`,
`List`, `match`, `type` nemají v aktuálním `TokenType` vlastní tokeny, proto zde
není vynuceno jejich mapování. Pokud rozšíříte `scanner.h`, je vhodné k nim doplnit
samostatné testy.

## AI označení

Testovací soubory a driver byly vytvořeny s pomocí AI. V každém `.ifj26` vstupu
je proto komentář s označením AI-generovaného testu a promptem. Driver obsahuje
stejné označení v hlavičce.
