#ifndef AHHHHH_CHUNK_H
#define AHHHHH_CHUNK_H

#include "buffer/buffer.h"
#include "vm/opcodes.h"

#include <stdint.h>
#include <stdlib.h>

struct VM;
typedef struct VM VM;

typedef struct Value Value;

// nativer zeiger für c funktionen
typedef Value (*NativeFn)(VM *vm, int arg_count, Value *args);

// upvalue-struktur für variabeln außerhalb des scopes
typedef struct Upvalue Upvalue;
struct Upvalue {
  int is_local;
  int index;
  Value *value; // zeiger auf den eigentlichen wert
  Upvalue *next;
};

// verscheiden objekte
typedef enum {
  OBJ_STRING,
  OBJ_OBJECT,
  OBJ_ARRAY,
  OBJ_FUNCTION,
  OBJ_NATIVE,
  OBJ_UPVALUE,
  OBJ_CLOSURE,
} ObjType;

// basis struktur für objekte für garbage collector
typedef struct Obj {
  ObjType type;
  int is_marked;
  struct Obj *next;
} Obj;

// string auf heap
typedef struct ObjString {
  Obj obj;
  uint32_t hash;     // hash für schnelle vergleiche
  const char *chars; //
} ObjString;

typedef struct Table Table;

typedef struct ObjObject {
  Obj obj;
  Table *fields;
} ObjObject;

typedef struct Chunk Chunk;

// compilierte funktion
typedef struct {
  Obj obj;
  Chunk *chunk;
  int arity;
  Upvalue *upvalues;
  int upvalue_count;
} ObjFunction;

// native c funktionen
typedef struct {
  Obj obj;
  NativeFn function;
} ObjNative;

typedef struct {
  Obj obj;
  ObjFunction *function;
  Upvalue **upvalues;
  int upvalue_count;
} ObjClosure;

// dynamisches array
typedef struct {
  Obj obj;
  Value *elements;
  size_t capacity;
  size_t count;
} ObjArray;

// wert
struct Value {
  ValueKind kind;
  union {
    double number;
    int boolean;
    Obj *obj;
  } as;
};

// makros
// für typ überprüfung
#define IS_BOOL(value) ((value).kind == VALUE_BOOL)
#define IS_NULL(value) ((value).kind == VALUE_NULL)
#define IS_NUMBER(value) ((value).kind == VALUE_NUMBER)
#define IS_OBJ(value)                                                          \
  ((value).kind == VALUE_OBJECT || (value).kind == VALUE_STRING ||             \
   (value).kind == VALUE_FUNCTION || (value).kind == VALUE_ARRAY ||            \
   (value).kind == VALUE_NATIVE)

// typ kovertierung
#define AS_BOOL(value) ((value).as.boolean)
#define AS_NUMBER(value) ((value).as.number)
#define AS_OBJ(value) ((value).as.obj)

#define OBJ_TYPE(value) (AS_OBJ(value)->type)

#define IS_STRING(value) (is_obj_type(value, OBJ_STRING))
#define IS_OBJECT(value) (is_obj_type(value, OBJ_OBJECT))
#define IS_FUNCTION(value) (is_obj_type(value, OBJ_FUNCTION))
#define IS_ARRAY(value) (is_obj_type(value, OBJ_ARRAY))
#define IS_NATIVE(value) (is_obj_type(value, OBJ_NATIVE))

static inline int is_obj_type(Value value, ObjType type) {
  return IS_OBJ(value) && AS_OBJ(value)->type == type;
}

#define AS_STRING(value) ((ObjString *)AS_OBJ(value))
#define AS_CSTRING(value) (((ObjString *)AS_OBJ(value))->chars)
#define AS_FUNCTION(value) ((ObjFunction *)AS_OBJ(value))
#define AS_NATIVE(value) (((ObjNative *)AS_OBJ(value))->function)
#define AS_OBJ_STRUCT(value) ((ObjObject *)AS_OBJ(value))

// konstantes array
typedef struct {
  Value *values;
  size_t capacity;
  size_t count;
} ValueArray;

// 1 chunk repräsentiert einen bytecode-blok
struct Chunk {
  uint8_t *code; // bytecode
  size_t capacity;
  size_t count;
  ValueArray constants;
  const char **function_names;
  size_t *function_name_capacity;
  size_t function_name_count;
  int is_interned;
};

// hilfsfunktionen für konstante arrays
void value_array_init(ValueArray *array);
void value_array_free(ValueArray *array);
void value_array_write(ValueArray *array, Value value);

// hilfsfunktionen für chunks
void chunk_init(Chunk *chunk);
void chunk_free(Chunk *chunk);

void chunk_write_opcode(Chunk *chunk, Opcode opcode, int line);
void chunk_write_byte(Chunk *chunk, uint8_t byte, int line);
void chunk_write_short(Chunk *chunk, uint16_t value, int line);
void chunk_write_constant(Chunk *chunk, Value value, int line);
int chunk_add_constant(Chunk *chunk, Value value);

// sprünge und kontrollstrukturen
size_t chunk_write_jump(Chunk *chunk, Opcode opcode, int line);
void chunk_patch_jump(Chunk *chunk, size_t offset);

size_t chunk_write_loop(Chunk *chunk, int line);
void chunk_patch_loop(Chunk *chunk, size_t offset, size_t loop_start);

int chunk_add_function_name(Chunk *chunk, const char *name);

// disassembler
void disassemble_chunk(Chunk *chunk, const char *name);
int disassemble_instruction(Chunk *chunk, int offset);

#endif
