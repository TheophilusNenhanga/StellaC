#ifndef MATH_H
#define MATH_H

#include "../value.h"
#include "std.h"


ObjectResult* powFunction(VM *vm, int argCount, Value *args);
ObjectResult* sqrtFunction(VM *vm, int argCount, Value *args);
ObjectResult* absFunction(VM *vm, int argCount, Value *args);

ObjectResult* sinFunction(VM *vm, int argCount, Value *args);
ObjectResult* cosFunction(VM *vm, int argCount, Value *args);
ObjectResult* tanFunction(VM *vm, int argCount, Value *args);

ObjectResult* asinFunction(VM *vm, int argCount, Value *args);
ObjectResult* acosFunction(VM *vm, int argCount, Value *args);
ObjectResult* atanFunction(VM *vm, int argCount, Value *args);

ObjectResult* expFunction(VM *vm, int argCount, Value *args);
ObjectResult* lnFunction(VM *vm, int argCount, Value *args);
ObjectResult* log10Function(VM *vm, int argCount, Value *args);

ObjectResult* ceilFunction(VM *vm, int argCount, Value *args);
ObjectResult* floorFunction(VM *vm, int argCount, Value *args);
ObjectResult* roundFunction(VM *vm, int argCount, Value *args);

Value piFunction(VM *vm, int argCount, Value *args);
Value eFunction(VM *vm, int argCount, Value *args);
#endif //MATH_H
