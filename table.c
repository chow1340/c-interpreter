#include <stdlib.h>
#include <string.h>
#include "table.h"
#include "memory.h"
#include "object.h"
#include "value.h"


#define TABLE_MAX_LOAD 0.75

Table table;

void initTable(Table* table){
    table->count = 0;
    table->capacity = 0;
    table->entries = NULL;
}

void freeTable(Table* table){
    FREE_ARRAY(Entry, table->entries, table->capacity);
    initTable(table);
}

static Entry* findEntry(Entry* entries, int capacity, ObjString* key){
    uint32_t index = key-> hash % capacity;
    Entry* tombstone = NULL;
    while(true){
        Entry* entry = &entries[index];

        if(entry->key == NULL) {
            if(IS_NIL(entry->value)) {
                //empty entry
                //Allows us to reuse tombstones for new entries
                return tombstone != NULL ? tombstone: entry;
            } else {
                //Found a tombstone
                if(tombstone == NULL) tombstone = entry;
            }
        } else if(entry->key == key){
            return entry;
        }
        //begin probing
        index = (index+1) % capacity;
    }
}

static void adjustCapacity(Table* table, int capacity){
    Entry* entries = ALLOCATE(Entry, capacity);
    //Create new empty bucket array
    for(int i = 0; i < capacity; i++){
        entries[i].key = NULL;
        entries[i].value = NIL_VAL;
    }

    table->count = 0;

    //Since hash entries are based on capacity/modulos,
    //cannot use realloc, need to rebuild the table
    for(int i = 0; i < table->capacity; i++){
        //Find already existing entries in cur. table
        Entry* entry = &table->entries[i];
        if(entry->key == NULL) continue;

        //Rehash and set to new index position
        Entry* dest = findEntry(entries, capacity, entry->key);
        dest->key = entry->key;
        dest->value = entry->value;
        table->count++;
    }
    //Free memory
    FREE_ARRAY(Entry, table->entries, table->capacity);

    //Store entries in table struct
    table->entries = entries;
    table->capacity = capacity;
}

//adds given pair to table
bool tableSet(Table* table, ObjString* key, Value value) {
    if(table->count + 1 > table->capacity * TABLE_MAX_LOAD){
        int capacity = GROW_CAPACITY(table->capacity);
        adjustCapacity(table, capacity);
    }

    Entry* entry = findEntry(table->entries, table->capacity, key);
    
    bool isNewKey = entry->key == NULL;
    //The count is number of entries + tombstones
    //Count is only incremented everytime there is a NEW entry
    if(isNewKey && IS_NIL(entry->value))table->count++;
    entry->key = key;
    entry->value = value;
    return isNewKey;
}

void tableAddAll(Table* from, Table* to){
    for(int i = 0; i < from->capacity; i++){
        Entry* entry = &from->entries[i];
        if(entry->key != NULL){
            tableSet(to, entry->key, entry->value);
        }
    }
}

ObjString* tableFindString(Table* table, const char* chars, int length, uint32_t hash){
    if(table->count == 0) return NULL;
    uint32_t index = hash % table->capacity;

    while(true){
        Entry* entry = &table->entries[index];
        if(entry->key == NULL){
            //Stop if find an empty non-tombstone entry.
            if(IS_NIL(entry->value)) return NULL;
        } else if (entry->key->length == length &&
                   entry->key->hash == hash &&
                   //If hash  collision,check character by character
                   memcmp(entry->key->chars, chars, length) == 0 ) {
                       return entry->key;
                   }
        index = (index +  1) % table->capacity;
    }
}

bool tableGet(Table* table, ObjString* key, Value* value) {
    if(table->count == 0) return false;
    Entry* entry = findEntry(table->entries, table->capacity,key);
    if(entry->key ==  NULL) return false;
    *value = entry->value;
    return true;
}

bool tableDelete(Table* table, ObjString* key){
    if(table->count == 0 )return false;

    Entry* entry = findEntry(table->entries, table->capacity, key);
    if(entry->key == NULL) return false;

    //Use NULL key and True value to represent a tombstone
    entry->key = NULL;
    entry->value = BOOL_VAL(true);

    return true;
}



