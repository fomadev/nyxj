/* Internal helpers shared by core and module sources. */
#ifndef NYXJ_INTERNAL_H
#define NYXJ_INTERNAL_H

#include "../../include/nyxj.h"

#define NYXJ_VFLAG_ARENA 1u /* Value (and its children) live in an arena */

/* Capacity policy for arrays built by the parser or the builder API. */
static inline int nyxj_array_capacity_for(int count) {
    int cap = 4;
    while (cap < count) cap *= 2;
    return cap;
}

char* nyxj_strdup(const char* s);
char* nyxj_strndup(const char* s, size_t len);

#endif
