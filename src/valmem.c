#include <string.h>
#include "valmem.h"

int valmem_check(const char *name, data_type receiver, unsigned *operation, data_type *result) {
    if (strcmp(name, "toString") == 0) {
        *operation = VALMEM_STRING;
        *result    = type_string;
        return receiver == type_unknown || receiver == type_any || receiver == type_bool ? 6 : 0;
    }
    if (strcmp(name, "toInt") == 0) {
        *operation = VALMEM_INT;
        *result    = type_int;
        return receiver == type_double ? 0 : 6;
    }
    if (strcmp(name, "floor") == 0) {
        *operation = VALMEM_FLOOR;
        *result    = type_double;
        return receiver == type_double ? 0 : 6;
    }
    if (strcmp(name, "length") == 0) {
        *operation = VALMEM_LENGTH;
        *result    = type_int;
        return receiver == type_string ? 0 : 6;
    }
    return 3;
}
