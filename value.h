#ifndef cInterp_value_h
#define cInterp_value_h
#include "common.h"

// typedef double Value;



typedef enum{
    VAL_BOOL,
    VAL_NIL,
    VAL_NUMBER,
} ValueType;

typedef struct {
    ValueType type;
    union {
        bool boolean;
        double number;
    } as;
} Value;

typedef struct {
    int capacity;
    int count;
    Value* values;
} ValueArray;
//takes a value of C type and produces a Value with 
//the correct type tag and underlying value
#define BOOL_VAL(value) ((Value){VAL_BOOL, {.boolean = value}})
#define NIL_VAL ((Value){VAL_NIL, {.number = 0}})
#define NUMBER_VAL(value) ((Value) {VAL_NUMBER, {.number = value}})

//unpack and give the C value
#define AS_BOOL(value) ((value).as.boolean)
#define AS_NUM(value) ((value).as.number)

//Used to safeguard the AS_ macros so types do not get mixed up
#define IS_BOOL(value) ((value).type == VAL_BOOL)
#define IS_NIL(value) ((value).type == VAL_NIL)
#define IS_NUMBER(value) ((value).type == VAL_NUMBER)

void initValueArray(ValueArray* array);
void writeValueArray(ValueArray* array, Value value);
void freeValueArray(ValueArray* array);
void printValue(Value value);
bool valuesEqual(Value a, Value b);
#endif