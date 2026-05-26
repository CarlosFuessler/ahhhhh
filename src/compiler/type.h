#ifndef AHHHHH_TYPE_H
#define AHHHHH_TYPE_H

#include <stddef.h>
#include <stdbool.h>

typedef enum {
    TYPE_UNKNOWN,
    TYPE_NUMBER,
    TYPE_STRING,
    TYPE_BOOL,
    TYPE_NULL,
    TYPE_VOID,
    TYPE_ARRAY,
    TYPE_STRUCT,
    TYPE_FUNCTION,
    TYPE_VARIABLE,
} TypeKind;

typedef struct Type Type;

struct Type {
    TypeKind kind;
    const char *name; // For structs and primitives
    
    // For arrays
    struct Type *element_type;
    
    // For structs
    struct {
        char **names;
        struct Type **types;
        size_t count;
    } struct_info;
    
    // For functions
    struct {
        struct Type **params;
        size_t param_count;
        struct Type *return_type;
    } fn_info;
};

// Type Registry / Table
typedef struct TypeTable {
    Type **types;
    size_t count;
    size_t capacity;
} TypeTable;

void type_table_init(TypeTable *table);
void type_table_free(TypeTable *table);
void type_table_register(TypeTable *table, Type *type);
Type *type_table_find(TypeTable *table, const char *name);

// Built-in types
extern Type type_number;
extern Type type_string;
extern Type type_bool;
extern Type type_null;
extern Type type_void;
extern Type type_unknown;

// Helpers
bool types_equal(Type *a, Type *b);
Type *parse_type_name(TypeTable *table, const char *name);

#endif