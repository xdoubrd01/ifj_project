/**
 * @project Implementace překladače imperativního jazyka IFJ26
 * @file dynamic_buffer.h
 * @author Denis Doubrava (xdoubrd01)
 * @brief Datová struktura a rozhraní obecného dynamického textového bufferu.
 * Definuje strukturu DynamicBuffer s daty, délkou a kapacitou.
 * Deklaruje operace pro rezervaci paměti, přidávání obsahu,
 * vymazání a uvolnění bufferu.
 * @note AI byla využita k lepšímu znění a obsahu komentářů.
 */

#ifndef DYNAMIC_BUFFER_H
#define DYNAMIC_BUFFER_H

// Seznam argumentů va_list umožňuje připojovat formátovaný text bez vlastní variadické obálky.
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>

/**
 * @brief Dynamický textový buffer s oddělenou délkou a kapacitou.
 *
 * length nezahrnuje koncovou nulu; capacity udává velikost alokovaného pole v bajtech. Po
 * inicializaci nemusí být pole dosud alokováno.
 */
typedef struct {
    char  *data;     ///< Vlastněné datové pole; před první alokací NULL.
    size_t length;   ///< Počet uložených znaků bez koncového nulového znaku.
    size_t capacity; ///< Počet alokovaných bajtů včetně místa pro terminátor.
} DynamicBuffer;

/**
 * @brief Inicializuje prázdný dynamický buffer bez alokace.
 *
 * Vynuluje ukazatel na data, délku i kapacitu. Funkce neuvolňuje předchozí obsah a slouží
 * pro dosud neinicializovaný nebo již uvolněný objekt.
 * @param buffer Platný ukazatel na strukturu bufferu; nesmí být NULL.
 * @warning Nepoužívejte k přepsání živého bufferu bez předchozího uvolnění.
 */
void dynamic_buffer_init(DynamicBuffer *buffer);
/**
 * @brief Zajistí kapacitu pro požadovaný obsah včetně koncové nuly.
 *
 * Parametr length udává délku obsahu bez koncového nulového znaku. Je-li kapacita
 * dostatečná, nealokuje se; jinak se bezpečně vypočítá nová kapacita a provede realloc().
 * Dosavadní obsah a logická délka zůstávají zachovány.
 * @param buffer Inicializovaný buffer.
 * @param length Požadovaná délka obsahu bez koncové nuly.
 * @return  true při dostatečné nebo úspěšně zvětšené kapacitě; false při NULL, přetečení nebo
 * selhání alokace.
 * @note Při neúspěšné realokaci zůstává původní alokace platná.
 */
bool dynamic_buffer_reserve(DynamicBuffer *buffer, size_t length);
/**
 * @brief Připojí jeden znak a obnoví koncovou nulu.
 *
 * Před zápisem zajistí místo pro přidávaný znak i terminátor. Logickou délku zvýší až po
 * úspěšném zajištění kapacity.
 * @param buffer Inicializovaný cílový buffer.
 * @param c Znak určený k připojení.
 * @return True při úspěchu; false při neplatném bufferu, přetečení nebo nedostatku paměti.
 */
bool dynamic_buffer_append_char(DynamicBuffer *buffer, char c);
/**
 * @brief Připojí řetězec zakončený nulovým znakem.
 *
 * Zjistí délku textu, ověří součet délek a zajistí kapacitu. Překopíruje také koncovou nulu
 * a aktualizuje logickou délku.
 * @param buffer Inicializovaný cílový buffer.
 * @param text Zdrojový řetězec; nesmí být NULL.
 * @return True při úspěchu; false při neplatném vstupu, přetečení nebo selhání rezervace.
 * @warning Zdroj nesmí překrývat cílovou oblast ani odkazovat do bufferu, který může být
 * realokován.
 */
bool dynamic_buffer_append_string(DynamicBuffer *buffer, const char *text);
/**
 * @brief Připojí formátovaný text ze seznamu argumentů.
 *
 * Pomocí kopie va_list nejprve zjistí potřebnou délku a poté provede zápis. Při neshodě
 * délky výsledku obnoví terminátor na původním konci a logickou délku nezmění.
 * @param buffer Inicializovaný cílový buffer.
 * @param format Formátovací řetězec pro vsnprintf().
 * @param args Seznam argumentů odpovídající formátu.
 * @return True při úspěchu; false při neplatném vstupu, chybě formátování, přetečení nebo
 * nedostatku paměti.
 * @warning Formát a textové argumenty nesmějí být zneplatněny realokací cílového bufferu.
 */
bool dynamic_buffer_append_vformat(DynamicBuffer *buffer, const char *format, va_list args);
/**
 * @brief Vymaže obsah při zachování přidělené kapacity.
 *
 * Nastaví délku na nulu a existující datové pole ukončí nulovým znakem. Buffer lze následně
 * znovu naplňovat bez nové počáteční alokace.
 * @param buffer Buffer k vymazání; NULL se ignoruje.
 */
void dynamic_buffer_clear(DynamicBuffer *buffer);
/**
 * @brief Uvolní datové pole a vynuluje strukturu bufferu.
 *
 * Po uvolnění má buffer nulovou délku, kapacitu i ukazatel na data. Opakované volání nad již
 * uvolněným bufferem je bezpečné.
 * @param buffer Buffer k uvolnění; NULL se ignoruje.
 */
void dynamic_buffer_free(DynamicBuffer *buffer);

#endif
