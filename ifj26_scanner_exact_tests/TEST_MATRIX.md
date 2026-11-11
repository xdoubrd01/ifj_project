# Test matrix

Testy níže ověřují pouze chování scanneru. `rc=0` znamená lexikálně platný vstup,
`rc=1` očekávanou lexikální chybu.

## `tokens/` — přesné typy tokenů

| Test | Co ověřuje |
|---|---|
| `all_keywords` | všechna základní klíčová slova a typová klíčová slova |
| `identifiers_and_case` | identifikátory, `_` uvnitř jména, číslice a case sensitivity |
| `all_operators_delimiters` | `= + - * / == != < <= > >= ( ) : , . ; @main` |
| `prolog_tokenization` | přesné tokeny `import ifj26.*` |
| `unit_literal_tokens` | `()` jako `LPAREN` + `RPAREN` |
| `adjacent_unambiguous_tokens` | tokeny bez zbytečných mezer |
| `realistic_mixed_program` | realistický mix keywordů, typů, literálů, EOL/INDENT/DEDENT |
| `long_identifier` | růst dynamického bufferu u dlouhého identifikátoru |
| `unmatched_right_paren_is_token` | samostatné `)` je lexikálně token, chybu řeší parser |
| `missing_right_paren_same_physical_line` | chybějící `)` nesmí být samo o sobě lexikální chyba |
| `multiple_operator_tokens` | tokenizace `===` a `!==` na jednotlivé platné operátory |

## `numbers/` — validní číselné literály

| Test | Co ověřuje |
|---|---|
| `integers_valid` | `0`, běžná Int čísla, podtržítka mezi číslicemi |
| `doubles_fraction_valid` | `.5`, `0.5`, desetinné části, `_`, suffix `d/D` |
| `doubles_exponent_valid` | `e/E`, znaménko exponentu, `_` v exponentu, `d/D` |
| `unary_signs_are_tokens` | `-` a `+` jsou samostatné tokeny před číslem |

## `strings/`

| Test | Co ověřuje |
|---|---|
| `ordinary_and_escapes` | běžné stringy a `\\`, `\"`, `\n`, `\r`, `\t` |
| `empty_ordinary` | platný prázdný string `""` |
| `multiline_raw_content` | `"""..."""`, fyzické LF a nevyhodnocené escape |
| `empty_multiline` | prázdný multiline string |
| `multiline_contains_single_quotes` | jedna/dvě `"` uvnitř multiline obsahu |
| `long_string` | růst dynamického bufferu u dlouhého řetězce |

## `comments/`

| Test | Co ověřuje |
|---|---|
| `line_comment_after_token` | `//` po tokenu vytvoří hranici logického řádku |
| `comment_only_and_blank_lines` | comment-only a prázdné řádky nevytvářejí bloky |
| `inline_block_separates_tokens` | `/*...*/` bez LF se chová jako whitespace |
| `nested_block_comment` | vnořené blokové komentáře |
| `multiline_block_after_token` | blokový komentář s LF se chová jako jeden EOL |
| `multiline_block_comment_only` | samostatný multiline komentář nevytváří prázdný příkaz |
| `leading_block_comment_preserves_indent` | komentář v úvodní whitespace části nesmí změnit změřené odsazení |

## `indentation/`

| Test | Co ověřuje |
|---|---|
| `single_indent_dedent` | jeden `INDENT` a `DEDENT` |
| `nested_indentation` | několik úrovní odsazení |
| `multiple_dedents_at_once` | více `DEDENT` tokenů před dalším tokenem |
| `dedent_at_eof_no_newline` | uzavření bloku při EOF bez závěrečného LF |
| `comment_only_does_not_change_indent` | comment-only řádek na jiné úrovni neovlivní blok |

## `parentheses/`

| Test | Co ověřuje |
|---|---|
| `multiline_expression_no_structural_tokens_inside` | LF uvnitř `()` není `EOL`; nevzniká `INDENT/DEDENT` |
| `nested_multiline_parentheses` | vnořené závorky přes fyzické řádky |
| `comment_line_inside_parentheses` | comment-only continuation uvnitř závorek |

## `whitespace/`

| Test | Co ověřuje |
|---|---|
| `tabs_between_tokens` | tabulátor mimo začátek neprázdného řádku jako whitespace |
| `blank_lines_ignored` | prázdné/whitespace-only fyzické řádky |
| `final_newline` | `EOL` před `EOF` při skutečném LF |
| `no_final_newline` | správné ukončení tokenem `EOF` bez závěrečného LF |

## `lexical_errors/`

Každý z těchto testů očekává `rc=1` a `ERROR(1)` přesně v místě chyby.

### Čísla

- `leading_zero`
- `integer_trailing_underscore`
- `integer_double_underscore`
- `integer_attached_identifier`
- `double_missing_fraction`
- `double_fraction_leading_underscore`
- `double_fraction_trailing_underscore`
- `double_fraction_double_underscore`
- `exponent_missing_digits`
- `exponent_sign_missing_digits`
- `exponent_leading_underscore`
- `exponent_trailing_underscore`
- `exponent_double_underscore`
- `integer_invalid_d_suffix`
- `zero_invalid_D_suffix`
- `double_suffix_followed_digit`
- `double_suffix_followed_identifier`
- `unsupported_f_suffix`
- `double_overflow`

### Řetězce a komentáře

- `string_bad_escape`
- `string_unclosed`
- `string_physical_newline`
- `multiline_unclosed`
- `block_comment_unclosed`
- `nested_block_comment_unclosed`
- `multiline_block_trailing_token`

### Odsazení a závorky

- `invalid_dedent_level`
- `tab_at_line_start`
- `parentheses_bad_continuation_indent`

### `@main`

- `annotation_lone_at`
- `annotation_spaced`
- `annotation_wrong_name`
- `annotation_attached_identifier`
- `annotation_attached_underscore`

### Identifikátory a neznámé znaky

- `lone_underscore`
- `illegal_hash`
- `illegal_left_brace`
- `illegal_left_bracket`
- `illegal_ampersand`
- `illegal_pipe`
- `illegal_apostrophe`
- `illegal_backtick`
