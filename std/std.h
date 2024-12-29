#ifndef TYPES_H
#define TYPES_H

#include "../object.h"
#include "../table.h"

typedef struct {
	const char *name;
	NativeFn function;
	int arity;
} Callable;

extern Callable stringMethods[];
extern Callable arrayMethods[];
extern Callable tableMethods[];
extern Callable errorMethods[];

bool defineNativeCallable(VM *vm, Table *methodTable, const char *methodName, NativeFn methodFunction, int arity);

bool defineMethods(VM *vm, Table *methodTable, Callable *methods);

bool defineNativeFunctions(VM *vm, Table *callableTable);

#endif // TYPES_H
