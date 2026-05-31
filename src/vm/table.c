#include "vm/table.h"
#include <string.h>

#define TABLE_MAX_LOAD 0.75

// init eine leere hash-tabelle
void table_init(Table *table) {
    table->count = 0;
    table->capacity = 0;
    table->entries = NULL;
}

// speicher frei geben
void table_free(Table *table) {
    free(table->entries);
    table_init(table);
}

// sucht einen eintrag in einer tabelle 
static Entry *find_entry(Entry *entries, int capacity, ObjString *key) {
    uint32_t index = key->hash % capacity;
    Entry *tombstone = NULL;

    for (;;) {
        Entry *entry = &entries[index];
        if (entry->key == NULL) {
            if (entry->value.kind == VALUE_NULL) {
                // leerer eintrag = tombstone
                return tombstone != NULL ? tombstone : entry;
            } else {
                // grabstein merken und weiter
                if (tombstone == NULL) tombstone = entry;
            }
        } else if (entry->key == key) {
            return entry;
        }

        // lineare sondierung 
        index = (index + 1) % capacity;
    }
}

// passt die kapazität der tabelle an und rehasht alle einträge
static void adjust_capacity(Table *table, int capacity) {
    Entry *entries = malloc(sizeof(Entry) * capacity);
    for (int i = 0; i < capacity; i++) {
        entries[i].key = NULL;
        entries[i].value = (Value){VALUE_NULL, {0}};
    }

    table->count = 0;
    for (int i = 0; i < table->capacity; i++) {
        Entry *entry = &table->entries[i];
        if (entry->key == NULL) continue;

        Entry *dest = find_entry(entries, capacity, entry->key);
        dest->key = entry->key;
        dest->value = entry->value;
        table->count++;
    }

    free(table->entries);
    table->entries = entries;
    table->capacity = capacity;
}

// fügt ein schlüsselwert paar ein
int table_set(Table *table, ObjString *key, Value value) {
    if (table->count + 1 > table->capacity * TABLE_MAX_LOAD) {
        int capacity = table->capacity < 8 ? 8 : table->capacity * 2;
        adjust_capacity(table, capacity);
    }

    Entry *entry = find_entry(table->entries, table->capacity, key);
    int is_new_key = entry->key == NULL;
    if (is_new_key && entry->value.kind == VALUE_NULL) table->count++;

    entry->key = key;
    entry->value = value;
    return is_new_key;
}

// holt den wert zu einem schlüssel aus der tabelle
int table_get(Table *table, ObjString *key, Value *value) {
    if (table->count == 0) return 0;

    Entry *entry = find_entry(table->entries, table->capacity, key);
    if (entry->key == NULL) return 0;

    *value = entry->value;
    return 1;
}

// löscht einen schlüssel aus der tabelle
int table_delete(Table *table, ObjString *key) {
    if (table->count == 0) return 0;

    Entry *entry = find_entry(table->entries, table->capacity, key);
    if (entry->key == NULL) return 0;

    // Einen Grabstein (Tombstone) in den Eintrag setzen.
    entry->key = NULL;
    entry->value = (Value){VALUE_BOOL, {.boolean = 1}};
    return 1;
}

// kopiert einträge
void table_add_all(Table *from, Table *to) {
    for (int i = 0; i < from->capacity; i++) {
        Entry *entry = &from->entries[i];
        if (entry->key != NULL) {
            table_set(to, entry->key, entry->value);
        }
    }
}

// sucht eine zeichenkette in der tabelle
ObjString *table_find_string(Table *table, const char *chars, int length, uint32_t hash) {
    if (table->count == 0) return NULL;

    uint32_t index = hash % table->capacity;
    for (;;) {
        Entry *entry = &table->entries[index];
        if (entry->key == NULL) {
            if (entry->value.kind == VALUE_NULL) return NULL;
        } else if (strlen(entry->key->chars) == (size_t)length &&
                   entry->key->hash == hash &&
                   memcmp(entry->key->chars, chars, length) == 0) {
            return entry->key;
        }

        index = (index + 1) % table->capacity;
    }
}
