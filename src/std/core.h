#ifndef COLLECTIONS_H
#define COLLECTIONS_H

#include "../object.h"
#include "../value.h"

#define MAX_ARRAY_SIZE UINT16_MAX - 1

ObjectResult* lengthFunction(VM *vm, int argCount, Value *args);
ObjectResult* intFunction(VM *vm, int argCount, Value *args);
ObjectResult* floatFunction(VM *vm, int argCount, Value *args);
ObjectResult* stringFunction(VM *vm, int argCount, Value *args);
ObjectResult* arrayFunction(VM *vm, int argCount, Value *args);
ObjectResult* tableFunction(VM *vm, int argCount, Value *args);

Value lengthFunction_(VM *vm, int argCount, Value *args);
Value typeFunction_(VM *vm, int argCount, Value *args);
Value intFunction_(VM *vm, int argCount, Value *args);
Value floatFunction_(VM *vm, int argCount, Value *args);
Value stringFunction_(VM *vm, int argCount, Value *args);
Value arrayFunction_(VM *vm, int argCount, Value *args);
Value tableFunction_(VM *vm, int argCount, Value *args);

#endif
