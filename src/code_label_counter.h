/**
 * @par         Implementace překladače imperativního jazyka IFJ26
 * @file        code_label_counter.h
 * @authors     Michal Zednicek <xzednim00>
 * @brief       Implementace počítadla návěští
 */

#ifndef CODE_LABEL_COUNTER_H
#define CODE_LABEL_COUNTER_H

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

/**
 * @brief Struktura pro správu a generování unikátních návěští při zanořených konstrukcích (např. if, while).
 */
typedef struct {
    size_t  cnt;       /**< Celkový počet dosud vygenerovaných návěští. */
    size_t *stack;     /**< Dynamické pole uchovávající ID aktuálně aktivních vnořených návěští. */
    size_t  stack_top; /**< Aktuální počet prvků na zásobníku. */
    size_t  capacity;  /**< Alokovaná kapacita pole zásobníku. */
} LabelCounter;

// Vygeneruje nové unikátní ID návěští, uloží ho na vrchol zásobníku a vrátí ho.
size_t label_push(LabelCounter *counter);

// Vrátí ID návěští na vrcholu zásobníku bez jeho odstranění.
size_t label_top(LabelCounter *counter);

// Odebere a vrátí ID návěští z vrcholu zásobníku.
size_t label_pop(LabelCounter *counter);

#endif // CODE_LABEL_COUNTER_H