#ifndef AHHHHH_COMPILER_H
#define AHHHHH_COMPILER_H

#include "parser/parser.h"
#include "vm/chunk.h"
#include "compiler/type.h"

#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define MAX_STRINGS 256

typedef struct {
    char name[64];
    int depth;
    Type *type;
} Local;

typedef struct Compiler {
    struct Compiler *enclosing;
    ObjFunction *function;
    
    Local locals[1024];
    int local_count;
    int scope_depth;

    char **params;
    int param_count;
    Type **param_types;
    
    char strings[MAX_STRINGS][256];
    uint8_t string_count;
    void *current_loop;
    
    TypeTable *type_table;
    Type *return_type;
} Compiler;

void compiler_init(Compiler *compiler, struct Compiler *enclosing);
void compiler_free(Compiler *compiler);
int compiler_compile(Compiler *compiler, const char *source);
int compiler_compile_ast(Compiler *compiler, AstProgram *program);
Chunk *compiler_get_chunk(Compiler *compiler);

#endif