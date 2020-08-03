#include <stdlib.h>
#include "memory.h"
#include "vm.h"

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

static void freeObject(Obj* object){
    switch(object->type){
        case OBJ_STRING: {
            ObjString* string = (ObjString*)object;
            //Free object itself
            FREE_ARRAY(char, string->chars, string->length + 1); 
            //Free object type 
            FREE(ObjString, object);
        }
    }
}

void freeObjects(){
    Obj* object = vm.objects;
    while(object != NULL){
        Obj* next = object->next;
        freeObject(object);
        object = next;
    }
}
