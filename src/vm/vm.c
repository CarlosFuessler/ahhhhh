#include "vm/vm.h"
#include "vm/opcodes.h"
#include "vm/chunk.h"
#include "vm/table.h"
#include "vm/stdlib.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static void push(VM *vm, Value value);
static Value pop(VM *vm);
static void collect_garbage(VM *vm);
static void mark_object(Obj *obj);
static void mark_value(Value value);

static void reset_stack(VM *vm) {
    vm->stack.stack_top = vm->stack.stack;
    vm->frame_count = 0;
}

// initialisiert die vm
void vm_init(VM *vm) {
    memset(vm, 0, sizeof(VM));
    reset_stack(vm);
    table_init(&vm->strings);
    table_init(&vm->globals.table);
    vm->objects = NULL;
    vm->gc_suspend = 0;
}
//befreit den speicher von obj
static void free_obj(Obj *obj) {
    switch (obj->type) {
        case OBJ_STRING: {
            ObjString *string = (ObjString *)obj;
            free((void *)string->chars);
            free(string);
            break;
        }
        case OBJ_OBJECT: {
            ObjObject *object = (ObjObject *)obj;
            table_free(object->fields);
            free(object->fields);
            free(object);
            break;
        }
        case OBJ_FUNCTION: {
            ObjFunction *function = (ObjFunction *)obj;
            if (function->chunk) {
                chunk_free(function->chunk);
                free(function->chunk);
            }
            free(function);
            break;
        }
        case OBJ_NATIVE: {
            free(obj);
            break;
        }
        case OBJ_CLOSURE: {
            ObjClosure *closure = (ObjClosure *)obj;
            free(closure->upvalues);
            free(closure);
            break;
        }
        case OBJ_ARRAY: {
            ObjArray *array = (ObjArray *)obj;
            free(array->elements);
            free(array);
            break;
        }
        case OBJ_UPVALUE: {
            free(obj);
            break;
        }
    }
}
//befreit vm
void vm_free(VM *vm) {
    reset_stack(vm);
    table_free(&vm->strings);
    table_free(&vm->globals.table);
    
    Obj *object = vm->objects;
    while (object != NULL) {
        Obj *next = object->next;
        free_obj(object);
        object = next;
    }
}

// legt einen wert oben auf den stack
static void push(VM *vm, Value value) {
    if (vm->stack.stack_top - vm->stack.stack >= STACK_MAX) { 
        fprintf(stderr, "Stack overflow\n"); 
        exit(1); 
    }
    *vm->stack.stack_top++ = value;
}

// nimmt den obersten wert vom stack
static Value pop(VM *vm) { 
    return *--vm->stack.stack_top; 
}

// wirft einen blick auf den obersten wert
static Value peek(VM *vm) { 
    return vm->stack.stack_top[-1]; 
}
//blick uaf bestimmten wert
static Value peek_n(VM *vm, int n) {
    return vm->stack.stack_top[-1 - n];
}

static int is_true(Value value) { 
    if (IS_NULL(value)) return 0;
    if (IS_BOOL(value)) return AS_BOOL(value);
    return 1;
}
//hashing
static uint32_t hash_string(const char *key, int length) {
    uint32_t hash = 2166136261u;
    for (int i = 0; i < length; i++) {
        hash ^= (uint8_t)key[i];
        hash *= 16777619;
    }
    return hash;
}
//speicher fuer objekt legen
static Obj *allocate_obj(VM *vm, size_t size, ObjType type) {
#ifdef DEBUG_STRESS_GC
    if (vm->gc_suspend <= 0) collect_garbage(vm);
#endif
    Obj *obj = malloc(size);
    obj->type = type;
    obj->is_marked = 0;
    obj->next = vm->objects;
    vm->objects = obj;
    return obj;
}

static ObjString *take_string(VM *vm, char *chars, int length) {
    uint32_t hash = hash_string(chars, length);
    ObjString *interned = table_find_string(&vm->strings, chars, length, hash);
    if (interned != NULL) {
        free(chars);
        return interned;
    }
    
    ObjString *string = (ObjString *)allocate_obj(vm, sizeof(ObjString), OBJ_STRING);
    string->chars = chars;
    string->hash = hash;
    table_set(&vm->strings, string, (Value){VALUE_NULL, {0}});
    return string;
}

ObjString *copy_string(VM *vm, const char *chars, int length) {
    uint32_t hash = hash_string(chars, length);
    ObjString *interned = table_find_string(&vm->strings, chars, length, hash);
    if (interned != NULL) {
        return interned;
    }
    
    char *heap_chars = malloc(length + 1);
    memcpy(heap_chars, chars, length);
    heap_chars[length] = '\0';
    
    ObjString *string = (ObjString *)allocate_obj(vm, sizeof(ObjString), OBJ_STRING);
    string->chars = heap_chars;
    string->hash = hash;
    table_set(&vm->strings, string, (Value){VALUE_NULL, {0}});
    return string;
}

static int values_equal(Value a, Value b) {
    if (a.kind != b.kind) return 0;
    switch (a.kind) { 
        case VALUE_NUMBER: return AS_NUMBER(a) == AS_NUMBER(b); 
        case VALUE_BOOL:   return AS_BOOL(a) == AS_BOOL(b); 
        case VALUE_NULL:   return 1; 
        case VALUE_STRING: return AS_CSTRING(a) == AS_CSTRING(b);
        case VALUE_OBJECT:
        case VALUE_FUNCTION:
        case VALUE_NATIVE:
        case VALUE_ARRAY:  return AS_OBJ(a) == AS_OBJ(b);
        default: break; 
    }
    return 0;
}

static void print_value(Value value) {
    if (IS_NUMBER(value)) printf("%.15g", AS_NUMBER(value));
    else if (IS_STRING(value)) printf("'%s'", AS_CSTRING(value));
    else if (IS_BOOL(value)) printf("%s", AS_BOOL(value) ? "true" : "false");
    else if (IS_NULL(value)) printf("null");
    else if (IS_ARRAY(value)) {
        printf("[");
        ObjArray *array = (ObjArray *)AS_OBJ(value);
        for (size_t i = 0; i < array->count; i++) {
            print_value(array->elements[i]);
            if (i < array->count - 1) printf(", ");
        }
        printf("]");
    } else if (IS_OBJECT(value)) {
        printf("<object>");
    } else if (IS_FUNCTION(value)) {
        printf("<fn>");
    } else if (IS_NATIVE(value)) {
        printf("<native fn>");
    } else {
        printf("<unknown>");
    }
}

static CallFrame *current_frame(VM *vm) {
    return &vm->frames[vm->frame_count - 1];
}

static int call_value(VM *vm, Value callee, int arg_count) {
    if (IS_FUNCTION(callee)) {
        ObjFunction *function = AS_FUNCTION(callee);
        if (arg_count != function->arity) {
            fprintf(stderr, "Runtime error: expected %d arguments, got %d\n", function->arity, arg_count);
            return 0;
        }
        if (vm->frame_count >= FRAMES_MAX) {
            fprintf(stderr, "Runtime error: call stack overflow\n");
            return 0;
        }
        CallFrame *frame = &vm->frames[vm->frame_count++];
        frame->function = function;
        frame->ip = function->chunk->code;
        frame->slots = vm->stack.stack_top - arg_count;
        frame->local_count = arg_count;
        return 1;
    } else if (IS_NATIVE(callee)) {
        NativeFn native = AS_NATIVE(callee);
        Value result = native(vm, arg_count, vm->stack.stack_top - arg_count);
        vm->stack.stack_top -= arg_count + 1;
        push(vm, result);
        return 1;
    }
    fprintf(stderr, "Runtime error: can only call functions and natives\n");
    return 0;
}

static void mark_value(Value value) {
    if (IS_OBJ(value)) mark_object(AS_OBJ(value));
}

static void mark_object(Obj *obj) {
    if (obj == NULL || obj->is_marked) return;
    obj->is_marked = 1;

    switch (obj->type) {
        case OBJ_FUNCTION: {
            ObjFunction *fn = (ObjFunction *)obj;
            // Konstanten nur markieren nur wenn interniert
            if (fn->chunk && fn->chunk->is_interned) {
                for (int i = 0; i < fn->chunk->constants.count; i++) {
                    mark_value(fn->chunk->constants.values[i]);
                }
            }
            break;
        }
        case OBJ_ARRAY: {
            ObjArray *array = (ObjArray *)obj;
            for (size_t i = 0; i < array->count; i++) {
                mark_value(array->elements[i]);
            }
            break;
        }
        case OBJ_OBJECT: {
            ObjObject *object = (ObjObject *)obj;
            if (object->fields) {
                for (int i = 0; i < object->fields->capacity; i++) {
                    Entry *entry = &object->fields->entries[i];
                    if (entry->key != NULL) {
                        mark_object((Obj*)entry->key);
                        mark_value(entry->value);
                    }
                }
            }
            break;
        }
        case OBJ_CLOSURE: {
            ObjClosure *closure = (ObjClosure *)obj;
            mark_object((Obj*)closure->function);
            break;
        }
        default: break;
    }
}

// automatische speicherbereinigung
static void collect_garbage(VM *vm) {
    if (vm->gc_suspend > 0) return;
    
    // alle werte auf dem stack als erreichbar markieren
    for (Value *slot = vm->stack.stack; slot < vm->stack.stack_top; slot++) {
        mark_value(*slot);
    }
    
    // alle globalen variablen markieren
    for (int i = 0; i < vm->globals.table.capacity; i++) {
        Entry *entry = &vm->globals.table.entries[i];
        if (entry->key != NULL) {
            mark_object((Obj*)entry->key);
            mark_value(entry->value);
        }
    }
    
    // alle funktionen im callstack
    for (int i = 0; i < vm->frame_count; i++) {
        mark_object((Obj*)vm->frames[i].function);
    }
    
    // alle nicht markierten objekte freigeben
    Obj **prev = &vm->objects;
    while (*prev != NULL) {
        if (!(*prev)->is_marked) {
            Obj *unreached = *prev;
            *prev = unreached->next;
            if (unreached->type == OBJ_STRING) {
                ObjString *str = (ObjString *)unreached;
                table_delete(&vm->strings, str);
            }
            free_obj(unreached);
        } else {
            (*prev)->is_marked = 0;
            prev = &(*prev)->next;
        }
    }
}

static void debug_trace_step(VM *vm, CallFrame *frame) {
    printf("          ");
    for (Value *slot = vm->stack.stack; slot < vm->stack.stack_top; slot++) {
        printf("[ ");
        print_value(*slot);
        printf(" ]");
    }
    printf("\n");
    disassemble_instruction(frame->function->chunk, (int)(frame->ip - frame->function->chunk->code));
}

// butecode ausfuehren dispatcher
static InterpretResult run(VM *vm) {
    CallFrame *frame = current_frame(vm);
    
#define READ_BYTE() (*frame->ip++)
#define READ_SHORT() (frame->ip += 2, (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))

    for (;;) {
        if (vm->debug_trace) {
            debug_trace_step(vm, frame);
        }

        uint8_t opcode = READ_BYTE();
        switch (opcode) {
            case OP_CONSTANT: {
                uint8_t idx = READ_BYTE();
                push(vm, frame->function->chunk->constants.values[idx]);
                break;
            }
            case OP_NULL:  push(vm, (Value){VALUE_NULL, {0}}); break;
            case OP_TRUE:  push(vm, (Value){VALUE_BOOL, {.boolean = 1}}); break;
            case OP_FALSE: push(vm, (Value){VALUE_BOOL, {.boolean = 0}}); break;

            case OP_ADD: {
                Value b = pop(vm), a = pop(vm);
                if (IS_STRING(a) && IS_STRING(b)) {
                    size_t len_a = strlen(AS_CSTRING(a));
                    size_t len_b = strlen(AS_CSTRING(b));
                    char *result = malloc(len_a + len_b + 1);
                    memcpy(result, AS_CSTRING(a), len_a);
                    memcpy(result + len_a, AS_CSTRING(b), len_b + 1);
                    ObjString *string = take_string(vm, result, (int)(len_a + len_b));
                    push(vm, (Value){VALUE_STRING, {.obj = (Obj*)string}});
                } else if (IS_NUMBER(a) && IS_NUMBER(b)) {
                    push(vm, (Value){VALUE_NUMBER, {.number = AS_NUMBER(a) + AS_NUMBER(b)}});
                } else {
                    fprintf(stderr, "Runtime error: invalid operands for +\n");
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_SUB: {
                Value b = pop(vm), a = pop(vm);
                push(vm, (Value){VALUE_NUMBER, {.number = AS_NUMBER(a) - AS_NUMBER(b)}});
                break;
            }
            case OP_MUL: {
                Value b = pop(vm), a = pop(vm);
                push(vm, (Value){VALUE_NUMBER, {.number = AS_NUMBER(a) * AS_NUMBER(b)}});
                break;
            }
            case OP_DIV: {
                Value b = pop(vm), a = pop(vm);
                push(vm, (Value){VALUE_NUMBER, {.number = AS_NUMBER(a) / AS_NUMBER(b)}});
                break;
            }
            case OP_MOD: {
                Value b = pop(vm), a = pop(vm);
                if (AS_NUMBER(b) == 0) {
                    fprintf(stderr, "Runtime error: modulo by zero\n");
                    return INTERPRET_RUNTIME_ERROR;
                }
                push(vm, (Value){VALUE_NUMBER, {.number = fmod(AS_NUMBER(a), AS_NUMBER(b))}});
                break;
            }

            case OP_EQ: {
                Value b = pop(vm), a = pop(vm);
                push(vm, (Value){VALUE_BOOL, {.boolean = values_equal(a, b)}});
                break;
            }
            case OP_NE: {
                Value b = pop(vm), a = pop(vm);
                push(vm, (Value){VALUE_BOOL, {.boolean = !values_equal(a, b)}});
                break;
            }
            case OP_LT: {
                Value b = pop(vm), a = pop(vm);
                push(vm, (Value){VALUE_BOOL, {.boolean = AS_NUMBER(a) < AS_NUMBER(b)}});
                break;
            }
            case OP_GT: {
                Value b = pop(vm), a = pop(vm);
                push(vm, (Value){VALUE_BOOL, {.boolean = AS_NUMBER(a) > AS_NUMBER(b)}});
                break;
            }
            case OP_LE: {
                Value b = pop(vm), a = pop(vm);
                push(vm, (Value){VALUE_BOOL, {.boolean = AS_NUMBER(a) <= AS_NUMBER(b)}});
                break;
            }
            case OP_GE: {
                Value b = pop(vm), a = pop(vm);
                push(vm, (Value){VALUE_BOOL, {.boolean = AS_NUMBER(a) >= AS_NUMBER(b)}});
                break;
            }

            case OP_NOT: push(vm, (Value){VALUE_BOOL, {.boolean = !is_true(pop(vm))}}); break;
            case OP_NEG: push(vm, (Value){VALUE_NUMBER, {.number = -AS_NUMBER(pop(vm))}}); break;

            case OP_PRINT: {
                uint8_t arg_count = READ_BYTE();
                Value args[8];
                for (int i = 0; i < arg_count; i++) args[i] = pop(vm);
                for (int i = 0; i < arg_count / 2; i++) {
                    Value temp = args[i];
                    args[i] = args[arg_count - 1 - i];
                    args[arg_count - 1 - i] = temp;
                }
                vm_native_print(vm, arg_count, args);
                break;
            }

            case OP_GET_GLOBAL: {
                uint8_t index = READ_BYTE();
                Value name_val = frame->function->chunk->constants.values[index];
                ObjString *name = (ObjString*)AS_OBJ(name_val);
                Value value;
                if (table_get(&vm->globals.table, name, &value)) {
                    push(vm, value);
                } else {
                    push(vm, (Value){VALUE_NULL, {0}});
                }
                break;
            }
            case OP_DEFINE_GLOBAL: {
                uint8_t index = READ_BYTE();
                Value name_val = frame->function->chunk->constants.values[index];
                ObjString *name = (ObjString*)AS_OBJ(name_val);
                Value value = pop(vm);
                table_set(&vm->globals.table, name, value);
                break;
            }
            case OP_STORE_GLOBAL: {
                uint8_t index = READ_BYTE();
                Value name_val = frame->function->chunk->constants.values[index];
                ObjString *name = (ObjString*)AS_OBJ(name_val);
                Value value = peek(vm);
                if (table_set(&vm->globals.table, name, value)) {
                    table_delete(&vm->globals.table, name);
                    fprintf(stderr, "Undefined variable '%s'.\n", name->chars);
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }

            case OP_LOAD_LOCAL: {
                uint8_t index = READ_BYTE();
                push(vm, frame->slots[index]);
                break;
            }
            case OP_STORE_LOCAL: {
                uint8_t index = READ_BYTE();
                frame->slots[index] = peek(vm);
                break;
            }

            case OP_GET_FIELD: {
                uint8_t name_index = READ_BYTE();
                Value name_val = frame->function->chunk->constants.values[name_index];
                ObjString *name = (ObjString*)AS_OBJ(name_val);
                Value object_val = pop(vm);
                
                if (IS_ARRAY(object_val) && strcmp(name->chars, "len") == 0) {
                    push(vm, (Value){VALUE_NUMBER, {.number = (double)((ObjArray*)AS_OBJ(object_val))->count}});
                    break;
                }
                
                if (IS_STRING(object_val) && strcmp(name->chars, "len") == 0) {
                    push(vm, (Value){VALUE_NUMBER, {.number = (double)strlen(AS_CSTRING(object_val))}});
                    break;
                }
                
                if (!IS_OBJECT(object_val)) {
                    fprintf(stderr, "Runtime error: only objects have fields. Got kind %d, field %s\n", object_val.kind, name->chars);
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                ObjObject *obj = AS_OBJ_STRUCT(object_val);
                Value value;
                if (table_get(obj->fields, name, &value)) {
                    push(vm, value);
                } else {
                    push(vm, (Value){VALUE_NULL, {0}});
                }
                break;
            }
            case OP_SET_FIELD: {
                uint8_t name_index = READ_BYTE();
                Value name_val = frame->function->chunk->constants.values[name_index];
                ObjString *name = (ObjString*)AS_OBJ(name_val);
                Value object_val = pop(vm);
                Value value = peek(vm);
                
                if (!IS_OBJECT(object_val)) {
                    fprintf(stderr, "Runtime error: only objects have fields.\n");
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                ObjObject *obj = AS_OBJ_STRUCT(object_val);
                table_set(obj->fields, name, value);
                break;
            }
            case OP_STRUCT: {
                uint8_t field_count = READ_BYTE();
                // feldnamen zuerst lesen um sie im bytecode zu überspringen
                uint8_t name_indices[256];
                for (int i = 0; i < field_count; i++) {
                    name_indices[i] = READ_BYTE();
                }

                ObjObject *obj = (ObjObject *)allocate_obj(vm, sizeof(ObjObject), OBJ_OBJECT);
                obj->fields = malloc(sizeof(Table));
                table_init(obj->fields);
                
                // werte vom Stack holen und namen zuweisen
                for (int i = field_count - 1; i >= 0; i--) {
                    Value value = pop(vm);
                    uint8_t name_index = name_indices[i];
                    Value name_val = frame->function->chunk->constants.values[name_index];
                    ObjString *name = (ObjString*)AS_OBJ(name_val);
                    table_set(obj->fields, name, value);
                }
                push(vm, (Value){VALUE_OBJECT, {.obj = (Obj*)obj}});
                break;
            }

            case OP_ARRAY: {
                uint8_t count = READ_BYTE();
                ObjArray *array = (ObjArray *)allocate_obj(vm, sizeof(ObjArray), OBJ_ARRAY);
                array->count = count;
                array->capacity = count;
                array->elements = NULL;
                if (count > 0) {
                    array->elements = malloc(sizeof(Value) * count);
                    for (int i = count - 1; i >= 0; i--) {
                        array->elements[i] = pop(vm);
                    }
                }
                push(vm, (Value){VALUE_ARRAY, {.obj = (Obj*)array}});
                break;
            }

            case OP_INDEX_GET: {
                Value index_val = pop(vm);
                Value base_val = pop(vm);

                if (IS_STRING(base_val)) {
                    if (!IS_NUMBER(index_val)) {
                        fprintf(stderr, "Runtime error: string index must be a number.\n");
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    const char *s = AS_CSTRING(base_val);
                    int idx = (int)AS_NUMBER(index_val);
                    int len = (int)strlen(s);
                    if (idx < 0 || idx >= len) {
                        push(vm, (Value){VALUE_NULL, {0}});
                    } else {
                        char c[2] = {s[idx], '\0'};
                        push(vm, (Value){VALUE_STRING, {.obj = (Obj*)copy_string(vm, c, 1)}});
                    }
                } else if (IS_ARRAY(base_val)) {
                    if (!IS_NUMBER(index_val)) {
                        fprintf(stderr, "Runtime error: array index must be a number.\n");
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    ObjArray *array = (ObjArray *)AS_OBJ(base_val);
                    int idx = (int)AS_NUMBER(index_val);
                    if (idx < 0 || (size_t)idx >= array->count) {
                        push(vm, (Value){VALUE_NULL, {0}});
                    } else {
                        push(vm, array->elements[idx]);
                    }
                } else {
                    fprintf(stderr, "Runtime error: only strings and arrays can be indexed.\n");
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_INDEX_SET: {
                Value index_val = pop(vm);
                Value base_val = pop(vm);
                Value value = peek(vm);

                if (IS_ARRAY(base_val)) {
                    if (!IS_NUMBER(index_val)) {
                        fprintf(stderr, "Runtime error: array index must be a number.\n");
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    ObjArray *array = (ObjArray *)AS_OBJ(base_val);
                    int idx = (int)AS_NUMBER(index_val);
                    if (idx < 0 || (size_t)idx >= array->count) {
                        fprintf(stderr, "Runtime error: array index out of bounds.\n");
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    array->elements[idx] = value;
                } else {
                    fprintf(stderr, "Runtime error: only arrays can be index-set.\n");
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }

            case OP_JUMP: {
                uint16_t offset = READ_SHORT();
                frame->ip += offset;
                break;
            }
            case OP_JUMP_IF_FALSE: {
                uint16_t offset = READ_SHORT();
                if (!is_true(pop(vm))) frame->ip += offset;
                break;
            }
            case OP_LOOP: {
                uint16_t offset = READ_SHORT();
                frame->ip -= offset;
                break;
            }
            case OP_POP: pop(vm); break;
            case OP_DUP: push(vm, peek(vm)); break;

            case OP_CALL: {
                uint8_t arg_count = READ_BYTE();
                if (!call_value(vm, peek_n(vm, arg_count), arg_count)) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                frame = current_frame(vm);
                break;
            }

            case OP_RETURN: {
                Value result = pop(vm);
                vm->frame_count--;
                if (vm->frame_count == 0) {
                    pop(vm); // Die verankerte temp_fn vom Stack holen
                    push(vm, result);
                    return INTERPRET_OK;
                }
                vm->stack.stack_top = frame->slots - 1;
                push(vm, result);
                frame = current_frame(vm);
                break;
            }

            case OP_CLOSURE: {
                Value function_val = pop(vm);
                ObjFunction *function = AS_FUNCTION(function_val);
                ObjClosure *closure = (ObjClosure *)allocate_obj(vm, sizeof(ObjClosure), OBJ_CLOSURE);
                closure->function = function;
                push(vm, (Value){VALUE_OBJECT, {.obj = (Obj*)closure}});
                break;
            }

            case OP_LOAD_GLOBAL:
            case OP_CLOSE_UPVALUE:
            case OP_ARRAY_GET:
            case OP_ARRAY_SET:
            case OP_ARRAY_LEN:
            case OP_TYPE:
            case OP_BREAK:
            case OP_CONTINUE:
                fprintf(stderr, "Unhandled opcode\n");
                return INTERPRET_RUNTIME_ERROR;

            default:
                fprintf(stderr, "Unknown opcode %d\n", opcode);
                return INTERPRET_RUNTIME_ERROR;
        }
    }
    
#undef READ_BYTE
#undef READ_SHORT
}

static void intern_constants(VM *vm, Chunk *chunk) {
    if (chunk->is_interned) return;
    
    vm->gc_suspend++;

    for (int i = 0; i < chunk->constants.count; i++) {
        Value *value = &chunk->constants.values[i];
        if (value->kind == VALUE_STRING) {
            const char *raw_chars = (const char *)value->as.obj;
            ObjString *string = copy_string(vm, raw_chars, (int)strlen(raw_chars));
            value->as.obj = (Obj *)string;
        } else if (value->kind == VALUE_FUNCTION) {
            ObjFunction *fn = (ObjFunction *)value->as.obj;
            // Die Funktion verankern, um zu verhindern, dass die GC sie bei der Rekursion bereinigt
            push(vm, (Value){VALUE_FUNCTION, {.obj = (Obj*)fn}});
            if (fn->obj.next == NULL && fn != (ObjFunction*)vm->objects) {
                fn->obj.next = vm->objects;
                vm->objects = (Obj*)fn;
            }
            if (fn->chunk) intern_constants(vm, fn->chunk);
            pop(vm);
        }
    }
    
    chunk->is_interned = 1;
    vm->gc_suspend--;
}

InterpretResult vm_interpret(VM *vm, Chunk *chunk) {
    ObjFunction *temp_fn = (ObjFunction *)allocate_obj(vm, sizeof(ObjFunction), OBJ_FUNCTION);
    temp_fn->chunk = chunk;
    temp_fn->arity = 0;
    temp_fn->upvalue_count = 0;
    temp_fn->upvalues = NULL;

    push(vm, (Value){VALUE_FUNCTION, {.obj = (Obj*)temp_fn}});
    intern_constants(vm, chunk);

    CallFrame *frame = &vm->frames[vm->frame_count++];
    frame->function = temp_fn;
    frame->ip = chunk->code;
    frame->slots = vm->stack.stack_top - 0; // Argumentanzahl ist 0
    frame->local_count = 0;

    InterpretResult result = run(vm);

    return result;
}

int vm_define_global(VM *vm, const char *name, Value value) {
    ObjString *string = copy_string(vm, name, (int)strlen(name));
    push(vm, (Value){VALUE_STRING, {.obj = (Obj*)string}});
    table_set(&vm->globals.table, string, value);
    pop(vm);
    return 0;
}

int vm_define_native(VM *vm, const char *name, NativeFn fn) {
    ObjNative *native = (ObjNative *)allocate_obj(vm, sizeof(ObjNative), OBJ_NATIVE);
    push(vm, (Value){VALUE_NATIVE, {.obj = (Obj *)native}});
    native->function = fn;
    vm_define_global(vm, name, (Value){VALUE_NATIVE, {.obj = (Obj *)native}});
    pop(vm);
    return 0;
}

NativeFn vm_lookup_native(VM *vm, const char *name) {
    for (int i = 0; i < vm->native_count; i++) {
        if (strcmp(vm->natives[i].name, name) == 0) return vm->natives[i].fn;
    }
    return NULL;
}

ObjArray *new_array(VM *vm, size_t count) {
    ObjArray *array = (ObjArray *)allocate_obj(vm, sizeof(ObjArray), OBJ_ARRAY);
    array->count = count;
    array->capacity = count;
    array->elements = NULL;
    if (count > 0) {
        array->elements = calloc(count, sizeof(Value));
    }
    return array;
}
