#ifndef VM_H
#define VM_H

#include "table.h"
#include "value.h"

typedef struct ObjectClosure ObjectClosure;
typedef struct ObjectUpvalue ObjectUpvalue;
typedef struct ObjectModule ObjectModule;

#define MAX_VM_DEPTH 64
#define FRAMES_MAX 64
#define STACK_MAX (FRAMES_MAX * UINT8_COUNT)

typedef enum { INTERPRET_OK, INTERPRET_COMPILE_ERROR, INTERPRET_RUNTIME_ERROR } InterpretResult;

/**
 * An ongoing function call
 */
typedef struct {
	ObjectClosure *closure;
	uint8_t *ip;
	Value *slots;
} CallFrame;

typedef struct {
	Table methods;
} NativeType;

typedef struct {
	char *name;
	Table *names;
} NativeModule;

typedef struct {
	Value matchTarget;
	Value matchBind;
	bool isMatchTarget;
	bool isMatchBind;
} MatchHandler;

typedef struct {
	NativeModule *modules;
	int count;
	int capacity;
}NativeModules;

struct VM {
	Value stack[STACK_MAX]; // always points just past the last item
	Value *stackTop;
	Object *objects;
	Table strings;
	Table globals;
	CallFrame frames[FRAMES_MAX];
	int frameCount;
	int grayCount;
	ObjectUpvalue *openUpvalues;
	size_t bytesAllocated;
	size_t nextGC;
	Object **grayStack;
	ObjectString *initString;
	NativeType randomType;
	NativeType stringType;
	NativeType arrayType;
	NativeType tableType;
	NativeType errorType;
	NativeType fileType;
	struct VM *enclosing;
	int grayCapacity;
	ObjectModule *module;
	NativeModules nativeModules;
	MatchHandler matchHandler;
};

VM *newVM();

void initVM(VM *vm);

void freeVM(VM *vm);

void resetStack(VM *vm);

InterpretResult interpret(VM *vm, char *source);

void push(VM *vm, Value value);

Value pop(VM *vm);

#endif // VM_H
