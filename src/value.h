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
#define TAG_INT32_BIT ((uint64_t) 1<<48)
typedef uint64_t Value;

#define IS_INT(value) (((value) & (QNAN | SIGN_BIT | TAG_INT32_BIT)) == (QNAN | TAG_INT32_BIT))
#define IS_FLOAT(value) (((value) & QNAN) != QNAN)
#define IS_NIL(value) ((value) == NIL_VAL)
#define IS_BOOL(value) (((value) | 1) == TRUE_VAL)
#define IS_CRUX_OBJECT(value) (((value) & (QNAN | SIGN_BIT)) == (QNAN | SIGN_BIT))

// #define IS_NUMBER(value) (IS_INT(value) || IS_FLOAT(value))

#define AS_INT(value) ((int32_t)((value) & 0xFFFFFFFF))
#define AS_FLOAT(value) valueToNum(value)
#define AS_BOOL(value) ((value) == TRUE_VAL)
#define AS_CRUX_OBJECT(value) ((Object *) (uintptr_t) ((value) & ~(SIGN_BIT | QNAN)))

#define OBJECT_VAL(obj) (Value)(SIGN_BIT | QNAN | (uint64_t) (uintptr_t) (obj))
#define BOOL_VAL(b) ((b) ? TRUE_VAL : FALSE_VAL)
#define FALSE_VAL ((Value) (uint64_t) (QNAN | TAG_FALSE))
#define TRUE_VAL ((Value) (uint64_t) (QNAN | TAG_TRUE))
#define NIL_VAL ((Value) (uint64_t) (QNAN | TAG_NIL))
#define FLOAT_VAL(num) numToValue(num)
#define INT_VAL(integer) ((Value)(QNAN | TAG_INT32_BIT | ((uint64_t)(integer) & 0xFFFFFFFF)))

static inline double valueToNum(Value value) {
	union {
		Value v;
		double d;
	} u;
	u.v = value;
	return u.d;
}

static inline Value numToValue(double num) {
	union {
		double d;
		Value v;
	} u;
	u.d = num;
	return u.v;
}

typedef struct {
	int capacity;
	int count;
	Value *values;
} ValueArray;

/**
 * @brief Compares two values for equality
 *
 * For number values, compares their numeric values.
 * For other types, performs direct comparison.
 *
 * @param a First value to compare
 * @param b Second value to compare
 * @return true if the values are equal, false otherwise
 */
bool valuesEqual(Value a, Value b);

/**
 * @brief Initializes a new value array
 *
 * Sets up an empty ValueArray with null values pointer and
 * zero capacity and count.
 *
 * @param array Pointer to the ValueArray to initialize
 */
void initValueArray(ValueArray *array);

/**
 * @brief Adds a value to a value array, growing the array if needed
 *
 * Appends the given value to the end of the array. If the array is at capacity,
 * it will be resized to accommodate the new value.
 *
 * @param vm Pointer to the virtual machine (used for memory management)
 * @param array Pointer to the ValueArray to modify
 * @param value The Value to append to the array
 */
void writeValueArray(VM *vm, ValueArray *array, Value value);

/**
 * @brief Frees memory allocated for a value array
 *
 * Deallocates the memory used by the array's values and resets the array
 * to an initialized state.
 *
 * @param vm Pointer to the virtual machine (used for memory management)
 * @param array Pointer to the ValueArray to free
 */
void freeValueArray(VM *vm, ValueArray *array);

/**
 * @brief Prints a human-readable representation of a value
 *
 * Outputs the value to stdout in a format appropriate for its type:
 * - Booleans print as "true" or "false"
 * - Nil prints as "nil"
 * - Numbers print in their natural format
 * - Objects are printed using the printObject function
 *
 * @param value The Value to print
 */
void printValue(Value value);

#endif // VALUE_H
