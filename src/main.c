#include "compiler/compiler.h"
#include "lexer/lexer.h"
#include "lexer/token.h"
#include "lexer/token_stream.h"
#include "parser/parser.h"
#include "vm/chunk.h"
#include "vm/vm.h"
#include "vm/stdlib.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <ctype.h>

static int maybe_read_source_file(const char *path, char **out_source) {
    *out_source = NULL;
    FILE *file = fopen(path, "rb");
    if (file == NULL) return 0;
    if (fseek(file, 0, SEEK_END) != 0) { fclose(file); return -1; }
    long size = ftell(file);
    if (size < 0) { fclose(file); return -1; }
    if (fseek(file, 0, SEEK_SET) != 0) { fclose(file); return -1; }
    char *buffer = malloc((size_t)size + 1);
    if (buffer == NULL) { fclose(file); return -1; }
    size_t nread = fread(buffer, 1, (size_t)size, file);
    if (nread != (size_t)size) { free(buffer); fclose(file); return -1; }
    buffer[nread] = '\0';
    fclose(file);
    *out_source = buffer;
    return 1;
}

static void print_parse_error(const char *label, const Token *errtok) {
    fprintf(stderr, "Parse error in %s at %d:%d: %s", label, errtok->line, errtok->col, token_type_name(errtok->type));
    if (errtok->type == TOKEN_IDENTIFIER || errtok->type == TOKEN_KEYWORD || errtok->type == TOKEN_STRING_LITERAL || errtok->type == TOKEN_ERROR) {
        fprintf(stderr, "{%s}", errtok->value.string == NULL ? "" : errtok->value.string);
    } else if (errtok->type == TOKEN_NUMBER_LITERAL) {
        fprintf(stderr, "{%.15g}", errtok->value.number);
    }
    fprintf(stderr, "\n");
}

static int parse_program_source(const char *source, TokenStream *out_stream, AstProgram *out_program, Token *out_errtok) {
    Lexer lexer = lexer_init(source);
    token_stream_init(out_stream, lexer);
    *out_program = (AstProgram){0};
    *out_errtok = (Token){0};
    return ast_program_parse(out_stream, out_program, out_errtok);
}

static char *dup_dirname(const char *path) {
    if (path == NULL) return NULL;
    const char *slash = strrchr(path, '/');
    const char *backslash = strrchr(path, '\\');
    const char *last_slash = (slash > backslash) ? slash : backslash;

    if (last_slash == NULL) { char *dir = malloc(2); dir[0] = '.'; dir[1] = '\0'; return dir; }
    if (last_slash == path) { 
        char *dir = malloc(2); 
        dir[0] = last_slash[0]; 
        dir[1] = '\0'; 
        return dir; 
    }
    size_t len = (size_t)(last_slash - path);
    char *dir = malloc(len + 1);
    memcpy(dir, path, len);
    dir[len] = '\0';
    return dir;
}

static int has_ahhhh_extension(const char *path) {
    if (path == NULL) return 0;
    size_t len = strlen(path);
    return len >= 6 && strcmp(path + len - 6, ".ahhhh") == 0;
}

static int is_absolute_path(const char *path) {
    if (path == NULL) return 0;
    if (path[0] == '/' || path[0] == '\\') return 1;
    // Check for Windows drive letter (e.g., C:\)
    if (strlen(path) >= 3 && isalpha((unsigned char)path[0]) && path[1] == ':' && (path[2] == '/' || path[2] == '\\')) return 1;
    return 0;
}

static int try_load_include(const char *candidate, const char *input_dir, char **out_source) {
    int rc = maybe_read_source_file(candidate, out_source);
    if (rc < 0) return -1;
    if (rc > 0) return 0;
    if (input_dir != NULL && !is_absolute_path(candidate)) {
        size_t left_len = strlen(input_dir);
        if (left_len > 0) {
            char last_char = input_dir[left_len - 1];
            char *joined;
            if (last_char == '/' || last_char == '\\') {
                joined = malloc(left_len + strlen(candidate) + 1);
                sprintf(joined, "%s%s", input_dir, candidate);
            } else {
                joined = malloc(left_len + 1 + strlen(candidate) + 1);
                sprintf(joined, "%s/%s", input_dir, candidate);
            }
            rc = maybe_read_source_file(joined, out_source);
            free(joined);
            if (rc < 0) return -1;
            if (rc > 0) return 0;
        }
    }
    return 1;
}

static int load_include(const char *include_path, const char *input_dir, char **out_source) {
    *out_source = NULL;
    int rc = try_load_include(include_path, input_dir, out_source);
    if (rc <= 0) return rc;
    if (has_ahhhh_extension(include_path)) return 1;
    size_t len = strlen(include_path);
    char *with_ext = malloc(len + 7);
    memcpy(with_ext, include_path, len);
    memcpy(with_ext + len, ".ahhhh", 7);
    rc = try_load_include(with_ext, input_dir, out_source);
    free(with_ext);
    return rc;
}

typedef struct {
    char **sources;
    size_t count;
    size_t capacity;
} SourceList;

static void source_list_init(SourceList *list) {
    list->sources = NULL;
    list->count = 0;
    list->capacity = 0;
}

static void source_list_add(SourceList *list, char *source) {
    if (list->count >= list->capacity) {
        list->capacity = list->capacity < 8 ? 8 : list->capacity * 2;
        list->sources = realloc(list->sources, list->capacity * sizeof(char *));
    }
    list->sources[list->count++] = source;
}

static void source_list_free(SourceList *list) {
    for (size_t i = 0; i < list->count; i++) {
        free(list->sources[i]);
    }
    free(list->sources);
}

typedef struct IncludeStack {
    const char *path;
    struct IncludeStack *next;
} IncludeStack;

static void rename_string_if_matches(char **str_ptr, const char *alias, char **decl_names, size_t decl_count) {
    if (str_ptr == NULL || *str_ptr == NULL) return;
    for (size_t i = 0; i < decl_count; i++) {
        if (strcmp(*str_ptr, decl_names[i]) == 0) {
            size_t new_len = strlen(alias) + 1 + strlen(*str_ptr) + 1;
            char *new_str = malloc(new_len);
            if (new_str != NULL) {
                snprintf(new_str, new_len, "%s_%s", alias, *str_ptr);
                free(*str_ptr);
                *str_ptr = new_str;
            }
            return;
        }
    }
}

static void rename_expression(AstExpression *expr, const char *alias, char **decl_names, size_t decl_count);

static void rename_lvalue(AstLvalueExpression *lval, const char *alias, char **decl_names, size_t decl_count) {
    if (lval == NULL) return;
    switch (lval->kind) {
        case AST_LVALUE_IDENTIFIER:
            rename_string_if_matches(&lval->expr.identifier, alias, decl_names, decl_count);
            break;
        case AST_LVALUE_BRACKETS:
            rename_expression(lval->expr.brackets.base, alias, decl_names, decl_count);
            rename_expression(lval->expr.brackets.index, alias, decl_names, decl_count);
            break;
        case AST_LVALUE_DOT:
            rename_expression(lval->expr.dot.base, alias, decl_names, decl_count);
            break;
    }
}

static void rename_expression(AstExpression *expr, const char *alias, char **decl_names, size_t decl_count) {
    if (expr == NULL) return;
    switch (expr->kind) {
        case AST_EXPR_LITERAL:
            break;
        case AST_EXPR_LVALUE:
            rename_lvalue(&expr->expr.lvalue, alias, decl_names, decl_count);
            break;
        case AST_EXPR_ASSIGNMENT:
            rename_lvalue(expr->expr.assignment.left, alias, decl_names, decl_count);
            rename_expression(expr->expr.assignment.right, alias, decl_names, decl_count);
            break;
        case AST_EXPR_COMPOUND_ASSIGN:
            rename_lvalue(expr->expr.compound_assignment.left, alias, decl_names, decl_count);
            rename_expression(expr->expr.compound_assignment.right, alias, decl_names, decl_count);
            break;
        case AST_EXPR_FUNCTION_CALL: {
            AstFunctionCallExpression *call = &expr->expr.function_call;
            rename_expression(call->function, alias, decl_names, decl_count);
            for (size_t i = 0; i < call->arg_count; i++) {
                rename_expression(&call->args[i], alias, decl_names, decl_count);
            }
            break;
        }
        case AST_EXPR_GROUP:
            rename_expression(expr->expr.group.expr, alias, decl_names, decl_count);
            break;
        case AST_EXPR_UNARY:
            rename_expression(expr->expr.unary.operand, alias, decl_names, decl_count);
            break;
        case AST_EXPR_BINARY:
            rename_expression(expr->expr.binary.left, alias, decl_names, decl_count);
            rename_expression(expr->expr.binary.right, alias, decl_names, decl_count);
            break;
        case AST_EXPR_ARRAY_LITERAL: {
            AstArrayLiteralExpression *arr = &expr->expr.array_literal;
            for (size_t i = 0; i < arr->element_count; i++) {
                rename_expression(&arr->elements[i], alias, decl_names, decl_count);
            }
            break;
        }
    }
}

static void rename_statement(AstStatement *stmt, const char *alias, char **decl_names, size_t decl_count);

static void rename_block(AstBlock *block, const char *alias, char **decl_names, size_t decl_count) {
    if (block == NULL) return;
    for (size_t i = 0; i < block->len; i++) {
        rename_statement(&block->statements[i], alias, decl_names, decl_count);
    }
}

static void rename_statement(AstStatement *stmt, const char *alias, char **decl_names, size_t decl_count) {
    if (stmt == NULL) return;
    switch (stmt->kind) {
        case AST_STMT_VAR_DECL:
            rename_string_if_matches(&stmt->stmt.var_decl.name, alias, decl_names, decl_count);
            rename_string_if_matches(&stmt->stmt.var_decl.type_name, alias, decl_names, decl_count);
            if (stmt->stmt.var_decl.initializer) {
                rename_expression(stmt->stmt.var_decl.initializer, alias, decl_names, decl_count);
            }
            break;
        case AST_STMT_RETURN:
            if (stmt->stmt.return_stmt.value) {
                rename_expression(stmt->stmt.return_stmt.value, alias, decl_names, decl_count);
            }
            break;
        case AST_STMT_IF:
            if (stmt->stmt.if_stmt.condition) {
                rename_expression(stmt->stmt.if_stmt.condition, alias, decl_names, decl_count);
            }
            if (stmt->stmt.if_stmt.then_block) {
                rename_block(stmt->stmt.if_stmt.then_block, alias, decl_names, decl_count);
            }
            if (stmt->stmt.if_stmt.else_branch) {
                rename_statement(stmt->stmt.if_stmt.else_branch, alias, decl_names, decl_count);
            }
            break;
        case AST_STMT_WHILE:
            if (stmt->stmt.while_stmt.condition) {
                rename_expression(stmt->stmt.while_stmt.condition, alias, decl_names, decl_count);
            }
            if (stmt->stmt.while_stmt.body) {
                rename_block(stmt->stmt.while_stmt.body, alias, decl_names, decl_count);
            }
            break;
        case AST_STMT_FOR_RANGE:
            if (stmt->stmt.for_stmt.start) {
                rename_expression(stmt->stmt.for_stmt.start, alias, decl_names, decl_count);
            }
            if (stmt->stmt.for_stmt.end) {
                rename_expression(stmt->stmt.for_stmt.end, alias, decl_names, decl_count);
            }
            if (stmt->stmt.for_stmt.body) {
                rename_block(stmt->stmt.for_stmt.body, alias, decl_names, decl_count);
            }
            break;
        case AST_STMT_SWITCH:
            if (stmt->stmt.switch_stmt.subject) {
                rename_expression(stmt->stmt.switch_stmt.subject, alias, decl_names, decl_count);
            }
            for (size_t i = 0; i < stmt->stmt.switch_stmt.clause_count; i++) {
                AstSwitchClause *clause = &stmt->stmt.switch_stmt.clauses[i];
                if (clause->value) {
                    rename_expression(clause->value, alias, decl_names, decl_count);
                }
                for (size_t j = 0; j < clause->len; j++) {
                    rename_statement(&clause->statements[j], alias, decl_names, decl_count);
                }
            }
            break;
        case AST_STMT_EXPR:
            if (stmt->stmt.expr_stmt.expr) {
                rename_expression(stmt->stmt.expr_stmt.expr, alias, decl_names, decl_count);
            }
            break;
        case AST_STMT_BLOCK:
            rename_block(&stmt->stmt.block, alias, decl_names, decl_count);
            break;
        case AST_STMT_FN_DECL: {
            rename_string_if_matches(&stmt->stmt.fn_decl.name, alias, decl_names, decl_count);
            rename_string_if_matches(&stmt->stmt.fn_decl.return_type, alias, decl_names, decl_count);
            if (stmt->stmt.fn_decl.param_types) {
                for (size_t i = 0; i < stmt->stmt.fn_decl.param_count; i++) {
                    rename_string_if_matches(&stmt->stmt.fn_decl.param_types[i], alias, decl_names, decl_count);
                }
            }
            if (stmt->stmt.fn_decl.body) {
                rename_block(stmt->stmt.fn_decl.body, alias, decl_names, decl_count);
            }
            break;
        }
        case AST_STMT_TYPE_DECL: {
            rename_string_if_matches(&stmt->stmt.type_decl.name, alias, decl_names, decl_count);
            if (stmt->stmt.type_decl.field_types) {
                for (size_t i = 0; i < stmt->stmt.type_decl.field_count; i++) {
                    rename_string_if_matches(&stmt->stmt.type_decl.field_types[i], alias, decl_names, decl_count);
                }
            }
            break;
        }
        case AST_STMT_ANNOTATION:
            break;
        case AST_STMT_BREAK:
        case AST_STMT_CONTINUE:
            break;
    }
}

static int flatten_ast(AstProgram *program, AstProgram *flat, const char *input_dir, SourceList *source_list, IncludeStack *stack, Token *errtok) {
    for (size_t i = 0; i < program->len; i++) {
        AstStatement *stmt = &program->statements[i];
        if (stmt->kind == AST_STMT_ANNOTATION) {
            char *inc_src = NULL;
            const char *inc_path = stmt->stmt.annotation.value;
            
            // Circular include detection
            IncludeStack *curr = stack;
            while (curr != NULL) {
                if (strcmp(curr->path, inc_path) == 0) {
                    fprintf(stderr, "Error: Circular include detected: %s\n", inc_path);
                    return -1;
                }
                curr = curr->next;
            }

            if (inc_path && load_include(inc_path, input_dir, &inc_src) == 0) {
                source_list_add(source_list, inc_src);
                TokenStream inc_stream = {0};
                AstProgram inc_prog = {0};
                if (parse_program_source(inc_src, &inc_stream, &inc_prog, errtok) == 0) {
                    if (stmt->stmt.annotation.alias != NULL) {
                        const char *alias = stmt->stmt.annotation.alias;
                        // 1. Collect all top-level declaration names
                        size_t decl_count = 0;
                        size_t decl_cap = 16;
                        char **decl_names = malloc(decl_cap * sizeof(char *));
                        for (size_t j = 0; j < inc_prog.len; j++) {
                            AstStatement *s = &inc_prog.statements[j];
                            const char *name = NULL;
                            if (s->kind == AST_STMT_VAR_DECL) {
                                name = s->stmt.var_decl.name;
                            } else if (s->kind == AST_STMT_FN_DECL) {
                                name = s->stmt.fn_decl.name;
                            } else if (s->kind == AST_STMT_TYPE_DECL) {
                                name = s->stmt.type_decl.name;
                            }
                            if (name != NULL) {
                                if (decl_count >= decl_cap) {
                                    decl_cap *= 2;
                                    decl_names = realloc(decl_names, decl_cap * sizeof(char *));
                                }
                                decl_names[decl_count++] = strdup(name);
                            }
                        }

                        // 2. Recursively walk and rename all declarations and references
                        for (size_t j = 0; j < inc_prog.len; j++) {
                            rename_statement(&inc_prog.statements[j], alias, decl_names, decl_count);
                        }

                        // 3. Free collected names
                        for (size_t j = 0; j < decl_count; j++) {
                            free(decl_names[j]);
                        }
                        free(decl_names);
                    }

                    IncludeStack new_stack = { .path = inc_path, .next = stack };
                    if (flatten_ast(&inc_prog, flat, input_dir, source_list, &new_stack, errtok) != 0) {
                        return -1;
                    }
                } else {
                    print_parse_error(inc_path, errtok);
                    return -1;
                }
            } else {
                fprintf(stderr, "Error: Could not find include file '%s'\n", inc_path);
                return -1;
            }
        } else {
            static size_t flat_cap = 0;
            if (flat->len + 1 > flat_cap) {
                flat_cap = flat_cap < 128 ? 128 : flat_cap * 2;
                flat->statements = realloc(flat->statements, flat_cap * sizeof(AstStatement));
            }
            flat->statements[flat->len++] = *stmt;
        }
    }
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "Usage: %s <code-or-path> [--debug]\n", argv[0]); return EXIT_FAILURE; }

    int debug_mode = 0;
    const char *input_path = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--debug") == 0 || strcmp(argv[i], "-d") == 0) {
            debug_mode = 1;
        } else if (input_path == NULL) {
            input_path = argv[i];
        }
    }

    if (input_path == NULL) {
        fprintf(stderr, "Usage: %s <code-or-path> [--debug]\n", argv[0]);
        return EXIT_FAILURE;
    }

    SourceList source_list;
    source_list_init(&source_list);

    char *owned_source = NULL;
    const char *source = input_path;
    int read_rc = maybe_read_source_file(input_path, &owned_source);
    if (read_rc < 0) { fprintf(stderr, "Failed to read input file: %s\n", input_path); return EXIT_FAILURE; }
    if (read_rc > 0) {
        source = owned_source;
        source_list_add(&source_list, owned_source);
    }

    char *input_dir = NULL;
    if (read_rc > 0) input_dir = dup_dirname(input_path);

    TokenStream stream = {0};
    AstProgram program = {0};
    Token errtok = {0};

    if (parse_program_source(source, &stream, &program, &errtok) < 0) {
        print_parse_error("input", &errtok);
        token_free(&errtok);
        free(input_dir);
        source_list_free(&source_list);
        token_stream_free(&stream);
        return EXIT_FAILURE;
    }

    AstProgram *flat = calloc(1, sizeof(AstProgram));
    flat->statements = NULL;
    flat->len = 0;

    IncludeStack base_stack = { .path = input_path, .next = NULL };
    if (flatten_ast(&program, flat, input_dir, &source_list, &base_stack, &errtok) != 0) {
        free(flat->statements);
        free(flat);
        free(input_dir);
        source_list_free(&source_list);
        return EXIT_FAILURE;
    }

    Compiler compiler;
    compiler_init(&compiler, NULL);

    if (compiler_compile_ast(&compiler, flat)) {
        fprintf(stderr, "Compile error\n");
        compiler_free(&compiler);
        free(flat->statements);
        free(flat);
        free(input_dir);
        source_list_free(&source_list);
        return EXIT_FAILURE;
    }

    Chunk *chunk = compiler_get_chunk(&compiler);
    if (debug_mode) {
        disassemble_chunk(chunk, "Main Chunk");
    }
    
    VM vm;
    vm_init(&vm);
    vm_stdlib_init(&vm);
    vm.debug_trace = debug_mode;
    
    InterpretResult result = vm_interpret(&vm, chunk);
    
    if (result == INTERPRET_OK) {
        Value main_val;
        ObjString *main_name_obj = copy_string(&vm, "main", 4);
        if (table_get(&vm.globals.table, main_name_obj, &main_val)) {
            if (IS_FUNCTION(main_val) || IS_NATIVE(main_val)) {
                Chunk main_chunk;
                chunk_init(&main_chunk);
                
                // Use a raw string for the constant, vm_interpret will intern it.
                Value main_name_val = {VALUE_STRING, {.obj = (Obj*)"main"}};
                int name_idx = chunk_add_constant(&main_chunk, main_name_val);
                
                chunk_write_byte(&main_chunk, OP_GET_GLOBAL, 0);
                chunk_write_byte(&main_chunk, (uint8_t)name_idx, 0);
                chunk_write_byte(&main_chunk, OP_CALL, 0);
                chunk_write_byte(&main_chunk, 0, 0);
                chunk_write_byte(&main_chunk, OP_RETURN, 0);
                
                InterpretResult main_result = vm_interpret(&vm, &main_chunk);
                if (main_result != INTERPRET_OK) result = main_result;
                chunk_free(&main_chunk);
            }
        }
    }
    
    // Cleanup
    compiler_free(&compiler);
    free(flat->statements);
    free(flat);
    free(input_dir);
    source_list_free(&source_list);
    
    return result == INTERPRET_OK ? EXIT_SUCCESS : EXIT_FAILURE;
}