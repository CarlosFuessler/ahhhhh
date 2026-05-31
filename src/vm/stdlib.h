#ifndef AHHHHH_STDLIB_H
#define AHHHHH_STDLIB_H

#include "vm/vm.h"

int vm_stdlib_init(VM *vm);

// native funktionen zur konsolen ein-ausgabe
Value vm_native_print(VM *vm, int arg_count, Value *args);
Value vm_native_log(VM *vm, int arg_count, Value *args);
Value vm_native_input(VM *vm, int arg_count, Value *args);

#endif
