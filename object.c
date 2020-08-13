#include <stdio.h>
#include <string.h>
#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"
#include "vm.h"

#define ALLOCATE_OBJ(type, objectType) \
    (type*)allocateObject(sizeof(type), objectType)

//Allocates based on given size of the heap
static Obj* allocateObject(size_t size, ObjType type) {
    Obj* object = (Obj*)(reallocate(NULL, 0, size));
    object->type = type;
    object->next = vm.objects;
    vm.objects = object;
    return object;
}

ObjClosure* newClosure(ObjFunction* function){
    ObjClosure* closure = ALLOCATE_OBJ(ObjClosure, OBJ_CLOSURE);
    closure->function = function;
    return closure;
}

//Creates new object on heap and initialiszes it (similar to constructors)
static ObjString* allocateString(char* chars, int length, uint32_t hash) {
    //Init object so vm knows type of object
    ObjString* string = ALLOCATE_OBJ(ObjString, OBJ_STRING);
    string->length = length;
    string->chars = chars;
    string->hash = hash;
    tableSet(&vm.strings,  string, NIL_VAL);
    return string;
}

static uint32_t hashString(const char* key, int length){
    //FNV-1a hash method
    uint32_t hash = 2166136261u;
    for(int i = 0; i < length; i++){
        hash ^= key[i];
        hash *= 16777619;
    }

    return hash;
}

ObjString* copyString(const char* chars, int length){
    uint32_t hash = hashString(chars, length);
    ObjString* interned = tableFindString(&vm.strings, chars, length, hash);
    if(interned != NULL) return interned;
    char* heapChars = ALLOCATE(char, length+1);
    //Copy char from chars to heapChars in memory
    memcpy(heapChars, chars, length);
    //Null terminator to end string
    heapChars[length] = '\0';

    return allocateString(heapChars, length, hash); 
}

ObjString* takeString(char* chars, int length){
    uint32_t hash = hashString(chars, length);
    //If string already exists, free up memory for the string passed in
    //Since we do not need the duplicate string
    ObjString* interned = tableFindString(&vm.strings, chars, length, hash);
    if(interned != NULL) {
        FREE_ARRAY(char, chars, length+1);
        return interned;
    }
    return allocateString(chars, length, hash);
}

ObjFunction* newFunction() {
    ObjFunction* function = ALLOCATE_OBJ(ObjFunction, OBJ_FUNCTION);
    function->arity = 0;
    function->name = NULL;
    function->upvalueCount = 0;
    initChunk(&function->chunk);
    return function;
}

//Constructor
ObjNative* newNative(NativeFn function){
    ObjNative* native = ALLOCATE_OBJ(ObjNative, OBJ_NATIVE);
    native->function = function;
    return native;
}

