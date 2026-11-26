/**
 * @project Implementace překladače imperativního jazyka IFJ26
 * @file dynamic_buffer.c
 * @author Denis Dubrava (xdoubrd01)
 * @brief Správa dynamického textového bufferu a připojování znaků,
 * řetězců a formátovaného textu. Podle potřeby zvětšuje kapacitu
 * a udržuje ukončení obsahu nulovým znakem. Poskytuje také vymazání
 * obsahu bez uvolnění kapacity a úplné uvolnění bufferu.
 * @note AI byla využita k lepšímu znění a obsahu komentářů.
 */

#include "dynamic_buffer.h"
#include "array_utils.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief Inicializuje prázdný dynamický buffer bez alokace.
 *
 * Vynuluje ukazatel na data, délku i kapacitu. Funkce neuvolňuje předchozí obsah a slouží
 * pro dosud neinicializovaný nebo již uvolněný objekt.
 * @param buffer Platný ukazatel na strukturu bufferu; nesmí být NULL.
 * @warning Nepoužívejte k přepsání živého bufferu bez předchozího uvolnění.
 */
void dynamic_buffer_init(DynamicBuffer *buffer) { *buffer = (DynamicBuffer){0}; }

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
bool dynamic_buffer_reserve(DynamicBuffer *buffer, size_t length) {
    if (buffer == NULL || length == SIZE_MAX) {
        return false;
    }

    // Kapacita musí zahrnovat i koncovou nulu, proto je porovnání ostré.
    if (length < buffer->capacity) {
        return true;
    }

    // Pro terminátor je potřeba ještě jeden bajt nad požadovanou délku.
    size_t capacity;
    if (!array_next_capacity(buffer->capacity, length + 1, sizeof(char), 32, &capacity)) {
        return false;
    }

    // Původní ukazatel přepíšeme až po úspěchu, aby jej chyba nezneplatnila.
    char *data = realloc(buffer->data, capacity);
    if (data == NULL) {
        return false;
    }

    buffer->data     = data;
    buffer->capacity = capacity;
    // Po přesunu pole obnovíme terminátor na dosavadním logickém konci obsahu.
    buffer->data[buffer->length] = '\0';
    return true;
}

/**
 * @brief Připojí jeden znak a obnoví koncovou nulu.
 *
 * Před zápisem zajistí místo pro přidávaný znak i terminátor. Logickou délku zvýší až po
 * úspěšném zajištění kapacity.
 * @param buffer Inicializovaný cílový buffer.
 * @param c Znak určený k připojení.
 * @return True při úspěchu; false při neplatném bufferu, přetečení nebo nedostatku paměti.
 */
bool dynamic_buffer_append_char(DynamicBuffer *buffer, char c) {
    if (buffer == NULL || buffer->length == SIZE_MAX ||
        !dynamic_buffer_reserve(buffer, buffer->length + 1)) {
        return false;
    }

    // Znak přidáme na konec a délku zvýšíme právě o jeden.
    buffer->data[buffer->length++] = c;
    buffer->data[buffer->length]   = '\0';
    return true;
}

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
bool dynamic_buffer_append_string(DynamicBuffer *buffer, const char *text) {
    if (buffer == NULL || text == NULL) {
        return false;
    }

    // Délku vstupu zjistíme před výpočtem součtu s dosavadním obsahem.
    size_t length = strlen(text);
    if (length > SIZE_MAX - buffer->length ||
        !dynamic_buffer_reserve(buffer, buffer->length + length)) {
        return false;
    }

    // Kopírujeme i terminátor; předchozí obsah bufferu zůstává zachován.
    memcpy(buffer->data + buffer->length, text, length + 1);
    buffer->length += length;
    return true;
}

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
bool dynamic_buffer_append_vformat(DynamicBuffer *buffer, const char *format, va_list args) {
    if (buffer == NULL || format == NULL) {
        return false;
    }

    // Průchod seznamem argumentů jej spotřebovává; pro každý průchod vytvoříme kopii.
    va_list copy;
    va_copy(copy, args);
    // První formátovací průchod pouze změří délku výsledku bez koncové nuly.
    int length = vsnprintf(NULL, 0, format, copy);
    va_end(copy);

    if (length < 0 || (size_t)length > SIZE_MAX - buffer->length ||
        !dynamic_buffer_reserve(buffer, buffer->length + (size_t)length)) {
        return false;
    }

    va_copy(copy, args);
    int written = vsnprintf(buffer->data + buffer->length, (size_t)length + 1, format, copy);
    va_end(copy);

    // Neshoda druhého průchodu nesmí posunout logický konec původního obsahu.
    if (written != length) {
        buffer->data[buffer->length] = '\0';
        return false;
    }

    buffer->length += (size_t)length;
    return true;
}

/**
 * @brief Vymaže obsah při zachování přidělené kapacity.
 *
 * Nastaví délku na nulu a existující datové pole ukončí nulovým znakem. Buffer lze následně
 * znovu naplňovat bez nové počáteční alokace.
 * @param buffer Buffer k vymazání; NULL se ignoruje.
 */
void dynamic_buffer_clear(DynamicBuffer *buffer) {
    if (buffer != NULL) {
        // Vymazání nevrací paměť systému; rezervovaná kapacita zůstává pro další použití.
        buffer->length = 0;
        if (buffer->data != NULL) {
            buffer->data[0] = '\0';
        }
    }
}

/**
 * @brief Uvolní datové pole a vynuluje strukturu bufferu.
 *
 * Po uvolnění má buffer nulovou délku, kapacitu i ukazatel na data. Opakované volání nad již
 * uvolněným bufferem je bezpečné.
 * @param buffer Buffer k uvolnění; NULL se ignoruje.
 */
void dynamic_buffer_free(DynamicBuffer *buffer) {
    if (buffer != NULL) {
        // Uvolníme vlastněné pole a následně odstraníme i neplatný ukazatel ze struktury.
        free(buffer->data);
        *buffer = (DynamicBuffer){0};
    }
}