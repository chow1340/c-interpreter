#ifndef cInterp_memory_h
#define cInterp_memory_h

#include "common.h"

#define ALLOCATE(type, count) \
    (type*)reallocate(NULL, 0, sizeof(type) * (count))

#define GROW_CAPACITY(capacity) \
    ((capacity) < 8 ? 8 : (capacity) * 2)

// Macro to cast the void* generic pointer to the original type
// Reallocate takes care of changing the size

#define GROW_ARRAY(type, pointer, oldCount, newCount) \
    (type*)reallocate(pointer, sizeof(type) * (oldCount), \
        sizeof(type) * (newCount))

#define FREE_ARRAY(type, pointer, oldCount) \
    reallocate(pointer, oldCount, 0)

#define FREE(type, pointer) reallocate(pointer, sizeof(type), 0);

void* reallocate(void* pointer, size_t oldSize, size_t newSize);
void freeObjects();

#endif