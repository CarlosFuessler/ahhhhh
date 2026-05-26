#ifndef AHHHHH_TABLE_H
#define AHHHHH_TABLE_H

#include "vm/chunk.h"

#include <stdint.h>
#include <stdlib.h>

typedef struct {
    ObjString *key;
    Value value;
} Entry;

struct Table {
    int count;
    int capacity;
    Entry *entries;
};

void table_init(Table *table);
void table_free(Table *table);
int table_set(Table *table, ObjString *key, Value value);
int table_get(Table *table, ObjString *key, Value *value);
int table_delete(Table *table, ObjString *key);
void table_add_all(Table *from, Table *to);
ObjString *table_find_string(Table *table, const char *chars, int length, uint32_t hash);

#endif
