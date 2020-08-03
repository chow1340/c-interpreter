#ifndef cInterp_object_h
#define cInterp_object_h
#include "common.h"
#include "value.h"

#define OBJ_TYPE(value) (AS_OBJ(value)->type)
#define IS_STRING(value) (isObjType(value, OBJ_STRING))
#define AS_STRING(value) ((ObjString*)AS_OBJ(value)) //Return as ObjString* pointer
#define AS_CSTRING(value) (((ObjString*)AS_OBJ(value))->chars) // return as chars array

typedef enum {
    OBJ_STRING,
} ObjType;

struct sObj{
    ObjType type;
    struct sObj* next;
};

struct sObjString {
    Obj obj;
    int length;
    char* chars;
};

//Not a macro because it would evaluate twice.
//I.E if isObjType(pop()) would pop  twice
static inline bool isObjType(Value value, ObjType type) {
  return IS_OBJ(value) && AS_OBJ(value)->type == type;
}

ObjString* copyString(const char* chars, int length);
ObjString* takeString(char* chars, int length);
#endif