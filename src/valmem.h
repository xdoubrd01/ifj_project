/**
 * @par Projekt
 * Implementace překladače imperativního jazyka IFJ26
 * @file valmem.h
 * @author Denis Doubrava (xdoubrd01), Jakub Grásgruber (xgrasgj00), Eliáš Oharek (xoharee00),
 * Michal Zedníček (xzednim00)
 * @brief Rozhraní sémantických kontrol členských přístupů rozšíření VALMEM.
 * Definuje označení podporovaných operací nad hodnotami.
 * Deklaruje ověření jména členu a jeho přípustnosti pro datový typ příjemce.
 * Předává analyzátoru označení operace a datový typ výsledku.
 * @note AI byla využita k lepšímu znění a obsahu komentářů.
 */
#ifndef VALMEM_H
#define VALMEM_H

#include "symtable.h"

/**
 * @brief Označení podporovaných členů předávané do stromu výrazu a generátoru.
 *
 * VALMEM_STRING označuje toString, VALMEM_INT toInt, VALMEM_FLOOR floor
 * a VALMEM_LENGTH length. Číselné hodnoty určuje pořadí položek výčtu.
 */
typedef enum { VALMEM_STRING, VALMEM_INT, VALMEM_FLOOR, VALMEM_LENGTH } ValMember;

/**
 * @brief Ověří členský přístup podle jména členu a datového typu příjemce.
 * @param name Jméno členu ukončené nulovým znakem; nesmí být NULL.
 * @param receiver Datový typ hodnoty, ke které se člen vztahuje.
 * @param operation Platný výstupní ukazatel na označení operace z výčtu ValMember.
 * @param result Platný výstupní ukazatel na datový typ výsledku.
 * @return 0 při úspěchu; 3 pro neznámý člen; 6 pro nekompatibilní typ příjemce.
 * @note Pro známý člen se výstupy nastaví i při chybě typu; používat se mají až při úspěchu.
 * @note Při neznámém členu se výstupní hodnoty nemění.
 */
int valmem_check(const char *name, data_type receiver, unsigned *operation, data_type *result);
#endif
