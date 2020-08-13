#include <stdio.h>
#include <string.h>
#include "memory.h"
#include "value.h"
#include "object.h"

void initValueArray(ValueArray* array){
    array->values = NULL;
    array->capacity = 0;
    array->count = 0;
}

void writeValueArray(ValueArray* array, Value value){
    if(array->capacity < array->count+1){
        int oldCapacity = array->capacity;
        array->capacity = GROW_CAPACITY(oldCapacity);
        array->values = GROW_ARRAY(Value, array->values, oldCapacity, array->capacity);
        
    }

    array->values[array->count] = value;
    array->count++;
}

void freeValueArray(ValueArray* array){
    FREE_ARRAY(Value, array->values, array->count);
    initValueArray(array);
}

static void printFunction(ObjFunction* function) {
    if(function->name == NULL) {
        printf("<script>");
        return;
    }
    printf("<fn %s>", function->name->chars);
}

void printObject(Value value){
    switch(OBJ_TYPE(value)){
        case OBJ_STRING: printf("%s", AS_CSTRING(value)); break;
        case OBJ_FUNCTION:
            printFunction(AS_FUNCTION(value));
            break;
        case OBJ_NATIVE:
            printf("<native fn>");
            break;
        case OBJ_CLOSURE:
            printFunction(AS_CLOSURE(value)->function);
            break;
    }
}

void printValue(Value value){
    switch(value.type){
        case VAL_BOOL: printf(AS_BOOL(value) ? "true" : "false");break;
        case VAL_NIL: printf("nil"); break;
        case VAL_NUMBER: printf("%g",AS_NUM(value)); break;
        case VAL_OBJ: printObject(value);
    }
}

bool valuesEqual(Value a, Value b){
    if(a.type != b.type) return false;
    switch (a.type){
        case VAL_BOOL: return AS_BOOL(a) == AS_BOOL(b);
        case VAL_NIL: return true;
        case VAL_NUMBER: return AS_NUM(a) == AS_NUM(b);
        case VAL_OBJ: return AS_OBJ(a) == AS_OBJ(b);
    }
}