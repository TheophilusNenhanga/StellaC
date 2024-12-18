#ifndef OBJECT_H
#define OBJECT_H

#include "chunk.h"
#include "common.h"
#include "table.h"
#include "value.h"
#include "vm.h"


#define OBJECT_TYPE(value) (AS_OBJECT(value)->type)

#define IS_STRING(value) isObjectType(value, OBJECT_STRING)
#define IS_FUNCTION(value) isObjectType(value, OBJECT_FUNCTION)
#define IS_NATIVE(value) isObjectType(value, OBJECT_NATIVE)
#define IS_CLOSURE(value) isObjectType(value, OBJECT_CLOSURE)
#define IS_CLASS(value) isObjectType(value, OBJECT_CLASS)
#define IS_INSTANCE(value) isObjectType(value, OBJECT_INSTANCE)
#define IS_BOUND_METHOD(value) isObjectType(value, OBJECT_BOUND_METHOD)
#define IS_ARRAY(value) isObjectType(value, OBJECT_ARRAY)
#define IS_TABLE(value) isObjectType(value, OBJECT_TABLE)
#define IS_ERROR(value) isObjectType(value, OBJECT_ERROR)

#define AS_STRING(value) ((ObjectString *) AS_OBJECT(value))
#define AS_CSTRING(value) (((ObjectString *) AS_OBJECT(value))->chars)
#define AS_FUNCTION(value) ((ObjectFunction *) AS_OBJECT(value))
#define AS_NATIVE(value) (((ObjectNative *) AS_OBJECT(value))->function)
#define AS_NATIVE_FN(value) ((ObjectNative *) AS_OBJECT(value))
#define AS_CLOSURE(value) ((ObjectClosure *) AS_OBJECT(value))
#define AS_CLASS(value) ((ObjectClass *) AS_OBJECT(value))
#define AS_INSTANCE(value) ((ObjectInstance *) AS_OBJECT(value))
#define AS_BOUND_METHOD(value) ((ObjectBoundMethod *) AS_OBJECT(value))
#define AS_ARRAY(value) ((ObjectArray *) AS_OBJECT(value))
#define AS_TABLE(value) ((ObjectTable *) AS_OBJECT(value))
#define AS_ERROR(value) ((ObjectError *) AS_OBJECT(value))


typedef enum {
	OBJECT_STRING,
	OBJECT_FUNCTION,
	OBJECT_NATIVE,
	OBJECT_CLOSURE,
	OBJECT_UPVALUE,
	OBJECT_CLASS,
	OBJECT_INSTANCE,
	OBJECT_BOUND_METHOD,
	OBJECT_ARRAY,
	OBJECT_TABLE,
	OBJECT_ERROR,
} ObjectType;

struct Object {
	ObjectType type;
	Object *next;
	bool isMarked;
};

struct ObjectString {
	Object Object;
	int length;
	char *chars;
	uint32_t hash;
};

typedef struct {
	Object object;
	int arity;
	int upvalueCount;
	Chunk chunk;
	ObjectString *name;
} ObjectFunction;

typedef struct ObjectUpvalue {
	Object object;
	Value *location;
	Value closed;
	struct ObjectUpvalue *next;
} ObjectUpvalue;

typedef struct ObjectClosure {
	Object object;
	ObjectFunction *function;
	ObjectUpvalue **upvalues;
	int upvalueCount;
} ObjectClosure;

typedef struct {
	Object object;
	ObjectString *name;
	Table methods;
} ObjectClass;

typedef struct {
	Object object;
	ObjectClass *klass;
	Table fields;
} ObjectInstance;

typedef struct {
	Object object;
	Value receiver;
	ObjectClosure *method;
} ObjectBoundMethod;

typedef struct {
	Object object;
	Value *array;
	int size;
	int capacity;
} ObjectArray;

typedef struct {
	Value* values;
	uint8_t size;
} NativeReturn;

typedef NativeReturn (*NativeFn)(int argCount, Value *args);

typedef struct {
	Object object;
	NativeFn function;
	int arity;
} ObjectNative;

typedef struct {
	Value key;
	Value value;
	bool isOccupied;
} ObjectTableEntry;

typedef struct {
	Object object;
	ObjectTableEntry *entries;
	uint16_t capacity;
	uint16_t size;
} ObjectTable;

typedef enum {
	SYNTAX,
	DIVISION_BY_ZERO,
	INDEX_OUT_OF_BOUNDS,
	RUNTIME,
	TYPE,
	LOOP_EXTENT,
	LIMIT,
	BRANCH_EXTENT,
	CLOSURE_EXTENT,
	LOCAL_EXTENT,
	ARGUMENT_EXTENT,
	NAME,
	COLLECTION_EXTENT,
	VARIABLE_EXTENT,
	VARIABLE_DECLARATION_MISMATCH,
	RETURN_EXTENT,
	ARGUMENT_MISMATCH,
	STACK_OVERFLOW,
	COLLECTION_GET,
	COLLECTION_SET,
	UNPACK_MISMATCH,
	MEMORY,
} ErrorType;

typedef enum { USER, STELLA, PANIC } ErrorCreator;

typedef struct {
	Object object;
	ObjectString *message;
	ErrorType type;
	ErrorCreator creator;
} ObjectError;

static bool isObjectType(Value value, ObjectType type) { return IS_OBJECT(value) && AS_OBJECT(value)->type == type; }

ObjectError *newError(ObjectString *message, ErrorType type, ErrorCreator creator);
ObjectBoundMethod *newBoundMethod(Value receiver, ObjectClosure *method);
ObjectUpvalue *newUpvalue(Value *slot);
ObjectClosure *newClosure(ObjectFunction *function);
ObjectNative *newNative(NativeFn function, int arity);
ObjectFunction *newFunction();
ObjectClass *newClass(ObjectString *name);
ObjectInstance *newInstance(ObjectClass *klass);
ObjectString *takeString(char *chars, int length);
ObjectString *copyString(const char *chars, int length);
void printObject(Value value);
NativeReturn makeNativeReturn(uint8_t size);

ObjectString *toString(Value value);

ObjectTable *newTable(int elementCount);
void freeObjectTable(ObjectTable *table);
bool objectTableSet(ObjectTable *table, Value key, Value value);
bool objectTableGet(ObjectTable *table, Value key, Value *value);
void markObjectTable(ObjectTable *table);

ObjectArray *newArray(int elementCount);
bool ensureCapacity(ObjectArray *array, int capacityNeeded);
bool arraySet(ObjectArray *array, int index, Value value);
bool arrayGet(ObjectArray *array, int index, Value *value);
bool arrayAdd(ObjectArray *array, Value value, int index);


#endif

// For heap allocated types
