#include <math.h>
#include <time.h>
#include <stdlib.h>
#include <ctype.h>

#include "vm/vm.h"
#include <stdio.h>
#include <string.h>

#if defined(__has_include)
#if __has_include(<raylib.h>)
#include <raylib.h>
#define AHHHHH_HAS_RAYLIB 1
#endif
#endif

#ifndef AHHHHH_HAS_RAYLIB
#define AHHHHH_HAS_RAYLIB 0
#endif

static double active_window_handle = 0.0;
static int window_open = 0;
static int frame_open = 0;

static const char *color_code_for_symbol(const char *symbol) {
    if (strcmp(symbol, ".red") == 0) return "\x1b[31m";
    if (strcmp(symbol, ".green") == 0) return "\x1b[32m";
    if (strcmp(symbol, ".yellow") == 0) return "\x1b[33m";
    if (strcmp(symbol, ".blue") == 0) return "\x1b[34m";
    if (strcmp(symbol, ".magenta") == 0) return "\x1b[35m";
    if (strcmp(symbol, ".cyan") == 0) return "\x1b[36m";
    if (strcmp(symbol, ".white") == 0) return "\x1b[37m";
    return NULL;
}

static void print_value_internal(Value value) {
    if (IS_NUMBER(value)) printf("%.15g", AS_NUMBER(value));
    else if (IS_STRING(value)) printf("%s", AS_CSTRING(value));
    else if (IS_BOOL(value)) printf("%s", AS_BOOL(value) ? "true" : "false");
    else if (IS_NULL(value)) printf("null");
    else if (IS_ARRAY(value)) {
        printf("[");
        ObjArray *array = (ObjArray *)AS_OBJ(value);
        for (size_t i = 0; i < array->count; i++) {
            print_value_internal(array->elements[i]);
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

Value vm_native_log(VM *vm, int arg_count, Value *args) {
    (void)vm;
    const char *color = NULL;
    int color_idx = -1;
    if (arg_count >= 2 && IS_STRING(args[arg_count - 1])) {
        color = color_code_for_symbol(AS_CSTRING(args[arg_count - 1]));
        if (color != NULL) color_idx = arg_count - 1;
    }
    if (color_idx == -1 && arg_count >= 2 && IS_STRING(args[1])) {
        color = color_code_for_symbol(AS_CSTRING(args[1]));
        if (color != NULL) color_idx = 1;
    }
    
    if (color) printf("%s", color);
    int first = 1;
    for (int i = 0; i < arg_count; i++) {
        if (i == color_idx) continue;
        if (!first) printf(" ");
        first = 0;
        print_value_internal(args[i]);
    }
    if (color) printf("\x1b[0m");
    printf("\n");
    return (Value){VALUE_NULL, {0}};
}

Value vm_native_print(VM *vm, int arg_count, Value *args) {
    (void)vm;
    const char *color = NULL;
    int color_idx = -1;
    if (arg_count >= 2 && IS_STRING(args[arg_count - 1])) {
        color = color_code_for_symbol(AS_CSTRING(args[arg_count - 1]));
        if (color != NULL) color_idx = arg_count - 1;
    }
    if (color_idx == -1 && arg_count >= 2 && IS_STRING(args[1])) {
        color = color_code_for_symbol(AS_CSTRING(args[1]));
        if (color != NULL) color_idx = 1;
    }
    
    if (color) printf("%s", color);
    for (int i = 0; i < arg_count; i++) {
        if (i == color_idx) continue;
        print_value_internal(args[i]);
    }
    if (color) printf("\x1b[0m");
    printf("\n");
    return (Value){VALUE_NULL, {0}};
}

static Value vm_native_add(VM *vm, int arg_count, Value *args) {
    (void)vm;
    if (arg_count < 2) return (Value){VALUE_NULL, {0}};
    if (!IS_NUMBER(args[0]) || !IS_NUMBER(args[1])) return (Value){VALUE_NULL, {0}};
    return (Value){VALUE_NUMBER, {.number = AS_NUMBER(args[0]) + AS_NUMBER(args[1])}};
}

static Value vm_native_sub(VM *vm, int arg_count, Value *args) {
    (void)vm;
    if (arg_count < 2) return (Value){VALUE_NULL, {0}};
    if (!IS_NUMBER(args[0]) || !IS_NUMBER(args[1])) return (Value){VALUE_NULL, {0}};
    return (Value){VALUE_NUMBER, {.number = AS_NUMBER(args[0]) - AS_NUMBER(args[1])}};
}

static Value vm_native_mul(VM *vm, int arg_count, Value *args) {
    (void)vm;
    if (arg_count < 2) return (Value){VALUE_NULL, {0}};
    if (!IS_NUMBER(args[0]) || !IS_NUMBER(args[1])) return (Value){VALUE_NULL, {0}};
    return (Value){VALUE_NUMBER, {.number = AS_NUMBER(args[0]) * AS_NUMBER(args[1])}};
}

static Value vm_native_div(VM *vm, int arg_count, Value *args) {
    (void)vm;
    if (arg_count < 2) return (Value){VALUE_NULL, {0}};
    if (!IS_NUMBER(args[0]) || !IS_NUMBER(args[1])) return (Value){VALUE_NULL, {0}};
    if (AS_NUMBER(args[1]) == 0) return (Value){VALUE_NULL, {0}};
    return (Value){VALUE_NUMBER, {.number = AS_NUMBER(args[0]) / AS_NUMBER(args[1])}};
}

static Value vm_native_window_create(VM *vm, int arg_count, Value *args) {
    (void)vm;
    if (arg_count < 3) return (Value){VALUE_NULL, {0}};
    if (!IS_NUMBER(args[0]) || !IS_NUMBER(args[1]) || !IS_STRING(args[2])) {
        return (Value){VALUE_NULL, {0}};
    }
    
    int width = (int)AS_NUMBER(args[0]);
    int height = (int)AS_NUMBER(args[1]);
    const char *title = AS_CSTRING(args[2]);
    
    if (width <= 0 || height <= 0) return (Value){VALUE_NULL, {0}};
    if (window_open) return (Value){VALUE_NULL, {0}};
#if AHHHHH_HAS_RAYLIB
    InitWindow(width, height, title);
    if (!IsWindowReady()) return (Value){VALUE_NULL, {0}};
    SetTargetFPS(60);
#else
    (void)title;
    return (Value){VALUE_NULL, {0}};
#endif

    window_open = 1;
    active_window_handle = 1.0;
    return (Value){VALUE_NUMBER, {.number = active_window_handle}};
}

static Value vm_native_window_should_close(VM *vm, int arg_count, Value *args) {
    (void)vm;
    if (arg_count < 1 || !window_open) {
        return (Value){VALUE_BOOL, {.boolean = 1}};
    }
#if AHHHHH_HAS_RAYLIB
    int should_close = WindowShouldClose() ? 1 : 0;
#else
    int should_close = 1;
#endif
    return (Value){VALUE_BOOL, {.boolean = should_close}};
}

static Value vm_native_begin_draw(VM *vm, int arg_count, Value *args) {
    (void)vm;
    if (arg_count < 1 || !window_open || frame_open) {
        return (Value){VALUE_NULL, {0}};
    }
#if AHHHHH_HAS_RAYLIB
    BeginDrawing();
    frame_open = 1;
#endif
    return (Value){VALUE_NULL, {0}};
}

static Value vm_native_end_draw(VM *vm, int arg_count, Value *args) {
    (void)vm;
    if (arg_count < 1 || !window_open || !frame_open) {
        return (Value){VALUE_NULL, {0}};
    }
#if AHHHHH_HAS_RAYLIB
    EndDrawing();
#endif
    frame_open = 0;
    return (Value){VALUE_NULL, {0}};
}

static Value vm_native_clear_background(VM *vm, int arg_count, Value *args) {
    (void)vm;
    if (arg_count < 5 || !window_open || !frame_open) {
        return (Value){VALUE_NULL, {0}};
    }
    
    int r = (int)AS_NUMBER(args[1]);
    int g = (int)AS_NUMBER(args[2]);
    int b = (int)AS_NUMBER(args[3]);
    int a = (int)AS_NUMBER(args[4]);
    
    if (r < 0 || r > 255 || g < 0 || g > 255 || b < 0 || b > 255 || a < 0 || a > 255) {
        return (Value){VALUE_NULL, {0}};
    }
    
#if AHHHHH_HAS_RAYLIB
    ClearBackground((Color){r, g, b, a});
#endif
    return (Value){VALUE_NULL, {0}};
}

static Value vm_native_draw_text(VM *vm, int arg_count, Value *args) {
    (void)vm;
    if (arg_count < 9 || !window_open || !frame_open) {
        return (Value){VALUE_NULL, {0}};
    }
    
    if (!IS_STRING(args[1])) return (Value){VALUE_NULL, {0}};
    const char *text = AS_CSTRING(args[1]);
    int x = (int)AS_NUMBER(args[2]);
    int y = (int)AS_NUMBER(args[3]);
    int size = (int)AS_NUMBER(args[4]);
    int r = (int)AS_NUMBER(args[5]);
    int g = (int)AS_NUMBER(args[6]);
    int b = (int)AS_NUMBER(args[7]);
    int a = (int)AS_NUMBER(args[8]);
    
    if (r < 0 || r > 255 || g < 0 || g > 255 || b < 0 || b > 255 || a < 0 || a > 255) {
        return (Value){VALUE_NULL, {0}};
    }
    
#if AHHHHH_HAS_RAYLIB
    DrawText(text, x, y, size, (Color){r, g, b, a});
#endif
    return (Value){VALUE_NULL, {0}};
}

static Value vm_native_window_close(VM *vm, int arg_count, Value *args) {
    (void)vm;
    if (arg_count < 1 || !window_open) {
        return (Value){VALUE_NULL, {0}};
    }
    if (frame_open) {
#if AHHHHH_HAS_RAYLIB
        EndDrawing();
#endif
        frame_open = 0;
    }
#if AHHHHH_HAS_RAYLIB
    CloseWindow();
#endif
    window_open = 0;
    return (Value){VALUE_NULL, {0}};
}

Value vm_native_input(VM *vm, int arg_count, Value *args) {
    if (arg_count > 0 && IS_STRING(args[0])) {
        printf("%s", AS_CSTRING(args[0]));
        fflush(stdout);
    }
    
    char buffer[1024];
    if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
        ObjString *string = copy_string(vm, "", 0);
        return (Value){VALUE_STRING, {.obj = (Obj*)string}};
    }
    
    size_t len = strlen(buffer);
    if (len > 0 && buffer[len - 1] == '\n') {
        buffer[len - 1] = '\0';
        len--;
    }
    
    ObjString *string = copy_string(vm, buffer, (int)len);
    return (Value){VALUE_STRING, {.obj = (Obj*)string}};
}

static Value vm_native_array_push(VM *vm, int arg_count, Value *args) {
    (void)vm;
    if (arg_count < 2 || !IS_ARRAY(args[0])) return (Value){VALUE_NULL, {0}};
    ObjArray *array = (ObjArray *)AS_OBJ(args[0]);
    if (array->count == array->capacity) {
        size_t new_cap = array->capacity < 8 ? 8 : array->capacity * 2;
        Value *new_elements = realloc(array->elements, sizeof(Value) * new_cap);
        if (new_elements == NULL) return (Value){VALUE_NULL, {0}};
        array->elements = new_elements;
        array->capacity = new_cap;
    }
    array->elements[array->count++] = args[1];
    return args[1];
}

static Value vm_native_array_pop(VM *vm, int arg_count, Value *args) {
    (void)vm;
    if (arg_count < 1 || !IS_ARRAY(args[0])) return (Value){VALUE_NULL, {0}};
    ObjArray *array = (ObjArray *)AS_OBJ(args[0]);
    if (array->count == 0) return (Value){VALUE_NULL, {0}};
    return array->elements[--array->count];
}

static Value vm_native_math_abs(VM *vm, int arg_count, Value *args) {
    (void)vm;
    if (arg_count < 1 || !IS_NUMBER(args[0])) return (Value){VALUE_NULL, {0}};
    return (Value){VALUE_NUMBER, {.number = fabs(AS_NUMBER(args[0]))}};
}

static Value vm_native_math_floor(VM *vm, int arg_count, Value *args) {
    (void)vm;
    if (arg_count < 1 || !IS_NUMBER(args[0])) return (Value){VALUE_NULL, {0}};
    return (Value){VALUE_NUMBER, {.number = floor(AS_NUMBER(args[0]))}};
}

static Value vm_native_math_ceil(VM *vm, int arg_count, Value *args) {
    (void)vm;
    if (arg_count < 1 || !IS_NUMBER(args[0])) return (Value){VALUE_NULL, {0}};
    return (Value){VALUE_NUMBER, {.number = ceil(AS_NUMBER(args[0]))}};
}

static Value vm_native_math_sqrt(VM *vm, int arg_count, Value *args) {
    (void)vm;
    if (arg_count < 1 || !IS_NUMBER(args[0])) return (Value){VALUE_NULL, {0}};
    return (Value){VALUE_NUMBER, {.number = sqrt(AS_NUMBER(args[0]))}};
}

static Value vm_native_math_sin(VM *vm, int arg_count, Value *args) {
    (void)vm;
    if (arg_count < 1 || !IS_NUMBER(args[0])) return (Value){VALUE_NULL, {0}};
    return (Value){VALUE_NUMBER, {.number = sin(AS_NUMBER(args[0]))}};
}

static Value vm_native_math_cos(VM *vm, int arg_count, Value *args) {
    (void)vm;
    if (arg_count < 1 || !IS_NUMBER(args[0])) return (Value){VALUE_NULL, {0}};
    return (Value){VALUE_NUMBER, {.number = cos(AS_NUMBER(args[0]))}};
}

static Value vm_native_math_tan(VM *vm, int arg_count, Value *args) {
    (void)vm;
    if (arg_count < 1 || !IS_NUMBER(args[0])) return (Value){VALUE_NULL, {0}};
    return (Value){VALUE_NUMBER, {.number = tan(AS_NUMBER(args[0]))}};
}

static Value vm_native_math_atan2(VM *vm, int arg_count, Value *args) {
    (void)vm;
    if (arg_count < 2 || !IS_NUMBER(args[0]) || !IS_NUMBER(args[1])) return (Value){VALUE_NULL, {0}};
    return (Value){VALUE_NUMBER, {.number = atan2(AS_NUMBER(args[0]), AS_NUMBER(args[1]))}};
}

static Value vm_native_math_exp(VM *vm, int arg_count, Value *args) {
    (void)vm;
    if (arg_count < 1 || !IS_NUMBER(args[0])) return (Value){VALUE_NULL, {0}};
    return (Value){VALUE_NUMBER, {.number = exp(AS_NUMBER(args[0]))}};
}

static Value vm_native_math_log10(VM *vm, int arg_count, Value *args) {
    (void)vm;
    if (arg_count < 1 || !IS_NUMBER(args[0])) return (Value){VALUE_NULL, {0}};
    return (Value){VALUE_NUMBER, {.number = log10(AS_NUMBER(args[0]))}};
}

static Value vm_native_math_ln(VM *vm, int arg_count, Value *args) {
    (void)vm;
    if (arg_count < 1 || !IS_NUMBER(args[0])) return (Value){VALUE_NULL, {0}};
    return (Value){VALUE_NUMBER, {.number = log(AS_NUMBER(args[0]))}};
}

static Value vm_native_math_pow(VM *vm, int arg_count, Value *args) {
    (void)vm;
    if (arg_count < 2 || !IS_NUMBER(args[0]) || !IS_NUMBER(args[1])) return (Value){VALUE_NULL, {0}};
    return (Value){VALUE_NUMBER, {.number = pow(AS_NUMBER(args[0]), AS_NUMBER(args[1]))}};
}

static Value vm_native_io_clock(VM *vm, int arg_count, Value *args) {
    (void)vm; (void)arg_count; (void)args;
    return (Value){VALUE_NUMBER, {.number = (double)clock() / CLOCKS_PER_SEC}};
}

static Value vm_native_io_exit(VM *vm, int arg_count, Value *args) {
    (void)vm;
    int code = 0;
    if (arg_count >= 1 && IS_NUMBER(args[0])) code = (int)AS_NUMBER(args[0]);
    exit(code);
    return (Value){VALUE_NULL, {0}};
}


static Value vm_native_is_key_down(VM *vm, int arg_count, Value *args) {
    (void)vm;
    if (arg_count < 1 || !IS_NUMBER(args[0])) return (Value){VALUE_BOOL, {.boolean = 0}};
    int key = (int)AS_NUMBER(args[0]);
#if AHHHHH_HAS_RAYLIB
    return (Value){VALUE_BOOL, {.boolean = IsKeyDown(key)}};
#else
    (void)key;
    return (Value){VALUE_BOOL, {.boolean = 0}};
#endif
}

static int values_equal_local(Value a, Value b) {
    if (a.kind != b.kind) return 0;
    switch (a.kind) { 
        case VALUE_NUMBER: return AS_NUMBER(a) == AS_NUMBER(b); 
        case VALUE_BOOL:   return AS_BOOL(a) == AS_BOOL(b); 
        case VALUE_NULL:   return 1; 
        case VALUE_STRING: return strcmp(AS_CSTRING(a), AS_CSTRING(b)) == 0;
        case VALUE_OBJECT:
        case VALUE_FUNCTION:
        case VALUE_NATIVE:
        case VALUE_ARRAY:  return AS_OBJ(a) == AS_OBJ(b);
        default: break; 
    }
    return 0;
}

static Value vm_native_rand(VM *vm, int arg_count, Value *args) {
    (void)vm; (void)arg_count; (void)args;
    static int seeded = 0;
    if (!seeded) {
        srand((unsigned int)time(NULL));
        seeded = 1;
    }
    double r = (double)rand() / ((double)RAND_MAX + 1.0);
    return (Value){VALUE_NUMBER, {.number = r}};
}

static Value vm_native_rand_range(VM *vm, int arg_count, Value *args) {
    (void)vm;
    if (arg_count < 2 || !IS_NUMBER(args[0]) || !IS_NUMBER(args[1])) {
        return (Value){VALUE_NUMBER, {.number = 0.0}};
    }
    static int seeded = 0;
    if (!seeded) {
        srand((unsigned int)time(NULL));
        seeded = 1;
    }
    double min = AS_NUMBER(args[0]);
    double max = AS_NUMBER(args[1]);
    if (min >= max) {
        return (Value){VALUE_NUMBER, {.number = min}};
    }
    double r = (double)rand() / ((double)RAND_MAX + 1.0);
    double val = min + r * (max - min);
    return (Value){VALUE_NUMBER, {.number = val}};
}

static Value vm_native_strlen(VM *vm, int arg_count, Value *args) {
    (void)vm;
    if (arg_count < 1 || !IS_STRING(args[0])) return (Value){VALUE_NUMBER, {.number = 0.0}};
    return (Value){VALUE_NUMBER, {.number = (double)strlen(AS_CSTRING(args[0]))}};
}

static Value vm_native_substr(VM *vm, int arg_count, Value *args) {
    if (arg_count < 3 || !IS_STRING(args[0]) || !IS_NUMBER(args[1]) || !IS_NUMBER(args[2])) {
        return (Value){VALUE_NULL, {0}};
    }
    const char *str = AS_CSTRING(args[0]);
    int str_len = (int)strlen(str);
    int start = (int)AS_NUMBER(args[1]);
    int len = (int)AS_NUMBER(args[2]);

    if (start < 0) start = 0;
    if (start > str_len) start = str_len;
    if (len < 0) len = 0;
    if (start + len > str_len) len = str_len - start;

    ObjString *sub = copy_string(vm, str + start, len);
    return (Value){VALUE_STRING, {.obj = (Obj*)sub}};
}

static int format_value_to_buffer(Value value, char *buf, size_t max_len) {
    if (IS_NUMBER(value)) {
        return snprintf(buf, max_len, "%.15g", AS_NUMBER(value));
    } else if (IS_STRING(value)) {
        return snprintf(buf, max_len, "%s", AS_CSTRING(value));
    } else if (IS_BOOL(value)) {
        return snprintf(buf, max_len, "%s", AS_BOOL(value) ? "true" : "false");
    } else if (IS_NULL(value)) {
        return snprintf(buf, max_len, "null");
    } else if (IS_ARRAY(value)) {
        ObjArray *array = (ObjArray *)AS_OBJ(value);
        size_t written = 0;
        int res = snprintf(buf + written, max_len - written, "[");
        if (res < 0) return -1;
        written += (size_t)res;
        for (size_t i = 0; i < array->count; i++) {
            if (written >= max_len) break;
            res = format_value_to_buffer(array->elements[i], buf + written, max_len - written);
            if (res < 0) return -1;
            written += (size_t)res;
            if (i < array->count - 1 && written < max_len) {
                res = snprintf(buf + written, max_len - written, ", ");
                if (res < 0) return -1;
                written += (size_t)res;
            }
        }
        if (written < max_len) {
            res = snprintf(buf + written, max_len - written, "]");
            if (res < 0) return -1;
            written += (size_t)res;
        }
        return (int)written;
    } else if (IS_OBJECT(value)) {
        return snprintf(buf, max_len, "<object>");
    } else if (IS_FUNCTION(value)) {
        return snprintf(buf, max_len, "<fn>");
    } else if (IS_NATIVE(value)) {
        return snprintf(buf, max_len, "<native fn>");
    } else {
        return snprintf(buf, max_len, "<unknown>");
    }
}

static Value vm_native_to_string(VM *vm, int arg_count, Value *args) {
    if (arg_count < 1) return (Value){VALUE_NULL, {0}};
    char buf[4096];
    format_value_to_buffer(args[0], buf, sizeof(buf));
    ObjString *str = copy_string(vm, buf, (int)strlen(buf));
    return (Value){VALUE_STRING, {.obj = (Obj*)str}};
}

static Value vm_native_to_number(VM *vm, int arg_count, Value *args) {
    (void)vm;
    if (arg_count < 1 || !IS_STRING(args[0])) return (Value){VALUE_NULL, {0}};
    const char *str = AS_CSTRING(args[0]);
    while (*str && isspace((unsigned char)*str)) str++;
    if (*str == '\0') return (Value){VALUE_NULL, {0}};
    
    char *end = NULL;
    double val = strtod(str, &end);
    
    while (*end && isspace((unsigned char)*end)) end++;
    if (*end != '\0') return (Value){VALUE_NULL, {0}};
    return (Value){VALUE_NUMBER, {.number = val}};
}

static Value vm_native_file_read(VM *vm, int arg_count, Value *args) {
    if (arg_count < 2 || !IS_STRING(args[1])) return (Value){VALUE_NULL, {0}};
    const char *path = AS_CSTRING(args[1]);
    FILE *f = fopen(path, "rb");
    if (!f) return (Value){VALUE_NULL, {0}};
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char *buf = malloc(size + 1);
    if (!buf) {
        fclose(f);
        return (Value){VALUE_NULL, {0}};
    }
    
    size_t read_bytes = fread(buf, 1, size, f);
    buf[read_bytes] = '\0';
    fclose(f);
    
    ObjString *str = copy_string(vm, buf, (int)read_bytes);
    free(buf);
    return (Value){VALUE_STRING, {.obj = (Obj*)str}};
}

static Value vm_native_file_write(VM *vm, int arg_count, Value *args) {
    (void)vm;
    if (arg_count < 3 || !IS_STRING(args[1]) || !IS_STRING(args[2])) return (Value){VALUE_BOOL, {.boolean = 0}};
    const char *path = AS_CSTRING(args[1]);
    const char *content = AS_CSTRING(args[2]);
    
    FILE *f = fopen(path, "wb");
    if (!f) return (Value){VALUE_BOOL, {.boolean = 0}};
    
    size_t len = strlen(content);
    size_t written = fwrite(content, 1, len, f);
    fclose(f);
    
    return (Value){VALUE_BOOL, {.boolean = (written == len) ? 1 : 0}};
}

static Value vm_native_file_exists(VM *vm, int arg_count, Value *args) {
    (void)vm;
    if (arg_count < 2 || !IS_STRING(args[1])) return (Value){VALUE_BOOL, {.boolean = 0}};
    const char *path = AS_CSTRING(args[1]);
    FILE *f = fopen(path, "r");
    if (f) {
        fclose(f);
        return (Value){VALUE_BOOL, {.boolean = 1}};
    }
    return (Value){VALUE_BOOL, {.boolean = 0}};
}

static Value vm_native_draw_rectangle(VM *vm, int arg_count, Value *args) {
    (void)vm;
    if (arg_count < 9 || !window_open || !frame_open) {
        return (Value){VALUE_NULL, {0}};
    }
    int x = (int)AS_NUMBER(args[1]);
    int y = (int)AS_NUMBER(args[2]);
    int width = (int)AS_NUMBER(args[3]);
    int height = (int)AS_NUMBER(args[4]);
    int r = (int)AS_NUMBER(args[5]);
    int g = (int)AS_NUMBER(args[6]);
    int b = (int)AS_NUMBER(args[7]);
    int a = (int)AS_NUMBER(args[8]);
    
    if (r < 0 || r > 255 || g < 0 || g > 255 || b < 0 || b > 255 || a < 0 || a > 255) {
        return (Value){VALUE_NULL, {0}};
    }
    
#if AHHHHH_HAS_RAYLIB
    DrawRectangle(x, y, width, height, (Color){r, g, b, a});
#endif
    return (Value){VALUE_NULL, {0}};
}

static Value vm_native_draw_circle(VM *vm, int arg_count, Value *args) {
    (void)vm;
    if (arg_count < 8 || !window_open || !frame_open) {
        return (Value){VALUE_NULL, {0}};
    }
    int cx = (int)AS_NUMBER(args[1]);
    int cy = (int)AS_NUMBER(args[2]);
    double radius = AS_NUMBER(args[3]);
    int r = (int)AS_NUMBER(args[4]);
    int g = (int)AS_NUMBER(args[5]);
    int b = (int)AS_NUMBER(args[6]);
    int a = (int)AS_NUMBER(args[7]);
    
    if (r < 0 || r > 255 || g < 0 || g > 255 || b < 0 || b > 255 || a < 0 || a > 255) {
        return (Value){VALUE_NULL, {0}};
    }
    
#if AHHHHH_HAS_RAYLIB
    DrawCircle(cx, cy, (float)radius, (Color){r, g, b, a});
#endif
    return (Value){VALUE_NULL, {0}};
}

static Value vm_native_draw_line(VM *vm, int arg_count, Value *args) {
    (void)vm;
    if (arg_count < 9 || !window_open || !frame_open) {
        return (Value){VALUE_NULL, {0}};
    }
    int start_x = (int)AS_NUMBER(args[1]);
    int start_y = (int)AS_NUMBER(args[2]);
    int end_x = (int)AS_NUMBER(args[3]);
    int end_y = (int)AS_NUMBER(args[4]);
    int r = (int)AS_NUMBER(args[5]);
    int g = (int)AS_NUMBER(args[6]);
    int b = (int)AS_NUMBER(args[7]);
    int a = (int)AS_NUMBER(args[8]);
    
    if (r < 0 || r > 255 || g < 0 || g > 255 || b < 0 || b > 255 || a < 0 || a > 255) {
        return (Value){VALUE_NULL, {0}};
    }
    
#if AHHHHH_HAS_RAYLIB
    DrawLine(start_x, start_y, end_x, end_y, (Color){r, g, b, a});
#endif
    return (Value){VALUE_NULL, {0}};
}

static Value vm_native_is_key_pressed(VM *vm, int arg_count, Value *args) {
    (void)vm;
    if (arg_count < 2 || !IS_NUMBER(args[1])) return (Value){VALUE_BOOL, {.boolean = 0}};
    int key = (int)AS_NUMBER(args[1]);
#if AHHHHH_HAS_RAYLIB
    return (Value){VALUE_BOOL, {.boolean = IsKeyPressed(key) ? 1 : 0}};
#else
    (void)key;
    return (Value){VALUE_BOOL, {.boolean = 0}};
#endif
}

static Value vm_native_get_mouse_position(VM *vm, int arg_count, Value *args) {
    (void)arg_count; (void)args;
    ObjArray *arr = new_array(vm, 2);
    
    double x = 0.0;
    double y = 0.0;
#if AHHHHH_HAS_RAYLIB
    Vector2 pos = GetMousePosition();
    x = pos.x;
    y = pos.y;
#endif
    arr->elements[0] = (Value){VALUE_NUMBER, {.number = x}};
    arr->elements[1] = (Value){VALUE_NUMBER, {.number = y}};
    
    return (Value){VALUE_ARRAY, {.obj = (Obj*)arr}};
}

static Value vm_native_char_at(VM *vm, int arg_count, Value *args) {
    if (arg_count < 2 || !IS_STRING(args[0]) || !IS_NUMBER(args[1])) {
        return (Value){VALUE_NULL, {0}};
    }
    const char *str = AS_CSTRING(args[0]);
    int str_len = (int)strlen(str);
    int idx = (int)AS_NUMBER(args[1]);
    if (idx < 0 || idx >= str_len) {
        return (Value){VALUE_STRING, {.obj = (Obj*)copy_string(vm, "", 0)}};
    }
    char c = str[idx];
    return (Value){VALUE_STRING, {.obj = (Obj*)copy_string(vm, &c, 1)}};
}

static Value vm_native_array_len(VM *vm, int arg_count, Value *args) {
    (void)vm;
    if (arg_count < 1 || !IS_ARRAY(args[0])) return (Value){VALUE_NUMBER, {.number = 0.0}};
    ObjArray *array = (ObjArray *)AS_OBJ(args[0]);
    return (Value){VALUE_NUMBER, {.number = (double)array->count}};
}

static Value vm_native_array_remove(VM *vm, int arg_count, Value *args) {
    (void)vm;
    if (arg_count < 2 || !IS_ARRAY(args[0]) || !IS_NUMBER(args[1])) return (Value){VALUE_NULL, {0}};
    ObjArray *array = (ObjArray *)AS_OBJ(args[0]);
    int idx = (int)AS_NUMBER(args[1]);
    if (idx < 0 || idx >= (int)array->count) return (Value){VALUE_NULL, {0}};

    Value removed = array->elements[idx];
    for (size_t i = (size_t)idx; i < array->count - 1; i++) {
        array->elements[i] = array->elements[i + 1];
    }
    array->count--;
    return removed;
}

static Value vm_native_array_insert(VM *vm, int arg_count, Value *args) {
    (void)vm;
    if (arg_count < 3 || !IS_ARRAY(args[0]) || !IS_NUMBER(args[1])) return (Value){VALUE_NULL, {0}};
    ObjArray *array = (ObjArray *)AS_OBJ(args[0]);
    int idx = (int)AS_NUMBER(args[1]);
    Value val = args[2];

    if (idx < 0) idx = 0;
    if (idx > (int)array->count) idx = (int)array->count;

    if (array->count == array->capacity) {
        size_t new_cap = array->capacity < 8 ? 8 : array->capacity * 2;
        Value *new_elements = realloc(array->elements, sizeof(Value) * new_cap);
        if (new_elements == NULL) return (Value){VALUE_NULL, {0}};
        array->elements = new_elements;
        array->capacity = new_cap;
    }

    for (size_t i = array->count; i > (size_t)idx; i--) {
        array->elements[i] = array->elements[i - 1];
    }
    array->elements[idx] = val;
    array->count++;
    return (Value){VALUE_NULL, {0}};
}

static Value vm_native_array_contains(VM *vm, int arg_count, Value *args) {
    (void)vm;
    if (arg_count < 2 || !IS_ARRAY(args[0])) return (Value){VALUE_BOOL, {.boolean = 0}};
    ObjArray *array = (ObjArray *)AS_OBJ(args[0]);
    Value val = args[1];
    for (size_t i = 0; i < array->count; i++) {
        if (values_equal_local(array->elements[i], val)) {
            return (Value){VALUE_BOOL, {.boolean = 1}};
        }
    }
    return (Value){VALUE_BOOL, {.boolean = 0}};
}

static Value vm_native_array_reverse(VM *vm, int arg_count, Value *args) {
    (void)vm;
    if (arg_count < 1 || !IS_ARRAY(args[0])) return (Value){VALUE_NULL, {0}};
    ObjArray *array = (ObjArray *)AS_OBJ(args[0]);
    if (array->count > 1) {
        size_t left = 0;
        size_t right = array->count - 1;
        while (left < right) {
            Value temp = array->elements[left];
            array->elements[left] = array->elements[right];
            array->elements[right] = temp;
            left++;
            right--;
        }
    }
    return (Value){VALUE_NULL, {0}};
}

int vm_stdlib_init(VM *vm) {
    vm_define_native(vm, "abs", vm_native_math_abs);
    vm_define_native(vm, "floor", vm_native_math_floor);
    vm_define_native(vm, "ceil", vm_native_math_ceil);
    vm_define_native(vm, "sqrt", vm_native_math_sqrt);
    vm_define_native(vm, "sin", vm_native_math_sin);
    vm_define_native(vm, "cos", vm_native_math_cos);
    vm_define_native(vm, "tan", vm_native_math_tan);
    vm_define_native(vm, "atan2", vm_native_math_atan2);
    vm_define_native(vm, "exp", vm_native_math_exp);
    vm_define_native(vm, "log10", vm_native_math_log10);
    vm_define_native(vm, "ln", vm_native_math_ln);
    vm_define_native(vm, "pow", vm_native_math_pow);
    vm_define_native(vm, "clock", vm_native_io_clock);
    vm_define_native(vm, "exit", vm_native_io_exit);
    vm_define_native(vm, "log", vm_native_log);
    vm_define_native(vm, "print", vm_native_print);
    vm_define_native(vm, "input", vm_native_input);
    vm_define_native(vm, "add", vm_native_add);
    vm_define_native(vm, "sub", vm_native_sub);
    vm_define_native(vm, "mul", vm_native_mul);
    vm_define_native(vm, "div", vm_native_div);
    vm_define_native(vm, "array_push", vm_native_array_push);
    vm_define_native(vm, "array_pop", vm_native_array_pop);
    vm_define_native(vm, "is_key_down", vm_native_is_key_down);
    vm_define_native(vm, "window_create", vm_native_window_create);
    vm_define_native(vm, "window_should_close", vm_native_window_should_close);
    vm_define_native(vm, "begin_draw", vm_native_begin_draw);
    vm_define_native(vm, "end_draw", vm_native_end_draw);
    vm_define_native(vm, "clear_background", vm_native_clear_background);
    vm_define_native(vm, "draw_text", vm_native_draw_text);
    vm_define_native(vm, "window_close", vm_native_window_close);
    
    // Neue native Funktionen
    vm_define_native(vm, "rand", vm_native_rand);
    vm_define_native(vm, "rand_range", vm_native_rand_range);
    vm_define_native(vm, "strlen", vm_native_strlen);
    vm_define_native(vm, "substr", vm_native_substr);
    vm_define_native(vm, "to_string", vm_native_to_string);
    vm_define_native(vm, "to_number", vm_native_to_number);
    vm_define_native(vm, "char_at", vm_native_char_at);
    vm_define_native(vm, "array_len", vm_native_array_len);
    vm_define_native(vm, "array_remove", vm_native_array_remove);
    vm_define_native(vm, "array_insert", vm_native_array_insert);
    vm_define_native(vm, "array_contains", vm_native_array_contains);
    vm_define_native(vm, "array_reverse", vm_native_array_reverse);
    
    // Datei-I/O
    vm_define_native(vm, "file_read", vm_native_file_read);
    vm_define_native(vm, "file_write", vm_native_file_write);
    vm_define_native(vm, "file_exists", vm_native_file_exists);
    
    // Erweitertes Raylib
    vm_define_native(vm, "draw_rectangle", vm_native_draw_rectangle);
    vm_define_native(vm, "draw_circle", vm_native_draw_circle);
    vm_define_native(vm, "draw_line", vm_native_draw_line);
    vm_define_native(vm, "is_key_pressed", vm_native_is_key_pressed);
    vm_define_native(vm, "get_mouse_position", vm_native_get_mouse_position);
    
    return 0;
}
