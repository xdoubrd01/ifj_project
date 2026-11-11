# Nálezy při spuštění proti aktuální opravené verzi scanneru

Tuto sadu jsem zkušebně spustil proti verzi `scanner.c`, kterou jsem předtím připravil.
Výsledek byl:

```text
77/82 passed, 5 failed
```

Těchto pět FAILů odpovídá čtyřem odlišným problémům.

## 1. Prázdný běžný string způsobí segmentation fault

Test:

```text
strings/empty_ordinary.ifj26
```

Vstup obsahuje `""`.

Příčina je v pomocné funkci nastavující `s_val`: při prázdném bufferu je
`buffer.text == NULL`, ale kopíruje se z něj jeden bajt (`length + 1`).

## 2. Prázdný multiline string způsobí stejný segmentation fault

Test:

```text
strings/empty_multiline.ifj26
```

Vstup obsahuje ``.

Je to stejná kořenová chyba jako u prázdného běžného stringu.

## 3. Číslice bezprostředně za suffixem `d/D` není odmítnuta

Test:

```text
lexical_errors/double_suffix_followed_digit.ifj26
```

Vstup:

```text
1.0d2
```

Očekává se lexikální chyba. Aktuální scanner místo toho vytváří:

```text
TOKEN_DOUBLE_LITERAL(1)
TOKEN_INT_LITERAL(2)
```

Kontrola delimitru po suffixu odmítá písmeno a `_`, ale ne číslici.

## 4. Přetečení `Double` není kontrolováno

Test:

```text
lexical_errors/double_overflow.ifj26
```

Vstup:

```text
1e9999
```

Aktuální scanner přijme výsledek `strtod()` jako `inf`.
Specifikace ale řadí literál mimo povolený rozsah mezi lexikální chyby.

## 5. Jednořádkový blokový komentář v části před prvním tokenem ztratí naměřené odsazení

Test:

```text
comments/leading_block_comment_preserves_indent.ifj26
```

Příklad:

```text
a
    /* prefix */ b
    c
d
```

Komentář bez LF se má chovat jako whitespace. Aktuální implementace po jeho
zpracování znovu začne počítat odsazení od nuly, takže mezi `b` a `c` vyrobí
nechtěný další `INDENT`.
