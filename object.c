#include <stdio.h>
#include <string.h>
#include "memory.h"
#include "object.h"
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

//Creates new object on heap and initialiszes it (similar to constructors)
static ObjString* allocateString(char* chars, int length) {
    ObjString* string = ALLOCATE_OBJ(ObjString, OBJ_STRING);
    string->length = length;
    string->chars = chars;

    return string;
}


ObjString* copyString(const char* chars, int length){
    char* heapChars = ALLOCATE(char, length+1);
    //Copy char from chars to heapChars in memory
    memcpy(heapChars, chars, length);
    //Null terminator to end string
    heapChars[length] = '\0';

    return allocateString(heapChars, length); 
}

ObjString* takeString(char* chars, int length){
    return allocateString(chars, length);
}

