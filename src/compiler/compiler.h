#ifndef AHHHHH_COMPILER_H
#define AHHHHH_COMPILER_H

#include "parser/parser.h"
#include "vm/chunk.h"
#include "compiler/type.h"

#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define MAX_STRINGS 256

// lokale variable
typedef struct {
    char name[64];      // name
    int depth;          // scope tiefe
    Type *type;         // typ
} Local;

// compiler status
typedef struct Compiler {
    struct Compiler *enclosing;      // umschliessender compiler
    ObjFunction *function;           // compilierte funktion
    
    Local locals[1024];              // lokale variablen
    int local_count;                 // anzahl lokale
    int scope_depth;                 // scope tiefe

    char **params;                   // parameter
    int param_count;                 // anzahl
    Type **param_types;              // parameter typen
    
    char strings[MAX_STRINGS][256];  // strings
    uint8_t string_count;            // string anzahl
    void *current_loop;              // schleifen zeiger
    
    TypeTable *type_table;           // typ tabelle
    Type *return_type;               // rueckgabetyp
} Compiler;

// compiler funktionen
void compiler_init(Compiler *compiler, struct Compiler *enclosing);
void compiler_free(Compiler *compiler);
int compiler_compile(Compiler *compiler, const char *source);
int compiler_compile_ast(Compiler *compiler, AstProgram *program);
Chunk *compiler_get_chunk(Compiler *compiler);

#endif