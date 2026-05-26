#ifndef AHHHHH_VM_H
#define AHHHHH_VM_H

#include "vm/chunk.h"
#include "vm/table.h"

#include <stdint.h>
#include <stdio.h>

#define STACK_MAX 1024
#define GLOBALS_MAX 256
#define NATIVE_FUNCS_MAX 128
#define FRAMES_MAX 64

typedef struct {
    Value stack[STACK_MAX];
    Value *stack_top;
} Stack;

typedef struct {
    Table table;
} Globals;

typedef struct {
    ObjFunction *function;
    uint8_t *ip;
    Value *slots;
    int local_count;
} CallFrame;

typedef struct {
    const char *name;
    NativeFn fn;
} NativeFunc;

struct VM {
    Stack stack;
    Globals globals;
    CallFrame frames[FRAMES_MAX];
    int frame_count;
    NativeFunc natives[NATIVE_FUNCS_MAX];
    int native_count;
    
    Table strings;
    Obj *objects;

    int debug_trace;
    int gc_suspend;
};

typedef enum {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR,
} InterpretResult;

void vm_init(VM *vm);
void vm_free(VM *vm);

InterpretResult vm_interpret(VM *vm, Chunk *chunk);
int vm_define_global(VM *vm, const char *name, Value value);
int vm_define_native(VM *vm, const char *name, NativeFn fn);
NativeFn vm_lookup_native(VM *vm, const char *name);

ObjString *copy_string(VM *vm, const char *chars, int length);
ObjArray *new_array(VM *vm, size_t count);
#endif