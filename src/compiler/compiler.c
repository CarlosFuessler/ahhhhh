#include "compiler/compiler.h"

#include "lexer/lexer.h"
#include "lexer/token.h"
#include "lexer/token_stream.h"
#include "parser/parser.h"
#include "vm/opcodes.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void emit_byte(Compiler *compiler, uint8_t byte, int line) { chunk_write_byte(compiler->function->chunk, byte, line); (void)line; }
static void emit_opcode(Compiler *compiler, Opcode opcode, int line) { chunk_write_opcode(compiler->function->chunk, opcode, line); (void)line; }
static void emit_constant(Compiler *compiler, Value value, int line) { 
    int index = chunk_add_constant(compiler->function->chunk, value);
    chunk_write_byte(compiler->function->chunk, OP_CONSTANT, line);
    chunk_write_byte(compiler->function->chunk, (uint8_t)index, line); 
}
static size_t emit_jump(Compiler *compiler, Opcode opcode, int line) { return chunk_write_jump(compiler->function->chunk, opcode, line); }
static void patch_jump(Compiler *compiler, size_t offset) { chunk_patch_jump(compiler->function->chunk, offset); }
static size_t emit_loop(Compiler *compiler, int line) { return chunk_write_loop(compiler->function->chunk, line); }
static void patch_loop(Compiler *compiler, size_t offset, size_t loop_start) { chunk_patch_loop(compiler->function->chunk, offset, loop_start); }

static int compile_expression(Compiler *compiler, AstExpression *expr);
static int compile_statement(Compiler *compiler, AstStatement *stmt);
static Type *get_expression_type(Compiler *compiler, AstExpression *expr);
static Type *get_lvalue_type(Compiler *compiler, AstLvalueExpression *lvalue);
static int resolve_global(Compiler *compiler, const char *name);

typedef struct Loop {
    size_t start;
    size_t *breaks;
    int break_count;
    int break_capacity;
    size_t *continues;
    int continue_count;
    int continue_capacity;
    struct Loop *enclosing;
} Loop;

static void start_loop(Compiler *compiler, Loop *loop, size_t start) {
    loop->start = start;
    loop->breaks = NULL;
    loop->break_count = 0;
    loop->break_capacity = 0;
    loop->continues = NULL;
    loop->continue_count = 0;
    loop->continue_capacity = 0;
    loop->enclosing = (Loop *)compiler->current_loop;
    compiler->current_loop = loop;
}

static void end_loop(Compiler *compiler, size_t continue_target) {
    Loop *loop = (Loop *)compiler->current_loop;
    if (loop->breaks) {
        for (int i = 0; i < loop->break_count; i++) {
            patch_jump(compiler, loop->breaks[i]);
        }
        free(loop->breaks);
    }
    if (loop->continues) {
        for (int i = 0; i < loop->continue_count; i++) {
            patch_loop(compiler, loop->continues[i], continue_target);
        }
        free(loop->continues);
    }
    compiler->current_loop = loop->enclosing;
}

static int compile_break(Compiler *compiler) {
    if (compiler->current_loop == NULL) {
        fprintf(stderr, "Error: 'br' outside of loop\n");
        return 1;
    }
    Loop *loop = (Loop *)compiler->current_loop;
    size_t offset = emit_jump(compiler, OP_JUMP, 0);
    if (loop->break_count >= loop->break_capacity) {
        loop->break_capacity = loop->break_capacity < 8 ? 8 : loop->break_capacity * 2;
        loop->breaks = realloc(loop->breaks, sizeof(size_t) * loop->break_capacity);
    }
    loop->breaks[loop->break_count++] = offset;
    return 0;
}

static int compile_continue(Compiler *compiler) {
    if (compiler->current_loop == NULL) {
        fprintf(stderr, "Error: 'fw' outside of loop\n");
        return 1;
    }
    Loop *loop = (Loop *)compiler->current_loop;
    size_t offset = emit_jump(compiler, OP_LOOP, 0);
    if (loop->continue_count >= loop->continue_capacity) {
        loop->continue_capacity = loop->continue_capacity < 8 ? 8 : loop->continue_capacity * 2;
        loop->continues = realloc(loop->continues, sizeof(size_t) * loop->continue_capacity);
    }
    loop->continues[loop->continue_count++] = offset;
    return 0;
}

static int compile_print(Compiler *compiler, AstExpression *fn_expr, AstExpression *args, size_t arg_count) {
    size_t i;
    for (i = 0; i < arg_count; i++) {
        if (compile_expression(compiler, args + i)) return 1;
    }
    emit_opcode(compiler, OP_PRINT, 0);
    emit_byte(compiler, (uint8_t)arg_count, 0);
    emit_opcode(compiler, OP_NULL, 0);
    return 0;
}

static int compile_literal(Compiler *compiler, AstLiteralExpression *literal) {
    switch (literal->kind) {
    case AST_LITERAL_NUMBER: emit_constant(compiler, (Value){VALUE_NUMBER, {.number = literal->expr.number.val}}, 0); break;
    case AST_LITERAL_STRING:   emit_constant(compiler, (Value){VALUE_STRING, {.obj = (Obj*)literal->expr.string.val}}, 0); break;
    case AST_LITERAL_BOOL:   emit_opcode(compiler, literal->expr.boolean.val ? OP_TRUE : OP_FALSE, 0); break;
    case AST_LITERAL_NULL:   emit_opcode(compiler, OP_NULL, 0); break;
    default:                 emit_opcode(compiler, OP_NULL, 0); break;
    }
    return 0;
}

static int resolve_local(Compiler *compiler, const char *name) {
    if (name == NULL) return -1;
    for (int i = compiler->local_count - 1; i >= 0; i--) {
        if (strcmp(compiler->locals[i].name, name) == 0) return compiler->param_count + i;
    }
    if (compiler->params) {
        for (int i = 0; i < compiler->param_count; i++) {
            if (compiler->params[i] && strcmp(compiler->params[i], name) == 0) return i;
        }
    }
    return -1;
}

static void begin_scope(Compiler *compiler) {
    compiler->scope_depth++;
}

static void end_scope(Compiler *compiler) {
    compiler->scope_depth--;
    while (compiler->local_count > 0 &&
           compiler->locals[compiler->local_count - 1].depth > compiler->scope_depth) {
        emit_opcode(compiler, OP_POP, 0);
        compiler->local_count--;
    }
}

static Type *get_expression_type(Compiler *compiler, AstExpression *expr) {
    if (!expr) return &type_unknown;
    switch (expr->kind) {
        case AST_EXPR_LITERAL:
            switch (expr->expr.literal.kind) {
                case AST_LITERAL_NUMBER: return &type_number;
                case AST_LITERAL_STRING: return &type_string;
                case AST_LITERAL_BOOL:   return &type_bool;
                case AST_LITERAL_NULL:   return &type_null;
                default:                 return &type_void;
            }
        case AST_EXPR_LVALUE:
            return get_lvalue_type(compiler, &expr->expr.lvalue);
        case AST_EXPR_BINARY: {
            Type *left = get_expression_type(compiler, expr->expr.binary.left);
            Type *right = get_expression_type(compiler, expr->expr.binary.right);
            if (expr->expr.binary.op == TOKEN_AND || expr->expr.binary.op == TOKEN_OR) {
                return &type_bool;
            }
            if (expr->expr.binary.op == TOKEN_PLUS && (types_equal(left, &type_string) || types_equal(right, &type_string))) {
                return &type_string;
            }
            if (expr->expr.binary.op >= TOKEN_DOUBLE_EQUALS && expr->expr.binary.op <= TOKEN_GREATER_EQUALS) {
                return &type_bool;
            }
            return &type_number;
        }
        case AST_EXPR_UNARY: {
            if (expr->expr.unary.op == TOKEN_EXCLAMATION_MARK) return &type_bool;
            if (expr->expr.unary.keyword && strcmp(expr->expr.unary.keyword, "mk") == 0) {
                return get_expression_type(compiler, expr->expr.unary.operand);
            }
            return &type_number;
        }
        case AST_EXPR_GROUP:
            return get_expression_type(compiler, expr->expr.group.expr);
        case AST_EXPR_ARRAY_LITERAL: {
            AstArrayLiteralExpression *array = &expr->expr.array_literal;
            Type *elem_type = &type_unknown;
            if (array->element_count > 0) {
                elem_type = get_expression_type(compiler, &array->elements[0]);
            }
            
            // Versuchen herauszufinden, ob dieser Array-Typ bereits existiert
            char array_name[256];
            snprintf(array_name, sizeof(array_name), "[]%s", elem_type->name);
            Type *existing = type_table_find(compiler->type_table, array_name);
            if (existing) return existing;

            // Temporären Array-Typ erstellen
            Type *arr_type = malloc(sizeof(Type));
            arr_type->kind = TYPE_ARRAY;
            arr_type->element_type = elem_type;
            arr_type->name = strdup(array_name);
            type_table_register(compiler->type_table, arr_type);
            return arr_type;
        }
        case AST_EXPR_ASSIGNMENT:
            return get_expression_type(compiler, expr->expr.assignment.right);
        case AST_EXPR_FUNCTION_CALL: {
            AstFunctionCallExpression *call = &expr->expr.function_call;

            // Auf Namensraum-Methodenaufruf oder Namensraum-Importfunktionsaufruf prüfen
            if (call->function->kind == AST_EXPR_LVALUE &&
                call->function->expr.lvalue.kind == AST_LVALUE_DOT) {
                AstExpression *base = call->function->expr.lvalue.expr.dot.base;
                const char *member = call->function->expr.lvalue.expr.dot.identifier;
                
                // Strukturmethode
                Type *base_type = get_expression_type(compiler, base);
                if (base_type && base_type->kind == TYPE_STRUCT) {
                    char method_name[256];
                    snprintf(method_name, sizeof(method_name), "%s_%s", base_type->name, member);
                    Type *ft = type_table_find(compiler->type_table, method_name);
                    if (ft && ft->kind == TYPE_FUNCTION) return ft->fn_info.return_type;
                }
                
                // Namensraum-Importaufruf
                if (base->kind == AST_EXPR_LVALUE && base->expr.lvalue.kind == AST_LVALUE_IDENTIFIER) {
                    const char *alias = base->expr.lvalue.expr.identifier;
                    if (resolve_local(compiler, alias) < 0) {
                        char namespaced_name[256];
                        snprintf(namespaced_name, sizeof(namespaced_name), "%s_%s", alias, member);
                        Type *ft = type_table_find(compiler->type_table, namespaced_name);
                        if (ft && ft->kind == TYPE_FUNCTION) return ft->fn_info.return_type;
                    }
                }
            }

            Type *ft = get_expression_type(compiler, call->function);
            if (ft && ft->kind == TYPE_FUNCTION) return ft->fn_info.return_type;
            if (ft && ft->kind == TYPE_STRUCT) return ft;
            
            if (call->function->kind == AST_EXPR_LVALUE &&
                call->function->expr.lvalue.kind == AST_LVALUE_IDENTIFIER) {
                const char *name = call->function->expr.lvalue.expr.identifier;
                if (strcmp(name, "print") == 0 || strcmp(name, "log") == 0 || strcmp(name, "exit") == 0 ||
                    strcmp(name, "array_insert") == 0 || strcmp(name, "array_reverse") == 0 ||
                    strcmp(name, "draw_rectangle") == 0 || strcmp(name, "draw_circle") == 0 ||
                    strcmp(name, "draw_line") == 0) return &type_void;
                if (strcmp(name, "sqrt") == 0 || strcmp(name, "sin") == 0 || strcmp(name, "cos") == 0 || strcmp(name, "abs") == 0 || 
                    strcmp(name, "floor") == 0 || strcmp(name, "ceil") == 0 || strcmp(name, "pow") == 0 ||
                    strcmp(name, "atan2") == 0 || strcmp(name, "exp") == 0 || strcmp(name, "log10") == 0 || strcmp(name, "ln") == 0 ||
                    strcmp(name, "clock") == 0 || strcmp(name, "rand") == 0 || strcmp(name, "rand_range") == 0 ||
                    strcmp(name, "strlen") == 0 || strcmp(name, "array_len") == 0 || strcmp(name, "to_number") == 0) return &type_number;
                if (strcmp(name, "input") == 0 || strcmp(name, "substr") == 0 || strcmp(name, "to_string") == 0 ||
                    strcmp(name, "char_at") == 0 || strcmp(name, "file_read") == 0) return &type_string;
                if (strcmp(name, "is_key_down") == 0 || strcmp(name, "is_key_pressed") == 0 ||
                    strcmp(name, "array_contains") == 0 || strcmp(name, "file_write") == 0 ||
                    strcmp(name, "file_exists") == 0) return &type_bool;
                if (strcmp(name, "get_mouse_position") == 0) {
                    Type *existing = type_table_find(compiler->type_table, "[]f64");
                    if (existing) return existing;
                    Type *arr_type = malloc(sizeof(Type));
                    arr_type->kind = TYPE_ARRAY;
                    arr_type->element_type = &type_number;
                    arr_type->name = strdup("[]f64");
                    type_table_register(compiler->type_table, arr_type);
                    return arr_type;
                }
            }
            return &type_unknown;
        }
        default:
            return &type_unknown;
    }
}

static int add_local(Compiler *compiler, const char *name, Type *type) {
    if (compiler->local_count >= 1024) return -1;
    Local *local = &compiler->locals[compiler->local_count++];
    strncpy(local->name, name, 63);
    local->name[63] = '\0';
    local->depth = compiler->scope_depth;
    local->type = type ? type : &type_unknown;
    return compiler->local_count - 1;
}

static int resolve_global(Compiler *compiler, const char *name) {
    if (name == NULL) return -1;
    for (int i = 0; i < compiler->function->chunk->constants.count; i++) {
        Value val = compiler->function->chunk->constants.values[i];
        if (val.kind == VALUE_STRING) {
            const char* existing_name = (const char*)val.as.obj;
            if (existing_name && strcmp(existing_name, name) == 0) return i;
        }
    }
    Value name_value = {VALUE_STRING, {.obj = (Obj*)name}};
    int index = chunk_add_constant(compiler->function->chunk, name_value);
    return index;
}

static int compile_lvalue(Compiler *compiler, AstLvalueExpression *lvalue) {
    switch (lvalue->kind) {
    case AST_LVALUE_IDENTIFIER: {
        const char *name = lvalue->expr.identifier;
        if (name && name[0] == '.') {
            emit_constant(compiler, (Value){VALUE_STRING, {.obj = (Obj*)name}}, 0);
            return 0;
        }
        int local = resolve_local(compiler, name);
        if (local >= 0) {
            emit_opcode(compiler, OP_LOAD_LOCAL, 0);
            emit_byte(compiler, (uint8_t)local, 0);
            return 0;
        }
        int index = resolve_global(compiler, lvalue->expr.identifier);
        if (index >= 0) {
            emit_opcode(compiler, OP_GET_GLOBAL, 0);
            emit_byte(compiler, (uint8_t)index, 0);
            return 0;
        }
        break;
    }
    case AST_LVALUE_DOT: {
        if (lvalue->expr.dot.base->kind == AST_EXPR_LVALUE && lvalue->expr.dot.base->expr.lvalue.kind == AST_LVALUE_IDENTIFIER) {
            const char *alias = lvalue->expr.dot.base->expr.lvalue.expr.identifier;
            if (resolve_local(compiler, alias) < 0) {
                char namespaced_name[256];
                snprintf(namespaced_name, sizeof(namespaced_name), "%s_%s", alias, lvalue->expr.dot.identifier);
                int index = resolve_global(compiler, namespaced_name);
                if (index >= 0) {
                    emit_opcode(compiler, OP_GET_GLOBAL, 0);
                    emit_byte(compiler, (uint8_t)index, 0);
                    return 0;
                }
            }
        }
        Type *base_type = get_expression_type(compiler, lvalue->expr.dot.base);
        if (base_type != &type_unknown && base_type->kind != TYPE_STRUCT) {
            if ((base_type->kind == TYPE_ARRAY || base_type->kind == TYPE_STRING) && strcmp(lvalue->expr.dot.identifier, "len") == 0) {
                // OK
            } else {
                fprintf(stderr, "Type error: cannot access property %s of non-struct type %s\n", lvalue->expr.dot.identifier, base_type->name);
                return 1;
            }
        }
        if (base_type != &type_unknown && base_type->kind == TYPE_STRUCT) {
            bool found = false;
            for (size_t i = 0; i < base_type->struct_info.count; i++) {
                if (strcmp(base_type->struct_info.names[i], lvalue->expr.dot.identifier) == 0) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                fprintf(stderr, "Type error: struct %s has no property %s\n", base_type->name, lvalue->expr.dot.identifier);
                return 1;
            }
        }
        if (compile_expression(compiler, lvalue->expr.dot.base)) return 1;
        int name_index = resolve_global(compiler, lvalue->expr.dot.identifier);
        emit_opcode(compiler, OP_GET_FIELD, 0);
        emit_byte(compiler, (uint8_t)name_index, 0);
        return 0;
    }
    case AST_LVALUE_BRACKETS: {
        Type *base_type = get_expression_type(compiler, lvalue->expr.brackets.base);
        Type *index_type = get_expression_type(compiler, lvalue->expr.brackets.index);
        if (base_type != &type_unknown && base_type->kind != TYPE_ARRAY && base_type->kind != TYPE_STRING) {
            fprintf(stderr, "Type error: cannot index into non-array/string type %s\n", base_type->name);
            return 1;
        }
        if (index_type != &type_unknown && !types_equal(index_type, &type_number)) {
            fprintf(stderr, "Type error: index must be a number, got %s\n", index_type->name);
            return 1;
        }
        if (compile_expression(compiler, lvalue->expr.brackets.base)) return 1;
        if (compile_expression(compiler, lvalue->expr.brackets.index)) return 1;
        emit_opcode(compiler, OP_INDEX_GET, 0);
        return 0;
    }
    }
    emit_opcode(compiler, OP_NULL, 0);
    return 0;
}

static int compile_binary(Compiler *compiler, AstBinaryExpression *binary) {
    Type *left_type = get_expression_type(compiler, binary->left);
    Type *right_type = get_expression_type(compiler, binary->right);

    if (left_type != &type_unknown && right_type != &type_unknown) {
        if (binary->op == TOKEN_PLUS) {
            if (types_equal(left_type, &type_number) && types_equal(right_type, &type_number)) {
                // OK
            } else if (types_equal(left_type, &type_string) && types_equal(right_type, &type_string)) {
                // OK
            } else {
                fprintf(stderr, "Type error: invalid operands for + (%s and %s)\n", left_type->name, right_type->name);
                return 1;
            }
        } else if (binary->op >= TOKEN_MINUS && binary->op <= TOKEN_PERCENT) {
            if (!types_equal(left_type, &type_number) || !types_equal(right_type, &type_number)) {
                fprintf(stderr, "Type error: arithmetic operators require numbers\n");
                return 1;
            }
        } else if (binary->op >= TOKEN_DOUBLE_EQUALS && binary->op <= TOKEN_GREATER_EQUALS) {
            if (!types_equal(left_type, right_type)) {
                fprintf(stderr, "Type error: comparison requires same types (%s and %s)\n", left_type->name, right_type->name);
                return 1;
            }
        } else if (binary->op == TOKEN_AND || binary->op == TOKEN_OR) {
            if (!types_equal(left_type, &type_bool) || !types_equal(right_type, &type_bool)) {
                fprintf(stderr, "Type error: logical operators require booleans (%s and %s)\n", left_type->name, right_type->name);
                return 1;
            }
        }
    }

    if (binary->op == TOKEN_AND) {
        if (compile_expression(compiler, binary->left)) return 1;
        size_t false_jump = emit_jump(compiler, OP_JUMP_IF_FALSE, 0);
        if (compile_expression(compiler, binary->right)) return 1;
        size_t end_jump = emit_jump(compiler, OP_JUMP, 0);
        patch_jump(compiler, false_jump);
        emit_opcode(compiler, OP_FALSE, 0);
        patch_jump(compiler, end_jump);
        return 0;
    } else if (binary->op == TOKEN_OR) {
        if (compile_expression(compiler, binary->left)) return 1;
        size_t false_jump = emit_jump(compiler, OP_JUMP_IF_FALSE, 0);
        emit_opcode(compiler, OP_TRUE, 0);
        size_t end_jump = emit_jump(compiler, OP_JUMP, 0);
        patch_jump(compiler, false_jump);
        if (compile_expression(compiler, binary->right)) return 1;
        patch_jump(compiler, end_jump);
        return 0;
    }

    if (compile_expression(compiler, binary->left)) return 1;
    if (compile_expression(compiler, binary->right)) return 1;
    switch (binary->op) {
    case TOKEN_PLUS:     emit_opcode(compiler, OP_ADD, 0); break;
    case TOKEN_MINUS:    emit_opcode(compiler, OP_SUB, 0); break;
    case TOKEN_ASTERISK:  emit_opcode(compiler, OP_MUL, 0); break;
    case TOKEN_SLASH:    emit_opcode(compiler, OP_DIV, 0); break;
    case TOKEN_PERCENT:  emit_opcode(compiler, OP_MOD, 0); break;
    case TOKEN_DOUBLE_EQUALS: emit_opcode(compiler, OP_EQ, 0); break;
    case TOKEN_NOT_EQUALS:   emit_opcode(compiler, OP_NE, 0); break;
    case TOKEN_LESS:           emit_opcode(compiler, OP_LT, 0); break;
    case TOKEN_GREATER:        emit_opcode(compiler, OP_GT, 0); break;
    case TOKEN_LESS_EQUALS:    emit_opcode(compiler, OP_LE, 0); break;
    case TOKEN_GREATER_EQUALS: emit_opcode(compiler, OP_GE, 0); break;
    default: return 1;
    }
    return 0;
}

static int compile_unary(Compiler *compiler, AstUnaryExpression *unary) {
    Type *operand_type = get_expression_type(compiler, unary->operand);
    if (operand_type != &type_unknown) {
        if (unary->op == TOKEN_MINUS && !types_equal(operand_type, &type_number)) {
            fprintf(stderr, "Type error: - operator requires a number\n");
            return 1;
        }
        if (unary->op == TOKEN_EXCLAMATION_MARK && !types_equal(operand_type, &type_bool)) {
            fprintf(stderr, "Type error: ! operator requires a boolean\n");
            return 1;
        }
    }
    if (compile_expression(compiler, unary->operand)) return 1;
    switch (unary->op) {
    case TOKEN_EXCLAMATION_MARK: emit_opcode(compiler, OP_NOT, 0); break;
    case TOKEN_MINUS:          emit_opcode(compiler, OP_NEG, 0); break;
    default: return 1;
    }
    return 0;
}

static Type *get_lvalue_type(Compiler *compiler, AstLvalueExpression *lvalue) {
    if (!lvalue) return &type_unknown;
    switch (lvalue->kind) {
        case AST_LVALUE_IDENTIFIER: {
            for (int i = compiler->local_count - 1; i >= 0; i--) {
                if (strcmp(compiler->locals[i].name, lvalue->expr.identifier) == 0) {
                    return compiler->locals[i].type;
                }
            }
            if (compiler->params) {
                for (int i = 0; i < compiler->param_count; i++) {
                    if (compiler->params[i] && strcmp(compiler->params[i], lvalue->expr.identifier) == 0) {
                        return compiler->param_types ? compiler->param_types[i] : &type_unknown;
                    }
                }
            }
            Type *t = type_table_find(compiler->type_table, lvalue->expr.identifier);
            if (t) {
                if (t->kind == TYPE_VARIABLE) return t->element_type;
                return t;
            }
            return &type_unknown;
        }
        case AST_LVALUE_DOT: {
            if (lvalue->expr.dot.base->kind == AST_EXPR_LVALUE && lvalue->expr.dot.base->expr.lvalue.kind == AST_LVALUE_IDENTIFIER) {
                const char *alias = lvalue->expr.dot.base->expr.lvalue.expr.identifier;
                if (resolve_local(compiler, alias) < 0) {
                    char namespaced_name[256];
                    snprintf(namespaced_name, sizeof(namespaced_name), "%s_%s", alias, lvalue->expr.dot.identifier);
                    Type *t = type_table_find(compiler->type_table, namespaced_name);
                    if (t) {
                        if (t->kind == TYPE_VARIABLE) return t->element_type;
                        return t;
                    }
                }
            }
            Type *base_type = get_expression_type(compiler, lvalue->expr.dot.base);
            if (base_type && (base_type->kind == TYPE_ARRAY || base_type->kind == TYPE_STRING) && strcmp(lvalue->expr.dot.identifier, "len") == 0) {
                return &type_number;
            }
            if (base_type && base_type->kind == TYPE_STRUCT) {
                for (size_t i = 0; i < base_type->struct_info.count; i++) {
                    if (strcmp(base_type->struct_info.names[i], lvalue->expr.dot.identifier) == 0) {
                        return base_type->struct_info.types[i];
                    }
                }
            }
            return &type_unknown;
        }
        case AST_LVALUE_BRACKETS: {
            Type *base_type = get_expression_type(compiler, lvalue->expr.brackets.base);
            if (base_type && base_type->kind == TYPE_ARRAY) {
                return base_type->element_type;
            }
            if (base_type && base_type->kind == TYPE_STRING) {
                return &type_string;
            }
            return &type_unknown;
        }
    }
    return &type_unknown;
}

static int compile_expression(Compiler *compiler, AstExpression *expr) {
    if (!expr) return 0;
    switch (expr->kind) {
    case AST_EXPR_LITERAL:   return compile_literal(compiler, &expr->expr.literal);
    case AST_EXPR_BINARY:   return compile_binary(compiler, &expr->expr.binary);
    case AST_EXPR_UNARY:    return compile_unary(compiler, &expr->expr.unary);
    case AST_EXPR_LVALUE:  return compile_lvalue(compiler, &expr->expr.lvalue);
    case AST_EXPR_ARRAY_LITERAL: {
        AstArrayLiteralExpression *array = &expr->expr.array_literal;
        for (size_t i = 0; i < array->element_count; i++) {
            if (compile_expression(compiler, &array->elements[i])) return 1;
        }
        emit_opcode(compiler, OP_ARRAY, 0);
        emit_byte(compiler, (uint8_t)array->element_count, 0);
        return 0;
    }
    case AST_EXPR_GROUP: {
        AstGroupExpression *group = &expr->expr.group;
        if (group->expr) return compile_expression(compiler, group->expr);
        emit_opcode(compiler, OP_NULL, 0);
        return 0;
    }
    case AST_EXPR_ASSIGNMENT: {
        AstAssignmentExpression *assign = &expr->expr.assignment;
        Type *left_type = get_lvalue_type(compiler, assign->left);
        Type *right_type = get_expression_type(compiler, assign->right);
        if (left_type != &type_unknown && right_type != &type_unknown && right_type != &type_null && !types_equal(left_type, right_type)) {
            fprintf(stderr, "Type error: cannot assign %s to %s\n", right_type->name, left_type->name);
            return 1;
        }
        if (compile_expression(compiler, assign->right)) return 1;
        if (assign->left->kind == AST_LVALUE_IDENTIFIER) {
            int local = resolve_local(compiler, assign->left->expr.identifier);
            if (local >= 0) {
                emit_opcode(compiler, OP_STORE_LOCAL, 0);
                emit_byte(compiler, (uint8_t)local, 0);
                return 0;
            }
            int index = resolve_global(compiler, assign->left->expr.identifier);
            if (index >= 0) {
                emit_opcode(compiler, OP_STORE_GLOBAL, 0);
                emit_byte(compiler, (uint8_t)index, 0);
                return 0;
            }
        } else if (assign->left->kind == AST_LVALUE_DOT) {
            if (assign->left->expr.dot.base->kind == AST_EXPR_LVALUE && assign->left->expr.dot.base->expr.lvalue.kind == AST_LVALUE_IDENTIFIER) {
                const char *alias = assign->left->expr.dot.base->expr.lvalue.expr.identifier;
                if (resolve_local(compiler, alias) < 0) {
                    char namespaced_name[256];
                    snprintf(namespaced_name, sizeof(namespaced_name), "%s_%s", alias, assign->left->expr.dot.identifier);
                    int index = resolve_global(compiler, namespaced_name);
                    if (index >= 0) {
                        emit_opcode(compiler, OP_STORE_GLOBAL, 0);
                        emit_byte(compiler, (uint8_t)index, 0);
                        return 0;
                    }
                }
            }
            if (compile_expression(compiler, assign->left->expr.dot.base)) return 1;
            int name_index = resolve_global(compiler, assign->left->expr.dot.identifier);
            emit_opcode(compiler, OP_SET_FIELD, 0);
            emit_byte(compiler, (uint8_t)name_index, 0);
            return 0;
        } else if (assign->left->kind == AST_LVALUE_BRACKETS) {
            if (compile_expression(compiler, assign->left->expr.brackets.base)) return 1;
            if (compile_expression(compiler, assign->left->expr.brackets.index)) return 1;
            emit_opcode(compiler, OP_INDEX_SET, 0);
            return 0;
        }
        return 0;
    }
    case AST_EXPR_FUNCTION_CALL: {
        AstFunctionCallExpression *call = &expr->expr.function_call;
        if (call->function->kind == AST_EXPR_LVALUE && 
            call->function->expr.lvalue.kind == AST_LVALUE_IDENTIFIER &&
            strcmp(call->function->expr.lvalue.expr.identifier, "print") == 0) {
            return compile_print(compiler, call->function, call->args, call->arg_count);
        }
        if (call->arg_names != NULL) {
            for (size_t i = 0; i < call->arg_count; i++) {
                if (compile_expression(compiler, &call->args[i])) return 1;
            }
            emit_opcode(compiler, OP_STRUCT, 0);
            emit_byte(compiler, (uint8_t)call->arg_count, 0);
            for (size_t i = 0; i < call->arg_count; i++) {
                int name_index = resolve_global(compiler, call->arg_names[i]);
                emit_byte(compiler, (uint8_t)name_index, 0);
            }
            return 0;
        }
        if (call->function->kind == AST_EXPR_LVALUE && 
            call->function->expr.lvalue.kind == AST_LVALUE_DOT) {
            AstExpression *base = call->function->expr.lvalue.expr.dot.base;
            const char *member = call->function->expr.lvalue.expr.dot.identifier;
            
            // Prüfen, ob die Basis ein Strukturtyp für benutzerdefinierte Methoden ist
            Type *base_type = get_expression_type(compiler, base);
            if (base_type && base_type->kind == TYPE_STRUCT) {
                char method_name[256];
                snprintf(method_name, sizeof(method_name), "%s_%s", base_type->name, member);
                int index = resolve_global(compiler, method_name);
                
                // Optional: Typüberprüfung der Methodenaufrufparameter
                Type *ft = type_table_find(compiler->type_table, method_name);
                if (ft && ft->kind == TYPE_FUNCTION) {
                    if (ft->fn_info.param_count != call->arg_count + 1) {
                        fprintf(stderr, "Type error: method %s expects %zu args but got %zu (excluding self)\n", 
                                ft->name, ft->fn_info.param_count - 1, call->arg_count);
                        return 1;
                    }
                    Type *expected_self = ft->fn_info.params[0];
                    if (expected_self != &type_unknown && !types_equal(expected_self, base_type)) {
                        fprintf(stderr, "Type error: self parameter of method %s expects %s but got %s\n",
                                ft->name, expected_self->name, base_type->name);
                        return 1;
                    }
                    for (size_t i = 0; i < call->arg_count; i++) {
                        Type *expected = ft->fn_info.params[i + 1];
                        Type *actual = get_expression_type(compiler, &call->args[i]);
                        if (expected != &type_unknown && !types_equal(expected, actual)) {
                            fprintf(stderr, "Type error: arg %zu of method %s expects %s but got %s\n",
                                    i, ft->name, expected->name, actual->name);
                            return 1;
                        }
                    }
                }
                
                emit_opcode(compiler, OP_GET_GLOBAL, 0);
                emit_byte(compiler, (uint8_t)index, 0);
                if (compile_expression(compiler, base)) return 1;
                for (size_t i = 0; i < call->arg_count; i++) {
                    if (compile_expression(compiler, &call->args[i])) return 1;
                }
                emit_opcode(compiler, OP_CALL, 0);
                emit_byte(compiler, (uint8_t)(call->arg_count + 1), 0);
                return 0;
            }
            
            // Prüfen, ob die Basis ein qualifizierter Modul-Alias ist
            if (base->kind == AST_EXPR_LVALUE && base->expr.lvalue.kind == AST_LVALUE_IDENTIFIER) {
                const char *alias = base->expr.lvalue.expr.identifier;
                if (resolve_local(compiler, alias) < 0) {
                    char namespaced_name[256];
                    snprintf(namespaced_name, sizeof(namespaced_name), "%s_%s", alias, member);
                    int index = resolve_global(compiler, namespaced_name);
                    
                    // Optional: Typüberprüfung der Parameter von Funktionen mit Namensraum
                    Type *ft = type_table_find(compiler->type_table, namespaced_name);
                    if (ft && ft->kind == TYPE_FUNCTION) {
                        if (ft->fn_info.param_count != call->arg_count) {
                            fprintf(stderr, "Type error: function %s expects %zu args but got %zu\n", 
                                    ft->name, ft->fn_info.param_count, call->arg_count);
                            return 1;
                        }
                        for (size_t i = 0; i < call->arg_count; i++) {
                            Type *expected = ft->fn_info.params[i];
                            Type *actual = get_expression_type(compiler, &call->args[i]);
                            if (expected != &type_unknown && !types_equal(expected, actual)) {
                                fprintf(stderr, "Type error: arg %zu of %s expects %s but got %s\n",
                                        i, ft->name, expected->name, actual->name);
                                return 1;
                            }
                        }
                    }
                    
                    emit_opcode(compiler, OP_GET_GLOBAL, 0);
                    emit_byte(compiler, (uint8_t)index, 0);
                    for (size_t i = 0; i < call->arg_count; i++) {
                        if (compile_expression(compiler, &call->args[i])) return 1;
                    }
                    emit_opcode(compiler, OP_CALL, 0);
                    emit_byte(compiler, (uint8_t)call->arg_count, 0);
                    return 0;
                }
            }
        }
        if (call->function->kind == AST_EXPR_LVALUE && call->function->expr.lvalue.kind == AST_LVALUE_IDENTIFIER) {
            Type *ft = type_table_find(compiler->type_table, call->function->expr.lvalue.expr.identifier);
            if (ft && ft->kind == TYPE_FUNCTION) {
                if (ft->fn_info.param_count != call->arg_count) {
                    fprintf(stderr, "Type error: function %s expects %zu args but got %zu\n", ft->name, ft->fn_info.param_count, call->arg_count);
                    return 1;
                }
                for (size_t i = 0; i < call->arg_count; i++) {
                    Type *expected = ft->fn_info.params[i];
                    Type *actual = get_expression_type(compiler, &call->args[i]);
                    if (expected != &type_unknown && !types_equal(expected, actual)) {
                        fprintf(stderr, "Type error: arg %zu of %s expects %s but got %s\n", i, ft->name, expected->name, actual->name);
                        return 1;
                    }
                }
            }
        }
        if (compile_expression(compiler, call->function)) return 1;
        for (size_t i = 0; i < call->arg_count; i++) {
            if (compile_expression(compiler, call->args + i)) return 1;
        }
        emit_opcode(compiler, OP_CALL, 0);
        emit_byte(compiler, (uint8_t)call->arg_count, 0);
        return 0;
    }
    default:
        emit_opcode(compiler, OP_NULL, 0);
        return 0;
    }
}

static int compile_var_decl(Compiler *compiler, AstVarDeclStatement *decl) {
    Type *declared_type = NULL;
    if (decl->type_name) {
        declared_type = parse_type_name(compiler->type_table, decl->type_name);
        if (!declared_type) {
            fprintf(stderr, "Type error: unknown type %s for variable %s\n", decl->type_name, decl->name);
            return 1;
        }
    }

    Type *init_type = decl->initializer ? get_expression_type(compiler, decl->initializer) : &type_null;
    if (declared_type && init_type != &type_null && init_type != &type_unknown && !types_equal(declared_type, init_type)) {
        fprintf(stderr, "Type error: cannot assign %s to variable of type %s at %s\n", init_type->name, declared_type->name, decl->name);
        return 1;
    }
    if (decl->initializer) {
        if (compile_expression(compiler, decl->initializer)) return 1;
    }
    else emit_opcode(compiler, OP_NULL, 0);
    if (compiler->scope_depth > 0) {
        add_local(compiler, decl->name, declared_type ? declared_type : init_type);
    } else {
        Type *final_type = declared_type ? declared_type : init_type;
        // Typ der globalen Variable registrieren, falls noch nicht durch register_types registriert
        Type *existing = type_table_find(compiler->type_table, decl->name);
        if (!existing || existing->kind != TYPE_VARIABLE) {
            Type *var_type = calloc(1, sizeof(Type));
            var_type->kind = TYPE_VARIABLE;
            var_type->name = strdup(decl->name);
            var_type->element_type = final_type;
            type_table_register(compiler->type_table, var_type);
        } else {
            // Typ aktualisieren, wenn er unbekannt war oder wir jetzt einen besseren haben
            existing->element_type = final_type;
        }

        int index = resolve_global(compiler, decl->name);
        emit_opcode(compiler, OP_DEFINE_GLOBAL, 0);
        emit_byte(compiler, (uint8_t)index, 0);
    }
    return 0;
}

static int compile_return(Compiler *compiler, AstReturnStatement *ret) {
    Type *actual_type = ret->value ? get_expression_type(compiler, ret->value) : &type_void;
    if (compiler->return_type != &type_unknown && !types_equal(compiler->return_type, actual_type)) {
        if (!(types_equal(compiler->return_type, &type_void) && types_equal(actual_type, &type_null))) {
            fprintf(stderr, "Type error: function returns %s but expected %s\n", actual_type->name, compiler->return_type->name);
            return 1;
        }
    }
    if (ret->value) {
        if (compile_expression(compiler, ret->value)) return 1;
    }
    else emit_opcode(compiler, OP_NULL, 0);
    emit_opcode(compiler, OP_RETURN, 0);
    return 0;
}

static int compile_block(Compiler *compiler, AstBlock *block) {
    begin_scope(compiler);
    for (size_t i = 0; i < block->len; i++) {
        if (compile_statement(compiler, block->statements + i)) return 1;
    }
    end_scope(compiler);
    return 0;
}

static int compile_if(Compiler *compiler, AstIfStatement *if_stmt) {
    if (compile_expression(compiler, if_stmt->condition)) return 1;
    size_t else_offset = emit_jump(compiler, OP_JUMP_IF_FALSE, 0);
    if (compile_block(compiler, if_stmt->then_block)) return 1;
    if (if_stmt->else_branch) {
        size_t end_offset = emit_jump(compiler, OP_JUMP, 0);
        patch_jump(compiler, else_offset);
        if (compile_statement(compiler, if_stmt->else_branch)) return 1;
        patch_jump(compiler, end_offset);
    } else {
        patch_jump(compiler, else_offset);
    }
    return 0;
}

static int compile_while(Compiler *compiler, AstWhileStatement *while_stmt) {
    size_t loop_start = compiler->function->chunk->count;
    if (compile_expression(compiler, while_stmt->condition)) return 1;
    size_t exit_offset = emit_jump(compiler, OP_JUMP_IF_FALSE, 0);
    Loop loop;
    start_loop(compiler, &loop, loop_start);
    if (compile_block(compiler, while_stmt->body)) return 1;
    size_t loop_offset = emit_loop(compiler, 0);
    patch_loop(compiler, loop_offset, loop_start);
    patch_jump(compiler, exit_offset);
    end_loop(compiler, loop_start);
    return 0;
}

static int compile_for_range(Compiler *compiler, AstForRangeStatement *for_stmt) {
    int loop_var_index = resolve_global(compiler, for_stmt->name);
    if (compile_expression(compiler, for_stmt->start)) return 1;
    emit_opcode(compiler, OP_DEFINE_GLOBAL, 0);
    emit_byte(compiler, (uint8_t)loop_var_index, 0);
    size_t condition_start = compiler->function->chunk->count;
    emit_opcode(compiler, OP_GET_GLOBAL, 0);
    chunk_write_byte(compiler->function->chunk, loop_var_index, 0);
    if (compile_expression(compiler, for_stmt->end)) return 1;
    emit_opcode(compiler, OP_LT, 0);
    size_t exit_jump = emit_jump(compiler, OP_JUMP_IF_FALSE, 0);
    size_t body_jump = emit_jump(compiler, OP_JUMP, 0);
    size_t increment_start = compiler->function->chunk->count;
    emit_opcode(compiler, OP_GET_GLOBAL, 0);
    chunk_write_byte(compiler->function->chunk, loop_var_index, 0);
    emit_constant(compiler, (Value){VALUE_NUMBER, {.number = 1}}, 0);
    emit_opcode(compiler, OP_ADD, 0);
    emit_opcode(compiler, OP_STORE_GLOBAL, 0);
    chunk_write_byte(compiler->function->chunk, loop_var_index, 0);
    emit_opcode(compiler, OP_POP, 0);
    size_t loop_to_cond = emit_loop(compiler, 0);
    patch_loop(compiler, loop_to_cond, condition_start);
    patch_jump(compiler, body_jump);
    Loop loop;
    start_loop(compiler, &loop, condition_start);
    if (compile_block(compiler, for_stmt->body)) return 1;
    size_t loop_to_inc = emit_loop(compiler, 0);
    patch_loop(compiler, loop_to_inc, increment_start);
    patch_jump(compiler, exit_jump);
    end_loop(compiler, increment_start);
    return 0;
}

static int compile_fn_decl(Compiler *compiler, AstFnDeclStatement *decl) {
    int index = resolve_global(compiler, decl->name);
    Compiler sub_compiler;
    compiler_init(&sub_compiler, compiler);
    sub_compiler.function->arity = (int)decl->param_count;
    sub_compiler.params = decl->params;
    sub_compiler.param_count = (int)decl->param_count;
    Type *ft = type_table_find(compiler->type_table, decl->name);
    if (ft && ft->kind == TYPE_FUNCTION) {
        sub_compiler.param_types = ft->fn_info.params;
    } else {
        sub_compiler.param_types = NULL;
    }
    sub_compiler.return_type = parse_type_name(compiler->type_table, decl->return_type);
    if (sub_compiler.return_type == NULL) sub_compiler.return_type = &type_void;

    begin_scope(&sub_compiler);
    size_t i;
    for (i = 0; i < decl->body->len; i++) {
        if (compile_statement(&sub_compiler, decl->body->statements + i)) return 1;
    }
    
    emit_opcode(&sub_compiler, OP_NULL, 0);
    emit_opcode(&sub_compiler, OP_RETURN, 0);
    
    Value fn_val = {VALUE_FUNCTION, {.obj = (Obj*)sub_compiler.function}};
    emit_constant(compiler, fn_val, 0);
    emit_opcode(compiler, OP_DEFINE_GLOBAL, 0);
    emit_byte(compiler, (uint8_t)index, 0);
    
    return 0;
}

static int compile_switch(Compiler *compiler, AstSwitchStatement *sw) {
    if (compile_expression(compiler, sw->subject)) return 1;
    size_t *end_jumps = malloc(sizeof(size_t) * sw->clause_count);
    if (!end_jumps) return -1;
    int end_jump_count = 0;
    size_t i;
    for (i = 0; i < sw->clause_count; i++) {
        AstSwitchClause *clause = sw->clauses + i;
        size_t next_clause_jump = 0;
        int has_next_jump = 0;
        if (!clause->is_default) {
            emit_opcode(compiler, OP_DUP, 0);
            if (compile_expression(compiler, clause->value)) return 1;
            emit_opcode(compiler, OP_EQ, 0);
            next_clause_jump = emit_jump(compiler, OP_JUMP_IF_FALSE, 0);
            has_next_jump = 1;
        }
        size_t j;
        for (j = 0; j < clause->len; j++) {
            if (compile_statement(compiler, clause->statements + j)) return 1;
        }
        if (has_next_jump) {
            end_jumps[end_jump_count++] = emit_jump(compiler, OP_JUMP, 0);
            patch_jump(compiler, next_clause_jump);
        }
    }
    for (int k = 0; k < end_jump_count; k++) patch_jump(compiler, end_jumps[k]);
    emit_opcode(compiler, OP_POP, 0);
    free(end_jumps);
    return 0;
}

static int compile_statement(Compiler *compiler, AstStatement *stmt) {
    switch (stmt->kind) {
    case AST_STMT_VAR_DECL:  return compile_var_decl(compiler, &stmt->stmt.var_decl);
    case AST_STMT_RETURN:     return compile_return(compiler, &stmt->stmt.return_stmt);
    case AST_STMT_IF:         return compile_if(compiler, &stmt->stmt.if_stmt);
    case AST_STMT_WHILE:     return compile_while(compiler, &stmt->stmt.while_stmt);
    case AST_STMT_FOR_RANGE:  return compile_for_range(compiler, &stmt->stmt.for_stmt);
    case AST_STMT_SWITCH:    return compile_switch(compiler, &stmt->stmt.switch_stmt);
    case AST_STMT_FN_DECL:   return compile_fn_decl(compiler, &stmt->stmt.fn_decl);
    case AST_STMT_EXPR: {
        if (stmt->stmt.expr_stmt.expr) {
            if (compile_expression(compiler, stmt->stmt.expr_stmt.expr)) return 1;
            emit_opcode(compiler, OP_POP, 0);
        }
        return 0;
    }
    case AST_STMT_BLOCK: return compile_block(compiler, &stmt->stmt.block);
    case AST_STMT_BREAK: return compile_break(compiler);
    case AST_STMT_CONTINUE: return compile_continue(compiler);
    default: return 0;
    }
}

void compiler_init(Compiler *compiler, Compiler *enclosing) {
    compiler->enclosing = enclosing;
    compiler->function = malloc(sizeof(ObjFunction));
    compiler->function->obj.type = OBJ_FUNCTION;
    compiler->function->obj.next = NULL;
    compiler->function->obj.is_marked = 0;
    compiler->function->arity = 0;
    compiler->function->upvalue_count = 0;
    compiler->function->upvalues = NULL;
    compiler->function->chunk = malloc(sizeof(Chunk));
    chunk_init(compiler->function->chunk);
    compiler->params = NULL;
    compiler->param_types = NULL;
    compiler->param_count = 0;
    compiler->local_count = 0;
    compiler->scope_depth = 0;
    if (enclosing) {
        compiler->string_count = enclosing->string_count;
        memcpy(compiler->strings, enclosing->strings, sizeof(compiler->strings));
        compiler->type_table = enclosing->type_table;
        compiler->return_type = &type_void;
    } else {
        compiler->string_count = 0;
        compiler->type_table = malloc(sizeof(TypeTable));
        compiler->return_type = &type_void;
        type_table_init(compiler->type_table);
    }
    compiler->current_loop = NULL;
}

void compiler_free(Compiler *compiler) {
    if (compiler->enclosing == NULL) {
        type_table_free(compiler->type_table);
        free(compiler->type_table);
    }
}

int compiler_compile(Compiler *compiler, const char *source) {
    TokenStream tokens;
    token_stream_init(&tokens, lexer_init(source));
    AstProgram program;
    if (!ast_program_parse(&tokens, &program, NULL)) {
        token_stream_free(&tokens);
        return 1;
    }
    compiler_init(compiler, NULL);
    int result = compiler_compile_ast(compiler, &program);
    ast_program_free(&program);
    token_stream_free(&tokens);
    return result;
}

static void register_types(Compiler *compiler, AstProgram *program) {
    for (size_t i = 0; i < program->len; i++) {
        AstStatement *stmt = &program->statements[i];
        if (stmt->kind == AST_STMT_TYPE_DECL) {
            AstTypeDeclStatement *decl = &stmt->stmt.type_decl;
            if (!decl->is_enum) {
                Type *struct_type = calloc(1, sizeof(Type));
                struct_type->kind = TYPE_STRUCT;
                struct_type->name = strdup(decl->name);
                struct_type->struct_info.count = decl->field_count;
                struct_type->struct_info.names = malloc(sizeof(char *) * decl->field_count);
                struct_type->struct_info.types = malloc(sizeof(Type *) * decl->field_count);
                for (size_t j = 0; j < decl->field_count; j++) {
                    struct_type->struct_info.names[j] = strdup(decl->fields[j]);
                    struct_type->struct_info.types[j] = parse_type_name(compiler->type_table, decl->field_types ? decl->field_types[j] : NULL);
                }
                type_table_register(compiler->type_table, struct_type);
            }
        } else if (stmt->kind == AST_STMT_FN_DECL) {
            AstFnDeclStatement *decl = &stmt->stmt.fn_decl;
            Type *fn_type = calloc(1, sizeof(Type));
            fn_type->kind = TYPE_FUNCTION;
            fn_type->name = strdup(decl->name);
            fn_type->fn_info.return_type = parse_type_name(compiler->type_table, decl->return_type);
            if (fn_type->fn_info.return_type == NULL) fn_type->fn_info.return_type = &type_void;
            fn_type->fn_info.param_count = decl->param_count;
            fn_type->fn_info.params = malloc(sizeof(Type*) * decl->param_count);
            for (size_t j = 0; j < decl->param_count; j++) {
                fn_type->fn_info.params[j] = parse_type_name(compiler->type_table, decl->param_types ? decl->param_types[j] : NULL);
                if (fn_type->fn_info.params[j] == NULL) fn_type->fn_info.params[j] = &type_unknown;
            }
            type_table_register(compiler->type_table, fn_type);
        } else if (stmt->kind == AST_STMT_VAR_DECL) {
            AstVarDeclStatement *decl = &stmt->stmt.var_decl;
            Type *t = parse_type_name(compiler->type_table, decl->type_name);
            if (t) {
                Type *var_type = calloc(1, sizeof(Type));
                var_type->kind = TYPE_VARIABLE;
                var_type->name = strdup(decl->name);
                var_type->element_type = t;
                type_table_register(compiler->type_table, var_type);
            }
        }
    }
}

int compiler_compile_ast(Compiler *compiler, AstProgram *program) {
    register_types(compiler, program);
    size_t i;
    for (i = 0; i < program->len; i++) {
        if (compile_statement(compiler, program->statements + i)) return 1;
    }
    emit_opcode(compiler, OP_NULL, 0);
    emit_opcode(compiler, OP_RETURN, 0);
    return 0;
}

Chunk *compiler_get_chunk(Compiler *compiler) {
    return compiler->function->chunk;
}
