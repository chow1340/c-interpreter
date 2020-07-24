#include <stdlib.h>
#include "memory.h"

//Frees allocation or uses realloc to resize
void* reallocate(void* pointer, size_t oldSize, size_t newSize){
    if(newSize == 0 ){
        free(pointer);
        return NULL;
    }
    void* result = realloc(pointer, newSize);
    // Sometimes realloc returns NULL (ie out of memory);
    if(result == NULL) exit(1);
    return result;
}
