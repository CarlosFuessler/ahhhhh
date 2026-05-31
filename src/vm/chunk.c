#include "vm/chunk.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// initialisiert ein wert-array (z.B. für konstanten)
void value_array_init(ValueArray *array) {
    array->values = NULL;
    array->capacity = 0;
    array->count = 0;
}

// gibt den speicher eines wert-arrays wieder frei
void value_array_free(ValueArray *array) {
    free(array->values);
    value_array_init(array);
}

// fügt einen wert an das wert-array an und vergrößert es falls nötig
void value_array_write(ValueArray *array, Value value) {
    if (array->count + 1 > array->capacity) {
        size_t new_capacity = array->capacity < 8 ? 8 : array->capacity * 2;
        array->values = realloc(array->values, new_capacity * sizeof(Value));
        array->capacity = new_capacity;
    }
    array->values[array->count++] = value;
}

// initialisiert einen neuen bytecode-chunk
void chunk_init(Chunk *chunk) {
    chunk->code = NULL;
    chunk->capacity = 0;
    chunk->count = 0;
    value_array_init(&chunk->constants);
    chunk->function_names = NULL;
    chunk->function_name_capacity = NULL;
    chunk->function_name_count = 0;
    chunk->is_interned = 0;
}

// gibt den gesamten speicher eines chunks frei
void chunk_free(Chunk *chunk) {
    free(chunk->code);
    value_array_free(&chunk->constants);
    free(chunk->function_names);
    free(chunk->function_name_capacity);
    chunk_init(chunk);
}

// schreibt einen befehl (opcode) in den chunk
void chunk_write_opcode(Chunk *chunk, Opcode opcode, int line) {
    chunk_write_byte(chunk, (uint8_t)opcode, line);
    (void)line;
}

// schreibt ein einzelnes byte in den bytecode
void chunk_write_byte(Chunk *chunk, uint8_t byte, int line) {
    (void)line;
    if (chunk->count + 1 > chunk->capacity) {
        size_t new_capacity = chunk->capacity < 8 ? 8 : chunk->capacity * 2;
        chunk->code = realloc(chunk->code, new_capacity);
        chunk->capacity = new_capacity;
    }
    chunk->code[chunk->count++] = byte;
}

// schreibt zwei bytes (einen 16-bit short-wert) in den bytecode
void chunk_write_short(Chunk *chunk, uint16_t value, int line) {
    chunk_write_byte(chunk, (value >> 8) & 0xFF, line);
    chunk_write_byte(chunk, value & 0xFF, line);
}

// schreibt eine konstante in den chunk (befehl + index der konstante)
void chunk_write_constant(Chunk *chunk, Value value, int line) {
    int index = chunk_add_constant(chunk, value);
    if (index > 255) {
        fprintf(stderr, "Too many constants\n");
        exit(1);
    }
    chunk_write_byte(chunk, OP_CONSTANT, line);
    chunk_write_byte(chunk, (uint8_t)index, line);
}

// fügt dem konstanten-pool eine neue konstante hinzu und liefert deren index zurück
int chunk_add_constant(Chunk *chunk, Value value) {
    value_array_write(&chunk->constants, value);
    return (int)(chunk->constants.count - 1);
}

// schreibt einen sprungbefehl mit platzhalter (wird später gepatcht)
size_t chunk_write_jump(Chunk *chunk, Opcode opcode, int line) {
    chunk_write_opcode(chunk, opcode, line);
    chunk_write_short(chunk, 0xFFFF, line);
    return chunk->count - 2;
}

// patcht einen sprungbefehl mit der korrekten ziel-distanz
void chunk_patch_jump(Chunk *chunk, size_t offset) {
    uint16_t jump = (uint16_t)(chunk->count - (offset + 2));
    chunk->code[offset] = (jump >> 8) & 0xFF;
    chunk->code[offset + 1] = jump & 0xFF;
}

// schreibt einen rückwärtssprung für schleifen
size_t chunk_write_loop(Chunk *chunk, int line) {
    chunk_write_opcode(chunk, OP_LOOP, line);
    chunk_write_short(chunk, 0, line);
    return chunk->count - 2;
}

// patcht den rückwärtssprung mit der distanz zum schleifenanfang
void chunk_patch_loop(Chunk *chunk, size_t offset, size_t loop_start) {
    uint16_t jump = (uint16_t)((offset + 2) - loop_start);
    chunk->code[offset] = (jump >> 8) & 0xFF;
    chunk->code[offset + 1] = jump & 0xFF;
}

// fügt einen funktionsnamen zum chunk hinzu (für dynamische methodenaufrufe)
int chunk_add_function_name(Chunk *chunk, const char *name) {
    if (chunk->function_name_count >= 256) {
        fprintf(stderr, "Too many function names\n");
        return -1;
    }
    
    for (size_t i = 0; i < chunk->function_name_count; i++) {
        if (strcmp(chunk->function_names[i], name) == 0) {
            return (int)i;
        }
    }
    
    size_t new_capacity = chunk->function_name_capacity ? *chunk->function_name_capacity : 16;
    if (chunk->function_name_count >= new_capacity) {
        new_capacity = new_capacity < 8 ? 8 : new_capacity * 2;
        chunk->function_names = realloc(chunk->function_names, new_capacity * sizeof(const char *));
        if (chunk->function_name_capacity == NULL) {
            chunk->function_name_capacity = malloc(sizeof(size_t));
        }
        *chunk->function_name_capacity = new_capacity;
    }
    
    chunk->function_names[chunk->function_name_count] = name;
    return (int)(chunk->function_name_count++);
}

// disassembliert einen gesamten bytecode-chunk und gibt ihn aus
void disassemble_chunk(Chunk *chunk, const char *name) {
    printf("== %s ==\n", name);
    for (int offset = 0; offset < (int)chunk->count;) {
        offset = disassemble_instruction(chunk, offset);
    }
}

static int simple_instruction(const char *name, int offset) {
    printf("%s\n", name);
    return offset + 1;
}

static int byte_instruction(const char *name, Chunk *chunk, int offset) {
    uint8_t slot = chunk->code[offset + 1];
    printf("%-16s %4d\n", name, slot);
    return offset + 2;
}

static int constant_instruction(const char *name, Chunk *chunk, int offset) {
    uint8_t constant = chunk->code[offset + 1];
    printf("%-16s %4d '", name, constant);
    Value value = chunk->constants.values[constant];
    if (value.kind == VALUE_NUMBER) printf("%.3g", value.as.number);
    else if (value.kind == VALUE_BOOL) printf("%s", value.as.boolean ? "true" : "false");
    else if (value.kind == VALUE_NULL) printf("null");
    else if (value.kind == VALUE_STRING) {
        if (chunk->is_interned) {
            ObjString *string = (ObjString *)value.as.obj;
            printf("%s", string->chars);
        } else {
            printf("%s", (char*)value.as.obj);
        }
    } else if (value.kind == VALUE_FUNCTION) {
        printf("<fn>");
    }
    printf("'\n");
    return offset + 2;
}

static int jump_instruction(const char *name, int sign, Chunk *chunk, int offset) {
    uint16_t jump = (uint16_t)(chunk->code[offset + 1] << 8);
    jump |= chunk->code[offset + 2];
    printf("%-16s %4d -> %d\n", name, offset, offset + 3 + sign * jump);
    return offset + 3;
}

int disassemble_instruction(Chunk *chunk, int offset) {
    printf("%04d ", offset);
    uint8_t instruction = chunk->code[offset];
    switch (instruction) {
        case OP_CONSTANT: return constant_instruction("OP_CONSTANT", chunk, offset);
        case OP_NULL:     return simple_instruction("OP_NULL", offset);
        case OP_TRUE:     return simple_instruction("OP_TRUE", offset);
        case OP_FALSE:    return simple_instruction("OP_FALSE", offset);
        case OP_POP:      return simple_instruction("OP_POP", offset);
        case OP_DUP:      return simple_instruction("OP_DUP", offset);
        case OP_GET_GLOBAL: return byte_instruction("OP_GET_GLOBAL", chunk, offset);
        case OP_DEFINE_GLOBAL: return byte_instruction("OP_DEFINE_GLOBAL", chunk, offset);
        case OP_STORE_GLOBAL: return byte_instruction("OP_STORE_GLOBAL", chunk, offset);
        case OP_LOAD_LOCAL: return byte_instruction("OP_LOAD_LOCAL", chunk, offset);
        case OP_STORE_LOCAL: return byte_instruction("OP_STORE_LOCAL", chunk, offset);
        case OP_EQ:       return simple_instruction("OP_EQ", offset);
        case OP_NE:       return simple_instruction("OP_NE", offset);
        case OP_LT:       return simple_instruction("OP_LT", offset);
        case OP_GT:       return simple_instruction("OP_GT", offset);
        case OP_LE:       return simple_instruction("OP_LE", offset);
        case OP_GE:       return simple_instruction("OP_GE", offset);
        case OP_ADD:      return simple_instruction("OP_ADD", offset);
        case OP_SUB:      return simple_instruction("OP_SUB", offset);
        case OP_MUL:      return simple_instruction("OP_MUL", offset);
        case OP_DIV:      return simple_instruction("OP_DIV", offset);
        case OP_MOD:      return simple_instruction("OP_MOD", offset);
        case OP_NOT:      return simple_instruction("OP_NOT", offset);
        case OP_NEG:      return simple_instruction("OP_NEG", offset);
        case OP_PRINT:    return byte_instruction("OP_PRINT", chunk, offset);
        case OP_JUMP:     return jump_instruction("OP_JUMP", 1, chunk, offset);
        case OP_JUMP_IF_FALSE: return jump_instruction("OP_JUMP_IF_FALSE", 1, chunk, offset);
        case OP_LOOP:     return jump_instruction("OP_LOOP", -1, chunk, offset);
        case OP_CALL:     return byte_instruction("OP_CALL", chunk, offset);
        case OP_RETURN:   return simple_instruction("OP_RETURN", offset);
        case OP_INDEX_GET: return simple_instruction("OP_INDEX_GET", offset);
        case OP_INDEX_SET: return simple_instruction("OP_INDEX_SET", offset);
        case OP_GET_FIELD: return byte_instruction("OP_GET_FIELD", chunk, offset);
        case OP_SET_FIELD: return byte_instruction("OP_SET_FIELD", chunk, offset);
        case OP_STRUCT: {
            uint8_t field_count = chunk->code[offset + 1];
            printf("%-16s %4d\n", "OP_STRUCT", field_count);
            for (int i = 0; i < field_count; i++) {
                uint8_t name_index = chunk->code[offset + 2 + i];
                printf("%04d |                     field %d: %s\n", offset + 2 + i, i, 
                       (char*)chunk->constants.values[name_index].as.obj);
            }
            return offset + 2 + field_count;
        }
        default:
            printf("Unknown opcode %d\n", instruction);
            return offset + 1;
    }
}