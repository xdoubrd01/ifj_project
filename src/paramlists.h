/**
 * @par Projekt
 * Implementace překladače imperativního jazyka IFJ26
 * @file paramlists.h
 * @author Denis Doubrava (xdoubrd01), Jakub Grásgruber (xgrasgj00), Eliáš Oharek (xoharee00),
 * Michal Zedníček (xzednim00)
 * @brief Datové struktury a rozhraní rozšíření PARAMLISTS.
 * Umožňuje zaznamenávat délky jednotlivých seznamů parametrů a argumentů.
 * Deklaruje kontrolu tvaru volání a vytvoření uzlu volání funkce samotným jménem.
 * Zpřístupňuje také sledování lokálních deklarací při sběru závislostí funkcí.
 * @note AI byla využita k lepšímu znění a obsahu komentářů.
 */
#ifndef PARAMLISTS_H
#define PARAMLISTS_H
#include "expr_tree.h"
#include "scanner.h"

/**
 * @brief Dynamické pole délek jednotlivých seznamů parametrů nebo argumentů.
 *
 * Před prvním použitím musí být struktura vynulována. Nulový počet seznamů
 * označuje funkci bez seznamu parametrů; položka s délkou 0 označuje prázdný seznam ().
 */
typedef struct {
    size_t *sizes;           ///< Vlastněné pole počtů prvků v jednotlivých seznamech.
    size_t  count, capacity; ///< Počet uložených seznamů a alokovaná kapacita pole.
} ParamLists;

/**
 * @brief Přidá délku jednoho seznamu a podle potřeby zvětší pole.
 * @param lists Platná vynulovaná nebo již používaná struktura.
 * @param length Počet parametrů nebo argumentů přidávaného seznamu; může být 0.
 * @return true při úspěchu; false při přetečení velikosti nebo selhání alokace.
 * @note Při neúspěchu se nový seznam nepřidá a dosavadní obsah zůstane zachován.
 */
bool paramlists_append(ParamLists *lists, size_t length);

/**
 * @brief Uvolní pole délek seznamů a vynuluje strukturu pro další použití.
 * @param lists Platná inicializovaná struktura; nesmí být NULL.
 */
void paramlists_free(ParamLists *lists);

/**
 * @brief Uloží vlastní kopii délek seznamů do signatury existující funkce.
 * @param table Tabulka symbolů obsahující cílovou funkci.
 * @param name Jméno cílové funkce ukončené nulovým znakem.
 * @param lists Platné členění parametrů do seznamů; jeho vlastnictví se nepřebírá.
 * @return 0 při úspěchu; 99 při nenalezení funkce, přetečení nebo chybě alokace.
 * @note Původní členění se nahradí až po úspěšném vytvoření nové kopie.
 */
int paramlists_set_signature(SymTable *table, const char *name, const ParamLists *lists);

/**
 * @brief Porovná počet seznamů a jejich délky se signaturou funkce.
 * @param table Prohledávaná tabulka symbolů.
 * @param name Jméno volané funkce.
 * @param lists Platné členění argumentů konkrétního volání.
 * @return 0 při shodě; 3 pro neznámou funkci; 5 pro jiné členění argumentů.
 * @note Typy argumentů kontroluje následné volání sem_check_call().
 */
int paramlists_check_shape(SymTable *table, const char *name, const ParamLists *lists);

/**
 * @brief Zjistí, zda funkce nemá žádný seznam parametrů a lze ji volat samotným jménem.
 * @param table Prohledávaná tabulka symbolů.
 * @param name Jméno zkoumané funkce.
 * @return true pro existující funkci s nulovým počtem seznamů; jinak false.
 * @note Funkce s prázdným seznamem () má jeden seznam, a proto podmínku nesplňuje.
 */
bool paramlists_is_bare(SymTable *table, const char *name);

/**
 * @brief Ověří volání funkce samotným jménem a vytvoří jeho uzel EXPR_CALL.
 * @param table Tabulka symbolů se signaturami funkcí.
 * @param name Ukazatel na vlastněné jméno; při úspěchu se převezme a nastaví na NULL.
 * @param out Platný výstupní ukazatel na vytvořený strom; při chybě se nemění.
 * @return 0 při úspěchu; jinak kód kontroly volání nebo 99 při chybě alokace.
 * @pre Volající již ověřil, že identifikátor neoznačuje viditelnou proměnnou.
 * @note Při neúspěchu zůstává vlastnictví jména volajícímu; při úspěchu vlastní uzel volající.
 */
int paramlists_bare_call(SymTable *table, char **name, ExprNode **out);

/** @brief Vlastněné jméno lokální deklarace a hloubka jejího rozsahu platnosti. */
typedef struct {
    char  *name;  ///< Vlastněná kopie jména lokální proměnné.
    size_t depth; ///< Hloubka odsazení, ve které byla deklarace zařazena.
} ParamBinding;

/**
 * @brief Stav sledování lokálních deklarací při sběru závislostí v prvním průchodu.
 *
 * Rozlišuje lokální jména od možných volání funkcí, aby zastínění nevytvářelo
 * falešné cykly v grafu volání. Před použitím se vynuluje a nastaví se table.
 */
typedef struct {
    ParamBinding *items;                  ///< Vlastněné pole aktivních lokálních deklarací.
    size_t        count, capacity, depth; ///< Počet deklarací, kapacita pole a aktuální hloubka.
    char         *pending;  ///< Vlastněné jméno deklarace čekající na dokončení příkazu.
    TokenType     previous; ///< Typ naposledy zpracovaného tokenu.
    SymTable     *table;    ///< Vypůjčená tabulka symbolů, například s parametry funkce.
} ParamBindings;

/**
 * @brief Zpracuje token a rozliší odkaz na funkci od lokálního jména.
 * @param bindings Platný stav sledování deklarací v aktuálně procházeném těle funkce.
 * @param token Platný aktuální token; jeho vlastnictví se nepřebírá.
 * @param next Typ následujícího tokenu pro rozpoznání volání nebo přiřazení.
 * @param reference Výstup: 0 žádný odkaz, 1 volání se závorkami, 2 možné volání samotným jménem.
 * @return 0 při úspěchu; 99 při přetečení nebo nedostatku paměti.
 * @note Sleduje INDENT a DEDENT, nová lokální jména a dokončení jejich deklarací.
 */
int paramlists_track(ParamBindings *bindings, const Token *token, TokenType next, int *reference);

/**
 * @brief Uvolní aktivní i čekající jména, pole deklarací a vynuluje celý stav.
 * @param bindings Platná inicializovaná struktura; nesmí být NULL.
 * @note Vypůjčená tabulka symbolů se neuvolňuje; po vynulování se zruší pouze odkaz na ni.
 */
void paramlists_bindings_free(ParamBindings *bindings);
#endif
