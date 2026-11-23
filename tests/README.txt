TESTY IFJ26

Python 3.9+, make, gcc s podporou C23 a sanitizery.
Bez instalace balicku pres pip. Určeno pro WSL/Linux.

Vse:
make test INTERPRETER=./ic26int-linux-x86_64

S kontrolou pameti:
make test INTERPRETER=./ic26int-linux-x86_64 TEST_ARGS="--sanitize"

Jednotlive casti:
make test-helpers
make test-tables
make test-scanner
make test-expressions
make test-semantic
make test-parser
make test-semantic-programs
make test-codegen INTERPRETER=./ic26int-linux-x86_64
make test-integration INTERPRETER=./ic26int-linux-x86_64

Prime spusteni:
python3 tests/run_tests.py --suite all --interpreter ./ic26int-linux-x86_64

Vysledky: tests/results/run-*/report.json a soubory stdout/stderr.
Barvy: --color auto/always/never.
Navratovy kod: 0 uspech, 1 chyba testu, 2 nektere behove kontroly neovereny.
Chybejici interpret neni uspech; platne programy se oznaci NEOVERENO.
Generovani kodu kontroluje UBSan i bez --sanitize. Volba --sanitize prida
ASan/UBSan do vlastnich C driveru, nikoliv do frontendovych/expressions testu.

Skupiny:
helpers - kapacity, dynamicky buffer, kopie retezcu, vlastnictvi tokenu;
tables - kolize, smazane bunky, 10000 operaci proti modelu, rozsahy a parametry;
scanner - presne tokenove fixture, atributy a chybove zdrojove programy;
expressions - vytvorene stromy, priority operaci a typove chyby;
semantic - prime volani funkci semantickeho analyzatoru;
parser - syntakticke priklady se skutecnym generatorem nahrazenym stuby;
semantic_programs - zakladni i rozsahle generovane semanticke priklady;
codegen - prime volani generatoru bez parseru + interpretace;
integration - cele zdrojove programy, generovany kod, vstup a presny vystup.

Testy predpokladaji zakladni IFJ26. Pri zavedeni rozsireni upravte hlavne
negativni pripady, ktere zakladni jazyk zakazuje.
Sada neni dukaz uplne spravnosti ani odhad bodu.
Zdrojove testy verzujte; tests/results/ se neverzuje.

Instalator upravuje pouze testovaci cile Makefile a doplnuje .gitignore.
Produkční zdrojove soubory nemeni.
Puvodni testy a artefakty najdete v zaloze vedle projektu.

Testovaci vstup parseru pouziva parse(const char *, size_t).
