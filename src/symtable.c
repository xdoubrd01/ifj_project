/**
 * @par Projekt
 * Implementace překladače imperativního jazyka IFJ26
 * @file symtable.c
 * @author Denis Doubrava, Jakub Grásgruber, Eliáš Oharek, Michal Zedníček
 * @brief Správa funkcí, proměnných, parametrů a rozsahů platnosti
 * nad hashovacími tabulkami. Udržuje globální tabulku funkcí a řetězec
 * lokálních rozsahů platnosti pro proměnné a parametry. Umožňuje
 * vyhledávání se zastíněním jmen, eviduje vlastnosti symbolů
 * a uvolňuje jejich data při zániku rozsahu.
 * @note AI byla využita k lepšímu znění a obsahu komentářů.
 */

#include "symtable.h"
#include "hashtable.h"
#include "str_utils.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief Výchozí kapacita každé podkladové hashovací tabulky.
 */
#define DEFAULT_SYMTABLE_CAPACITY 32U

/**
 * @brief Lokální rozsah propojený se svým rodičovským rozsahem.
 *
 * Každý rozsah vlastní tabulku svých proměnných a parametrů. Ukazatel parent pouze odkazuje
 * na vnější rozsah a nepřebírá jeho vlastnictví.
 */
typedef struct Scope {
    hashtable    *table;
    struct Scope *parent;
} Scope;

/**
 * @brief Skrytý stav tabulky symbolů.
 *
 * Funkce se ukládají globálně, odděleně od lokálních proměnných. Lokální rozsahy tvoří
 * zásobník prostřednictvím ukazatelů na rodiče.
 */
struct SymTable {
    hashtable *functions;
    Scope     *current_scope;
    size_t     initial_capacity;
    size_t     scope_depth;
    unsigned   next_var_id; ///< ID určené následující proměnné nebo parametru.
};

// Interní pomocné funkce.

/**
 * @brief Nahradí požadavek nulové kapacity výchozí hodnotou.
 * @param capacity Požadovaná počáteční kapacita.
 * @return Původní nenulová kapacita, nebo výchozí kapacita modulu.
 */
static size_t normalized_capacity(size_t capacity) {
    // Nula vybírá výchozí kapacitu, nikoli prázdnou alokaci.
    return capacity == 0 ? DEFAULT_SYMTABLE_CAPACITY : capacity;
}

/**
 * @brief Uvolní vlastněné pole jmen parametrů a jeho inicializované řetězce.
 * @param names Uvolňované pole; NULL se ignoruje.
 * @param count Počet již inicializovaných jmen k uvolnění.
 */
static void free_param_names(char **names, size_t count) {
    // Prázdné nebo dosud nevytvořené pole jmen nepotřebuje úklid.
    if (names == NULL) {
        return;
    }

    // Uvolňujeme pouze jména, jejichž položky již byly inicializovány.
    for (size_t i = 0; i < count; ++i) {
        free(names[i]);
    }

    free(names);
}

/**
 * @brief Uvolní záznam symbolu a jeho vlastněná pole.
 *
 * U funkce uvolní typy parametrů, velikosti seznamů a kopie jmen. Nakonec uvolní samotný
 * záznam; klíč zůstává ve správě hashovací tabulky.
 * @param data Ukazatel na záznam symbolu; NULL se ignoruje.
 */
static void free_symbol(void *data) {
    // Obecný argument obslužné funkce převedeme na konkrétní záznam symbolu.
    symbol_data *symbol = data;

    if (symbol == NULL) {
        return;
    }

    // Záznam funkce vlastní pole typů, velikostí seznamů a případné kopie jmen parametrů.
    if (symbol->type == sym_func) {
        free(symbol->data.func.param_list_sizes);
        free(symbol->data.func.params);
        free_param_names(symbol->data.func.param_names, symbol->data.func.param_count);
    }

    /* Proměnná nemá další samostatně alokovaná data.
     * Vlastněná pole funkce již byla uvolněna výše. */
    free(symbol);
}

/**
 * @brief Uvolní hodnoty symbolů bez zrušení jejich hashovací tabulky.
 * @param table Tabulka s uvolňovanými záznamy; NULL se ignoruje.
 * @warning Používejte pouze při rušení a bezprostředně navazujte ht_free().
 */
static void free_symbol_table_data(hashtable *table) {
    if (table == NULL) {
        return;
    }

    /* K hodnotám přistupujeme výhradně přes veřejné rozhraní hashovací tabulky.
     * Obslužná funkce uvolní záznamy, ale klíče nadále vlastní tabulka.
     * V buňkách pak zůstanou neplatné ukazatele; tabulku je nutné zrušit. */
    ht_foreach(table, free_symbol);
}

// Vytvoření a zrušení tabulky symbolů.

/**
 * @brief Vytvoří tabulku symbolů s prázdnou globální tabulkou funkcí.
 * @param initial_capacity Počáteční kapacita podkladových tabulek; 0 volí výchozí hodnotu.
 * @return Nová tabulka symbolů, nebo NULL při selhání.
 * @note Lokální rozsah ani vestavěné funkce se nevytvářejí automaticky.
 */
SymTable *symtable_create(size_t initial_capacity) {
    // Nejprve vytvoříme vnější objekt, teprve potom jeho vlastněné tabulky.
    SymTable *table = malloc(sizeof(*table));
    if (table == NULL) {
        return NULL;
    }

    /* Zpočátku neexistuje žádný lokální rozsah.
     * Normalizovanou kapacitu uložíme pro funkce i později vytvářené rozsahy. */
    table->current_scope    = NULL;
    table->scope_depth      = 0;
    table->next_var_id      = 0;
    table->initial_capacity = normalized_capacity(initial_capacity);

    // Funkce mají vlastní tabulku nezávislou na lokálních proměnných.
    table->functions = ht_create(table->initial_capacity);

    if (table->functions == NULL) {
        // Pokud nelze vytvořit tabulku funkcí, uvolníme vnější objekt.
        free(table);
        return NULL;
    }

    /* Objekt je připraven k použití.
     * Otevření rozsahu a registraci vestavěných funkcí musí zajistit volající. */
    return table;
}

/**
 * @brief Uvolní všechny rozsahy, funkce a vlastněné záznamy.
 * @param table Rušená tabulka symbolů; NULL se ignoruje.
 * @warning Všechny dříve vypůjčené ukazatele na její symboly a jejich pole se stanou neplatnými.
 */
void symtable_destroy(SymTable *table) {
    if (table == NULL) {
        return;
    }

    /* Než zrušíme globální funkce, postupně opustíme všechny lokální rozsahy.
     * Každý krok uvolní jeden rozsah a obnoví jeho rodiče. */
    while (table->current_scope != NULL) {
        symtable_leave_scope(table);
    }

    /* Nejprve uvolníme záznamy funkcí a jejich pole.
     * Potom zrušíme hashovací tabulku, která vlastní klíče a pole buněk. */
    free_symbol_table_data(table->functions);
    ht_free(table->functions);

    // Vlastněné rozsahy i záznamy funkcí jsou již uvolněny.
    free(table);
}

// Správa lokálních rozsahů platnosti.

/**
 * @brief Vytvoří prázdný rozsah vnořený do aktuálního rozsahu.
 *
 * Nový rozsah vlastní samostatnou hashovací tabulku proměnných a parametrů. Do řetězce
 * rodičů je připojen až po úspěchu všech alokací.
 * @param table Tabulka symbolů, v níž se rozsah otevře.
 * @return True při úspěchu; false při NULL nebo nedostatku paměti.
 * @note Při selhání zůstává aktuální rozsah i jeho hloubka zachována.
 */
bool symtable_enter_scope(SymTable *table) {
    if (table == NULL) {
        return false;
    }

    // Nový rozsah připojíme až po úspěchu všech potřebných alokací.
    Scope *scope = malloc(sizeof(*scope));
    if (scope == NULL) {
        return false;
    }

    // Každý lokální rozsah dostane vlastní prázdnou hashovací tabulku.
    scope->table = ht_create(table->initial_capacity);
    if (scope->table == NULL) {
        /* Je nutné uvolnit jen dosud nepřipojený objekt nového rozsahu.
         * Aktivní řetězec rozsahů zůstává beze změny. */
        free(scope);
        return false;
    }

    /* Inicializovaný rozsah propojíme s rodičem a nastavíme jako aktuální.
     * Změna probíhá až po úspěchu všech potřebných alokací. */
    scope->parent        = table->current_scope;
    table->current_scope = scope;
    table->scope_depth++;

    return true;
}

/**
 * @brief Opustí aktuální lokální rozsah a obnoví jeho rodiče.
 *
 * Uvolní symboly daného rozsahu, jeho hashovací tabulku i samotný objekt rozsahu. Globální
 * tabulka funkcí zůstává zachována.
 * @param table Tabulka symbolů s opouštěným rozsahem.
 * @return True při úspěchu; false při NULL nebo neexistujícím lokálním rozsahu.
 * @warning Ukazatele na symboly odstraněného rozsahu přestanou být platné.
 */
bool symtable_leave_scope(SymTable *table) {
    if (table == NULL || table->current_scope == NULL) {
        return false;
    }

    /* Zapamatujeme si opouštěný rozsah a nejprve obnovíme rodiče.
     * Rodič nadále patří do aktivního řetězce rozsahů. */
    Scope *scope         = table->current_scope;
    table->current_scope = scope->parent;

    /* Záznamy symbolů uvolníme před zrušením hashovací tabulky.
     * Ta uvolňuje své klíče, nikoli hodnoty symbolů. */
    free_symbol_table_data(scope->table);
    ht_free(scope->table);

    // Po uvolnění vlastněných dat zrušíme i objekt rozsahu.
    free(scope);

    // Počet aktivních rozsahů udržujeme v souladu s řetězcem rodičů.
    if (table->scope_depth > 0) {
        table->scope_depth--;
    }

    return true;
}

/**
 * @brief Zjistí počet aktivních lokálních rozsahů.
 * @param table Zkoumaná tabulka symbolů.
 * @return Aktuální hloubka, nebo 0 pro NULL či tabulku bez lokálního rozsahu.
 */
size_t symtable_scope_depth(SymTable *table) {
    // Pro NULL i tabulku bez lokálního rozsahu vracíme nulovou hloubku.
    return table == NULL ? 0 : table->scope_depth;
}

// Vyhledávání symbolů.

/**
 * @brief Najde proměnnou nebo parametr pouze v aktuálním rozsahu.
 *
 * Vnější rozsahy se neprocházejí, což umožňuje samostatnou kontrolu duplicitních deklarací.
 * @param table Prohledávaná tabulka symbolů.
 * @param key Jméno ukončené nulovým znakem.
 * @return Vypůjčený záznam, nebo NULL při nenalezení, neplatném vstupu či chybějícím rozsahu.
 */
symbol_data *symtable_search_current_var(SymTable *table, const char *key) {
    if (table == NULL || table->current_scope == NULL || key == NULL) {
        return NULL;
    }

    /* Pro kontrolu duplicity hledáme pouze v aktuálním rozsahu.
     * Stejnojmenná deklarace ve vnějším rozsahu zde není duplicitou. */
    symbol_data *symbol = ht_search(table->current_scope->table, key);
    if (symbol == NULL || symbol->type != sym_var) {
        return NULL;
    }

    // Vracíme vypůjčený záznam bez alokace a kopírování.
    return symbol;
}

/**
 * @brief Najde nejbližší viditelnou proměnnou nebo parametr.
 *
 * Prochází lokální rozsahy od aktuálního směrem k rodičům. První shoda realizuje zastínění
 * jmen.
 * @param table Prohledávaná tabulka symbolů.
 * @param key Hledané jméno ukončené nulou.
 * @return Vypůjčený záznam, nebo NULL při nenalezení či neplatném vstupu.
 */
symbol_data *symtable_search_var(SymTable *table, const char *key) {
    if (table == NULL || key == NULL) {
        return NULL;
    }

    // Hledáme od nejvnitřnějšího rozsahu směrem k rodičům, čímž vzniká zastínění.
    for (Scope *scope = table->current_scope; scope != NULL; scope = scope->parent) {
        symbol_data *symbol = ht_search(scope->table, key);

        // První nalezená proměnná zastíní stejné jméno ve vnějších rozsazích.
        if (symbol != NULL && symbol->type == sym_var) {
            return symbol;
        }
    }

    // Žádný aktivní lokální rozsah hledanou proměnnou nebo parametr neobsahuje.
    return NULL;
}

/**
 * @brief Vyhledá funkci v globální tabulce funkcí.
 * @param table Tabulka symbolů.
 * @param key Nulou ukončené jméno funkce.
 * @return Vypůjčený záznam funkce, nebo NULL při nenalezení či neplatných argumentech.
 * @note Vlastnictví záznamu zůstává tabulce symbolů.
 */
symbol_data *symtable_search_func(SymTable *table, const char *key) {
    if (table == NULL || key == NULL) {
        return NULL;
    }

    /* Funkce hledáme pouze v jejich globální tabulce.
     * Lokální rozsahy proměnných se při této operaci neprocházejí. */
    symbol_data *symbol = ht_search(table->functions, key);
    if (symbol == NULL || symbol->type != sym_func) {
        return NULL;
    }

    // Vlastnictví vráceného záznamu zůstává tabulce symbolů.
    return symbol;
}

// Společné vložení lokální proměnné nebo neměnitelného parametru.
/**
 * @brief Vloží společnou reprezentaci lokální proměnné nebo parametru.
 *
 * Nejprve kontroluje duplicitu pouze v aktuálním rozsahu. Po úspěšném vložení přidělí
 * záznamu další ID a tabulka symbolů převezme jeho vlastnictví.
 * @param table Tabulka s otevřeným lokálním rozsahem.
 * @param key Jméno symbolu.
 * @param type Ukládaný datový typ.
 * @param is_mutable Povolení následného přiřazení.
 * @param is_parameter Příznak funkčního parametru.
 * @return True při úspěchu; false při neplatném vstupu, duplicitě nebo nedostatku paměti.
 */
static bool insert_local_variable(SymTable *table, const char *key, data_type type, bool is_mutable,
                                  bool is_parameter) {
    if (table == NULL || table->current_scope == NULL || key == NULL) {
        return false;
    }

    // Duplicita je zakázána v aktuálním rozsahu; vnější jméno lze zastínit.
    if (symtable_search_current_var(table, key) != NULL) {
        return false;
    }

    /* Záznam má vlastní alokaci, takže se při přesunu buněk při přehashování
     * nemění jeho adresa. */
    symbol_data *symbol = malloc(sizeof(*symbol));
    if (symbol == NULL) {
        return false;
    }

    /* Uložíme dodaný typ a měnitelnost.
     * Přípustnost metadat v daném místě programu kontroluje analyzátor. */
    symbol->type                  = sym_var;
    symbol->data.var.var_type     = type;
    symbol->data.var.is_mutable   = is_mutable;
    symbol->data.var.is_parameter = is_parameter;
    // Generátor používá toto ID k rozlišení proměnných se stejným zdrojovým jménem.
    symbol->data.var.id = table->next_var_id;

    // Při selhání vložení uvolníme záznam, který tabulka ještě nepřevzala.
    if (!ht_insert(table->current_scope->table, key, symbol)) {
        free(symbol);
        return false;
    }

    /* ID se při opouštění rozsahu nevrací, a proto rozlišují zastíněná jména.
     * Záznam vlastní tabulka symbolů; kopii klíče vlastní hashovací tabulka. */
    // Další úspěšně vložený symbol dostane následující ID bez ohledu na svůj rozsah.
    table->next_var_id++;
    return true;
}

/**
 * @brief Zaregistruje proměnnou v aktuálním lokálním rozsahu.
 *
 * V aktuálním rozsahu odmítá duplicity, ale umožňuje zastínění vnějšího jména. Každý nový
 * záznam dostane ID jedinečné v rámci této tabulky symbolů.
 * @param table Cílová tabulka s otevřeným lokálním rozsahem.
 * @param key Jméno proměnné; při úspěchu se zkopíruje.
 * @param type Ukládaná informace o datovém typu.
 * @param is_mutable Zda lze proměnné následně přiřazovat.
 * @return True při vložení; false při neplatném vstupu, chybějícím rozsahu, duplicitě nebo selhání
 * alokace.
 * @note Přípustnost typu v konkrétním místě programu kontroluje parser nebo sémantický analyzátor.
 */
bool symtable_insert_var(SymTable *table, const char *key, data_type type, bool is_mutable) {
    return insert_local_variable(table, key, type, is_mutable, false);
}

/**
 * @brief Zaregistruje neměnitelný parametr v aktuálním rozsahu.
 *
 * Parametr sdílí reprezentaci proměnné, má však is_parameter nastaveno na true a is_mutable
 * na false. Nové ID rozlišuje stejnojmenné symboly v různých rozsazích.
 * @param table Tabulka s aktivním lokálním rozsahem.
 * @param key Jméno parametru; při úspěchu se zkopíruje.
 * @param type Datový typ parametru.
 * @return True při vložení; false při neplatném vstupu, chybějícím rozsahu, duplicitě nebo
 * nedostatku paměti.
 */
bool symtable_insert_param(SymTable *table, const char *key, data_type type) {
    return insert_local_variable(table, key, type, false, true);
}

/**
 * @brief Zaregistruje globální funkci a zkopíruje typy parametrů.
 *
 * Jméno funkce musí být jedinečné, přetěžování není podporováno. Záznam vytvoří s jedním
 * seznamem parametrů; případné další členění upravuje modul PARAMLISTS.
 * @param table Cílová tabulka symbolů.
 * @param key Jméno funkce; při úspěchu se zkopíruje.
 * @param ret_type Ukládaný návratový typ.
 * @param param_count Počet prvků pole params.
 * @param params Pole typů; při nulovém počtu smí být NULL.
 * @return True při úspěchu; false při neplatném vstupu, duplicitě, přetečení nebo nedostatku
 * paměti.
 * @note Vstupní pole zůstává volajícímu; tabulka vlastní jeho kopii.
 * @note Jména parametrů jsou zpočátku NULL; nastavuje je symtable_set_func_param_names().
 */
bool symtable_insert_func(SymTable *table, const char *key, data_type ret_type, size_t param_count,
                          const data_type *params) {
    if (table == NULL || key == NULL || (param_count > 0 && params == NULL)) {
        return false;
    }

    // Jméno funkce musí být jedinečné; přetěžování funkcí není podporováno.
    if (symtable_search_func(table, key) != NULL) {
        return false;
    }

    symbol_data *symbol = malloc(sizeof(*symbol));
    if (symbol == NULL) {
        return false;
    }

    /* Zpočátku není alokováno pole parametrů.
     * Funkce s nulovým počtem parametrů tuto alokaci nepotřebuje. */
    symbol->type = sym_func;
    // Základní deklarace používá jeden seznam; rozšíření může členění později upravit.
    symbol->data.func.param_list_count = 1;
    symbol->data.func.param_list_sizes = NULL;
    symbol->data.func.ret_type         = ret_type;
    symbol->data.func.param_count      = param_count;
    symbol->data.func.params           = NULL;
    symbol->data.func.param_names      = NULL;

    if (param_count > 0) {
        // Před alokací ověříme, že násobení počtu prvků jejich velikostí nepřeteče.
        if (param_count > SIZE_MAX / sizeof(*symbol->data.func.params)) {
            free(symbol);
            return false;
        }

        symbol->data.func.params = malloc(param_count * sizeof(*symbol->data.func.params));
        if (symbol->data.func.params == NULL) {
            free(symbol);
            return false;
        }

        /* Signaturu kopírujeme, neukládáme pouze odkaz na vstupní pole.
         * Volající může původní pole po návratu upravit nebo uvolnit. */
        for (size_t i = 0; i < param_count; i++) {
            symbol->data.func.params[i] = params[i];
        }
    }

    // Při neúspěchu vložení nepřešla žádná nová alokace do vlastnictví tabulky.
    if (!ht_insert(table->functions, key, symbol)) {
        free(symbol->data.func.params);
        free(symbol);
        return false;
    }

    // Tabulka symbolů nyní vlastní záznam i případná pole parametrů.
    return true;
}

/**
 * @brief Nahradí jména parametrů existující funkce jejich vlastní kopií.
 *
 * Nejprve ověří vstupy a vytvoří úplnou kopii pole i řetězců. Teprve pak nahradí původní
 * jména; při selhání se dosavadní údaje nemění.
 * @param table Tabulka obsahující danou funkci.
 * @param key Jméno existující funkce.
 * @param param_count Počet jmen shodný s počtem parametrů funkce.
 * @param param_names Jména v pořadí deklarace; NULL je přípustné jen při nulovém počtu.
 * @return True při úspěchu; false při neplatném vstupu, neshodě počtu, přetečení nebo selhání
 * alokace.
 * @note Jména musí být nenulová a neprázdná; jejich lexikální platnost a duplicity kontroluje
 * parser.
 * @note Volající si ponechává vlastnictví všech vstupů; vestavěné funkce mohou mít jména NULL.
 * @warning Úspěšná náhrada zneplatní dříve vypůjčené ukazatele na jména, nikoli pole typů
 * parametrů.
 */
bool symtable_set_func_param_names(SymTable *table, const char *key, size_t param_count,
                                   const char *const *param_names) {
    // Jména parametrů lze přiřadit pouze již zaregistrované funkci.
    symbol_data *symbol = symtable_search_func(table, key);

    if (symbol == NULL || symbol->data.func.param_count != param_count ||
        (param_count > 0 && param_names == NULL)) {
        return false;
    }

    // Funkce bez parametrů nepotřebuje úložiště pro jména.
    if (param_count == 0) {
        return true;
    }

    if (param_count > SIZE_MAX / sizeof(char *)) {
        return false;
    }

    // Všechna vstupní jména ověříme ještě před alokací náhradního pole.
    for (size_t i = 0; i < param_count; ++i) {
        if (param_names[i] == NULL || param_names[i][0] == '\0') {
            return false;
        }
    }

    char **copies = malloc(param_count * sizeof(*copies));
    if (copies == NULL) {
        return false;
    }

    for (size_t i = 0; i < param_count; ++i) {
        copies[i] = string_duplicate(param_names[i]);

        if (copies[i] == NULL) {
            free_param_names(copies, i);
            return false;
        }
    }

    // Zveřejníme až úplnou kopii; vstupy mohou odkazovat na dosavadní jména.
    free_param_names(symbol->data.func.param_names, param_count);
    symbol->data.func.param_names = copies;

    return true;
}

/**
 * @brief Zaregistruje deset vestavěných funkcí základního IFJ26.
 *
 * Uloží jejich názvy, návratové typy a typy parametrů. Každé vložení provádí samostatně a
 * při prvním neúspěchu skončí.
 * @param table Cílová tabulka symbolů.
 * @return True, pokud se vložily všechny funkce; jinak false.
 * @note Opakované volání po úspěchu narazí na duplicitní jména.
 * @warning Operace není atomická: dříve vložené funkce při pozdějším selhání zůstávají v tabulce.
 */
bool symtable_insert_builtins(SymTable *table) {
    if (table == NULL) {
        return false;
    }

    /* Typy parametrů vestavěných funkcí popíšeme dočasnými poli.
     * symtable_insert_func() je před návratem z této funkce zkopíruje. */
    const data_type any_param[]             = {type_any};
    const data_type double_param[]          = {type_double};
    const data_type string_param[]          = {type_string};
    const data_type string_int_int_params[] = {type_string, type_int, type_int};
    const data_type string_int_params[]     = {type_string, type_int};
    const data_type int_param[]             = {type_int};

    /* Při prvním neúspěchu skončíme.
     * Dříve vložené funkce se zpětně neodstraňují. */
    if (!symtable_insert_func(table, "print", type_unit, 1, any_param)) {
        return false;
    }

    // Vstupní vestavěné funkce nepřijímají argumenty.
    if (!symtable_insert_func(table, "readLine", type_string, 0, NULL)) {
        return false;
    }
    if (!symtable_insert_func(table, "readInt", type_int, 0, NULL)) {
        return false;
    }
    if (!symtable_insert_func(table, "readDouble", type_double, 0, NULL)) {
        return false;
    }

    // Převodní funkce zaregistrujeme s odpovídajícími typy parametrů.
    if (!symtable_insert_func(table, "toStr", type_string, 1, any_param)) {
        return false;
    }
    if (!symtable_insert_func(table, "toInt", type_int, 1, double_param)) {
        return false;
    }

    // Doplníme řetězcové operace a převody znakových kódů.
    if (!symtable_insert_func(table, "strLength", type_int, 1, string_param)) {
        return false;
    }
    if (!symtable_insert_func(table, "subStr", type_string, 3, string_int_int_params)) {
        return false;
    }
    if (!symtable_insert_func(table, "ord", type_int, 2, string_int_params)) {
        return false;
    }
    if (!symtable_insert_func(table, "chr", type_string, 1, int_param)) {
        return false;
    }

    return true;
}

// Metadata typů a kontrola jejich kompatibility.

/**
 * @brief Změní uložený návratový typ existující funkce.
 * @param table Tabulka obsahující funkci.
 * @param key Jméno funkce ukončené nulou.
 * @param ret_type Nový návratový typ.
 * @return True při změně; false, pokud funkce nebyla nalezena.
 * @note Přípustnost aktualizace posuzuje parser nebo sémantický analyzátor.
 */
bool symtable_set_func_return_type(SymTable *table, const char *key, data_type ret_type) {
    // Upravujeme existující deklaraci místo vytvoření nové funkce.
    symbol_data *symbol = symtable_search_func(table, key);
    if (symbol == NULL) {
        return false;
    }

    // O přípustnosti změny návratového typu rozhoduje parser.
    // Aktualizujeme uložený údaj o návratu, nikoli samotné tělo funkce.
    symbol->data.func.ret_type = ret_type;
    return true;
}

/**
 * @brief Ověří kompatibilitu dvou uložených typů.
 *
 * Nejprve odmítne type_unknown. Jinak přijímá očekávaný type_any, shodné typy nebo
 * implicitní rozšíření Int na Double.
 * @param expected Požadovaný typ; může být type_any.
 * @param actual Skutečný typ hodnoty.
 * @return True pro uvedené případy kompatibility; jinak false.
 * @note Neověřuje, zda je daný typ povolen v konkrétním kontextu zdrojového jazyka.
 */
bool symtable_type_compatible(data_type expected, data_type actual) {
    // Neodvozený typ nepotvrzuje kompatibilitu ani se stejným typem unknown.
    if (expected == type_unknown || actual == type_unknown) {
        return false;
    }

    // Po vyloučení unknown přijímá očekávaný typ Any libovolný skutečný typ.
    if (expected == type_any) {
        return true;
    }

    // Shodné známé typy nepotřebují implicitní převod.
    if (expected == actual) {
        return true;
    }

    /* Jediný podporovaný implicitní převod je rozšíření Int na Double.
     * Zde se ověřuje kompatibilita; samotný převod se provede v generovaném kódu. */
    return expected == type_double && actual == type_int;
}