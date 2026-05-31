#ifndef AHHHHH_TYPE_H
#define AHHHHH_TYPE_H

#include <stddef.h>
#include <stdbool.h>

// alle typenarten
typedef enum {
    TYPE_UNKNOWN,  // unbekannt
    TYPE_NUMBER,   // zahl
    TYPE_STRING,   // string
    TYPE_BOOL,     // bool
    TYPE_NULL,     // null
    TYPE_VOID,     // void
    TYPE_ARRAY,    // array
    TYPE_STRUCT,   // struct
    TYPE_FUNCTION, // funktion
    TYPE_VARIABLE, // variable
} TypeKind;

typedef struct Type Type;

// typen struktur
struct Type {
    TypeKind kind;     // art
    const char *name;  // name
    
    // fuer arrays
    struct Type *element_type; // element typ
    
    // fuer strukturen
    struct {
        char **names;          // feldnamen
        struct Type **types;   // feldtypen
        size_t count;          // anzahl
    } struct_info;
    
    // fuer funktionen
    struct {
        struct Type **params;     // param typen
        size_t param_count;       // param anzahl
        struct Type *return_type; // rueckgabetyp
    } fn_info;
};

// typen tabelle
typedef struct TypeTable {
    Type **types;     // liste von typen
    size_t count;     // belegte eintraege
    size_t capacity;  // kapazitaet
} TypeTable;

// tabellen methoden
void type_table_init(TypeTable *table);
void type_table_free(TypeTable *table);
void type_table_register(TypeTable *table, Type *type);
Type *type_table_find(TypeTable *table, const char *name);

// eingebaute typen
extern Type type_number;
extern Type type_string;
extern Type type_bool;
extern Type type_null;
extern Type type_void;
extern Type type_unknown;

// hilfen
bool types_equal(Type *a, Type *b);
Type *parse_type_name(TypeTable *table, const char *name);

#endif