#ifndef VALUE_H
#define VALUE_H

#include <string.h>


#include "common.h"

typedef struct Object Object;
typedef struct ObjectString ObjectString;

#ifdef NAN_BOXING
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

static double valueToNum(Value value) {
	double num;
	memcpy(&num, &value, sizeof(Value));
	return num;
}

static Value numToValue(double num) {
	Value value;
	memcpy(&value, &num, sizeof(double));
	return value;
}

#else

typedef enum { VAL_BOOL, VAL_NIL, VAL_NUMBER, VAL_OBJECT } ValueType;

typedef struct {
	ValueType type;
	union {
		bool _bool;
		double _number;
		Object *_object;
	} as;
} Value;

// Check a given value's type
#define IS_BOOL(value) ((value).type == VAL_BOOL)
#define IS_NIL(value) ((value).type == VAL_NIL)
#define IS_NUMBER(value) ((value).type == VAL_NUMBER)
#define IS_OBJECT(value) ((value).type == VAL_OBJECT)

// Make a StellaC type a value
#define AS_BOOL(value) ((value).as._bool)
#define AS_NUMBER(value) ((value).as._number)
#define AS_OBJECT(value) ((value).as._object)

// Make a value a StellaC type
#define BOOL_VAL(value) ((Value){VAL_BOOL, {._bool = value}})
#define NIL_VAL ((Value){VAL_NIL, {._number = 0}})
#define NUMBER_VAL(value) ((Value){VAL_NUMBER, {._number = value}})
#define OBJECT_VAL(value) ((Value){VAL_OBJECT, {._object = value}})
#endif

typedef struct {
	int capacity;
	int count;
	Value *values;
} ValueArray;

bool valuesEqual(Value a, Value b);

void initValueArray(ValueArray *array);

void writeValueArray(ValueArray *array, Value value);

void freeValueArray(ValueArray *array);

void printValue(Value value);

#endif // VALUE_H
