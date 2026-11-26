/**
 * @par Projekt
 * Implementace překladače imperativního jazyka IFJ26
 * @file symtable.h
 * @author Denis Doubrava, Jakub Grásgruber, Eliáš Oharek, Michal Zedníček
 * @brief Definice symbolů a veřejné rozhraní tabulky symbolů.
 * Zpřístupňuje údaje o typech a vlastnostech proměnných, parametrů
 * a funkcí. Deklaruje správu rozsahů platnosti, vkládání a vyhledávání
 * symbolů a operace nad signaturami funkcí.
 * @note AI byla využita k lepšímu znění a obsahu komentářů.
 */

#ifndef SYMTABLE_H
#define SYMTABLE_H

// Symboly spravuje tento modul; ostatní analyzátory si jejich záznamy pouze vypůjčují.
#include <stdbool.h>
#include <stddef.h>

/**
 * @brief Metadata datových typů včetně interních a dosud neodvozených typů.
 */
typedef enum {
    type_int,
    type_double,
    type_string,
    type_unit,
    type_bool,   ///< Interní pravdivostní typ; explicitní Boolean ve zdrojovém kódu vyžaduje
                 ///< BOOLTHEN.
    type_any,    ///< Zástupný očekávaný typ vybraných parametrů vestavěných funkcí.
    type_unknown ///< Dosud neurčený datový typ.
} data_type;

/**
 * @brief Druh symbolu; parametry používají společnou kategorii sym_var.
 */
typedef enum { sym_var, sym_func } symbol_type;

/**
 * @brief Záznam symbolu vlastněný tabulkou symbolů.
 *
 * Položka type určuje aktivní větev unie data. Pole funkčních parametrů i kopie jejich jmen
 * zůstávají ve vlastnictví tabulky, i když jsou přístupné přes vypůjčený záznam.
 */
typedef struct {
    symbol_type type; ///< Určuje aktivní větev unie data.

    union {
        struct {
            data_type var_type;     ///< Datový typ hodnoty proměnné nebo parametru.
            bool      is_mutable;   ///< Povolení zápisu; parametry a val jsou neměnitelné.
            bool      is_parameter; ///< Rozlišuje parametr od běžné lokální proměnné.
            unsigned
                id; ///< Jedinečné ID v rámci tabulky; rozlišuje stejnojmenné zastíněné symboly.
        } var;

        struct {
            data_type ret_type;         ///< Návratový typ funkce, případně dosud neodvozený typ.
            size_t    param_list_count; ///< Počet seznamů; 0 označuje funkci bez seznamu parametrů.
            size_t *param_list_sizes; ///< Vlastněné pole počtů parametrů v jednotlivých seznamech.
            size_t  param_count;      ///< Celkový počet parametrů ve všech seznamech.
            data_type *params;        ///< Vlastněné pole typů v pořadí parametrů napříč seznamy.
            char **param_names; ///< Vlastněné kopie jmen v pořadí parametrů; před nastavením NULL.
        } func;
    } data;
} symbol_data;

/**
 * @brief Neprůhledný typ tabulky symbolů se skrytou správou rozsahů.
 */
typedef struct SymTable SymTable;

// Vytvoření a zrušení tabulky.

/**
 * @brief Vytvoří tabulku symbolů s prázdnou globální tabulkou funkcí.
 * @param initial_capacity Počáteční kapacita podkladových tabulek; 0 volí výchozí hodnotu.
 * @return Nová tabulka symbolů, nebo NULL při selhání.
 * @note Lokální rozsah ani vestavěné funkce se nevytvářejí automaticky.
 */
SymTable *symtable_create(size_t initial_capacity);

/**
 * @brief Uvolní všechny rozsahy, funkce a vlastněné záznamy.
 * @param table Rušená tabulka symbolů; NULL se ignoruje.
 * @warning Všechny dříve vypůjčené ukazatele na její symboly a jejich pole se stanou neplatnými.
 */
void symtable_destroy(SymTable *table);

// Správa rozsahů platnosti.

/**
 * @brief Vytvoří prázdný rozsah vnořený do aktuálního rozsahu.
 *
 * Nový rozsah vlastní samostatnou hashovací tabulku proměnných a parametrů. Do řetězce
 * rodičů je připojen až po úspěchu všech alokací.
 * @param table Tabulka symbolů, v níž se rozsah otevře.
 * @return True při úspěchu; false při NULL nebo nedostatku paměti.
 * @note Při selhání zůstává aktuální rozsah i jeho hloubka zachována.
 */
bool symtable_enter_scope(SymTable *table);

/**
 * @brief Opustí aktuální lokální rozsah a obnoví jeho rodiče.
 *
 * Uvolní symboly daného rozsahu, jeho hashovací tabulku i samotný objekt rozsahu. Globální
 * tabulka funkcí zůstává zachována.
 * @param table Tabulka symbolů s opouštěným rozsahem.
 * @return True při úspěchu; false při NULL nebo neexistujícím lokálním rozsahu.
 * @warning Ukazatele na symboly odstraněného rozsahu přestanou být platné.
 */
bool symtable_leave_scope(SymTable *table);

/**
 * @brief Zjistí počet aktivních lokálních rozsahů.
 * @param table Zkoumaná tabulka symbolů.
 * @return Aktuální hloubka, nebo 0 pro NULL či tabulku bez lokálního rozsahu.
 */
size_t symtable_scope_depth(SymTable *table);

// Vyhledávání symbolů.

/**
 * @brief Najde proměnnou nebo parametr pouze v aktuálním rozsahu.
 *
 * Vnější rozsahy se neprocházejí, což umožňuje samostatnou kontrolu duplicitních deklarací.
 * @param table Prohledávaná tabulka symbolů.
 * @param key Jméno ukončené nulovým znakem.
 * @return Vypůjčený záznam, nebo NULL při nenalezení, neplatném vstupu či chybějícím rozsahu.
 */
symbol_data *symtable_search_current_var(SymTable *table, const char *key);

/**
 * @brief Najde nejbližší viditelnou proměnnou nebo parametr.
 *
 * Prochází lokální rozsahy od aktuálního směrem k rodičům. První shoda realizuje zastínění
 * jmen.
 * @param table Prohledávaná tabulka symbolů.
 * @param key Hledané jméno ukončené nulou.
 * @return Vypůjčený záznam, nebo NULL při nenalezení či neplatném vstupu.
 */
symbol_data *symtable_search_var(SymTable *table, const char *key);

/**
 * @brief Vyhledá funkci v globální tabulce funkcí.
 * @param table Tabulka symbolů.
 * @param key Nulou ukončené jméno funkce.
 * @return Vypůjčený záznam funkce, nebo NULL při nenalezení či neplatných argumentech.
 * @note Vlastnictví záznamu zůstává tabulce symbolů.
 */
symbol_data *symtable_search_func(SymTable *table, const char *key);

// Registrace proměnných, parametrů a funkcí.

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
bool symtable_insert_var(SymTable *table, const char *key, data_type type, bool is_mutable);

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
bool symtable_insert_param(SymTable *table, const char *key, data_type type);

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
                          const data_type *params);

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
                                   const char *const *param_names);

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
bool symtable_insert_builtins(SymTable *table);

// Metadata typů a jejich kompatibilita.

/**
 * @brief Změní uložený návratový typ existující funkce.
 * @param table Tabulka obsahující funkci.
 * @param key Jméno funkce ukončené nulou.
 * @param ret_type Nový návratový typ.
 * @return True při změně; false, pokud funkce nebyla nalezena.
 * @note Přípustnost aktualizace posuzuje parser nebo sémantický analyzátor.
 */
bool symtable_set_func_return_type(SymTable *table, const char *key, data_type ret_type);

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
bool symtable_type_compatible(data_type expected, data_type actual);

#endif // SYMTABLE_H