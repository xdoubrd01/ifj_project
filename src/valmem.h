// IFJ26: VALMEM member type checks.
#ifndef VALMEM_H
#define VALMEM_H

#include "symtable.h"

typedef enum { VALMEM_STRING, VALMEM_INT, VALMEM_FLOOR, VALMEM_LENGTH } ValMember;
/* Returns 3 for an unknown member, 6 for an incompatible receiver. */
int valmem_check(const char *name, data_type receiver, unsigned *operation, data_type *result);
#endif
