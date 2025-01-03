#ifndef VALUE_H
#define VALUE_H

#include <string.h>
#include "common.h"

typedef struct VM VM;
typedef struct Object Object;
typedef struct ObjectString ObjectString;

#define QNAN ((uint64_t) 0x7ffc000000000000)
#define SIGN_BIT ((uint64_t) 0x8000000000000000)
#define TAG_NIL 1 // 01.
#define TAG_FALSE 2 // 10.
#define TAG_TRUE 3 // 11.
typedef uint64_t Value;

#define IS_NUMBER(value) (((value) & QNAN) != QNAN)
#define IS_NIL(value) ((value) == NIL_VAL)
#define IS_BOOL(value) (((value) | 1) == TRUE_VAL)
#define IS_OBJECT(value) (((value) & (QNAN | SIGN_BIT)) == (QNAN | SIGN_BIT))

#define AS_NUMBER(value) valueToNum(value)
#define AS_BOOL(value) ((value) == TRUE_VAL)
#define AS_OBJECT(value) ((Object *) (uintptr_t) ((value) & ~(SIGN_BIT | QNAN)))

#define OBJECT_VAL(obj) (Value)(SIGN_BIT | QNAN | (uint64_t) (uintptr_t) (obj))
#define BOOL_VAL(b) ((b) ? TRUE_VAL : FALSE_VAL)
#define FALSE_VAL ((Value) (uint64_t) (QNAN | TAG_FALSE))
#define TRUE_VAL ((Value) (uint64_t) (QNAN | TAG_TRUE))
#define NIL_VAL ((Value) (uint64_t) (QNAN | TAG_NIL))
#define NUMBER_VAL(num) numToValue(num)

static inline double valueToNum(Value value) {
	double num;
	memcpy(&num, &value, sizeof(Value));
	return num;
}

static inline Value numToValue(double num) {
	Value value;
	memcpy(&value, &num, sizeof(double));
	return value;
}

typedef struct {
	int capacity;
	int count;
	Value *values;
} ValueArray;

bool valuesEqual(Value a, Value b);

void initValueArray(ValueArray *array);

void writeValueArray(VM *vm, ValueArray *array, Value value);

void freeValueArray(VM *vm, ValueArray *array);

void printValue(Value value);

#endif // VALUE_H
