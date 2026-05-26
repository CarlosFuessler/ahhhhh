#ifndef AHHHHH_OPCODES_H
#define AHHHHH_OPCODES_H

#include <stdint.h>

typedef enum {
    OP_CONSTANT,
    OP_NULL,
    OP_TRUE,
    OP_FALSE,

    OP_LOAD_GLOBAL,
    OP_STORE_GLOBAL,
    OP_LOAD_LOCAL,
    OP_STORE_LOCAL,

    OP_ADD,
    OP_SUB,
    OP_MUL,
    OP_DIV,
    OP_MOD,

    OP_EQ,
    OP_NE,
    OP_LT,
    OP_GT,
    OP_LE,
    OP_GE,

    OP_NOT,
    OP_NEG,

    OP_PRINT,

    OP_JUMP,
    OP_JUMP_IF_FALSE,

    OP_LOOP,

    OP_CALL,
    OP_RETURN,

    OP_CLOSURE,
    OP_CLOSE_UPVALUE,

    OP_GET_GLOBAL,
    OP_DEFINE_GLOBAL,

    OP_INDEX_GET,
    OP_INDEX_SET,

    OP_GET_FIELD,
    OP_SET_FIELD,

    OP_ARRAY,

    OP_ARRAY_GET,
    OP_ARRAY_SET,
    OP_ARRAY_LEN,

    OP_STRUCT,
    OP_TYPE,

    OP_POP,
    OP_DUP,

    OP_BREAK,
    OP_CONTINUE,

    OP_COUNT
} Opcode;

typedef enum {
    VALUE_NUMBER,
    VALUE_STRING,
    VALUE_BOOL,
    VALUE_NULL,
    VALUE_FUNCTION,
    VALUE_NATIVE,
    VALUE_OBJECT,
    VALUE_ARRAY,
} ValueKind;

#endif