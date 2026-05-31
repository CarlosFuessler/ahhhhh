#include "compiler/type.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

Type type_number = {TYPE_NUMBER, "f64", NULL, {NULL, NULL, 0}, {NULL, 0, NULL}};
Type type_string = {TYPE_STRING, "string", NULL, {NULL, NULL, 0}, {NULL, 0, NULL}};
Type type_bool   = {TYPE_BOOL, "bool", NULL, {NULL, NULL, 0}, {NULL, 0, NULL}};
Type type_null   = {TYPE_NULL, "null", NULL, {NULL, NULL, 0}, {NULL, 0, NULL}};
Type type_void   = {TYPE_VOID, "void", NULL, {NULL, NULL, 0}, {NULL, 0, NULL}};
Type type_unknown = {TYPE_UNKNOWN, "unknown", NULL, {NULL, NULL, 0}, {NULL, 0, NULL}};

void type_table_init(TypeTable *table) {
    table->types = NULL;
    table->count = 0;
    table->capacity = 0;

    // eingebaute registrieren
    type_table_register(table, &type_number);
    type_table_register(table, &type_string);
    type_table_register(table, &type_bool);
    type_table_register(table, &type_null);
    type_table_register(table, &type_void);
    type_table_register(table, &type_unknown);
}

void type_table_free(TypeTable *table) {
    // statische typen ignorieren, benutzerdefinierte freigeben
    for (size_t i = 0; i < table->count; i++) {
        Type *t = table->types[i];
        if (t == &type_number || t == &type_string || t == &type_bool || t == &type_null || t == &type_void || t == &type_unknown) {
            continue;
        }
        if (t->kind == TYPE_FUNCTION) {
            free(t->fn_info.params);
            free((void*)t->name);
        } else if (t->kind == TYPE_ARRAY) {
            free((void*)t->name);
        } else if (t->kind == TYPE_STRUCT) {
            for (size_t j = 0; j < t->struct_info.count; j++) {
                free(t->struct_info.names[j]);
            }
            free(t->struct_info.names);
            free(t->struct_info.types);
            free((void*)t->name);
        }
        free(t);
    }
    free(table->types);
}

void type_table_register(TypeTable *table, Type *type) {
    if (type == NULL || type->name == NULL) return;
    
    // duplikate pruefen
    for (size_t i = 0; i < table->count; i++) {
        if (table->types[i]->name && strcmp(table->types[i]->name, type->name) == 0) {
            // vorwaertsdeklaration
            if (table->types[i]->kind == TYPE_VARIABLE || table->types[i]->kind == TYPE_FUNCTION) {
                // erstes behalten
                return; 
            }
            return;
        }
    }

    if (table->count >= table->capacity) {
        table->capacity = table->capacity == 0 ? 8 : table->capacity * 2;
        table->types = realloc(table->types, sizeof(Type *) * table->capacity);
    }
    table->types[table->count++] = type;
}

Type *type_table_find(TypeTable *table, const char *name) {
    if (name == NULL) return NULL;
    for (size_t i = 0; i < table->count; i++) {
        if (strcmp(table->types[i]->name, name) == 0) {
            return table->types[i];
        }
    }
    // aliase verarbeiten
    if (strcmp(name, "u32") == 0 || strcmp(name, "i32") == 0 || strcmp(name, "u8") == 0 || strcmp(name, "f32") == 0) {
        return &type_number;
    }
    return NULL;
}

bool types_equal(Type *a, Type *b) {
    if (a == b) return true;
    if (a == NULL || b == NULL) return false;
    if (a->kind != b->kind) return false;
    
    if (a->kind == TYPE_STRUCT) {
        return strcmp(a->name, b->name) == 0;
    }
    
    if (a->kind == TYPE_ARRAY) {
        return types_equal(a->element_type, b->element_type);
    }
    
    return true;
}

Type *parse_type_name(TypeTable *table, const char *name) {
    if (name == NULL) return NULL;

    // existenz pruefen
    Type *existing = type_table_find(table, name);
    if (existing) return existing;

    // array typen parsen
    if (name[0] == '[') {
        const char *closing = strchr(name, ']');
        if (closing) {
            Type *element_type = parse_type_name(table, closing + 1);
            if (element_type) {
                Type *array_type = malloc(sizeof(Type));
                array_type->kind = TYPE_ARRAY;
                array_type->name = strdup(name);
                array_type->element_type = element_type;
                type_table_register(table, array_type);
                return array_type;
            }
        }
    }

    return NULL;
}