#include "collections.h"

#include "../memory.h"
#include "../object.h"


Value length(int argCount, Value *args) {
	Value value = args[0];
	if (IS_ARRAY(value)) {
		return NUMBER_VAL(AS_ARRAY(value)->size);
	}
	if (IS_STRING(value)) {
		return NUMBER_VAL(AS_STRING(value)->length);
	}
	// TODO: Runtime error
	return NIL_VAL;
}

Value arrayAdd(int argCount, Value *args) {
	Value arr = args[0];
	Value toAdd = args[1];

	if (!IS_ARRAY(arr)) {
		// TODO: RUNTIME_ERROR
		return NIL_VAL;
	}

	ObjectArray *array = AS_ARRAY(arr);

	if (array->size >= array->capacity) {
		int newCapacity = array->capacity < 8 ? 8 : array->capacity * 2;

		if (newCapacity > MAX_ARRAY_SIZE) {
			newCapacity = MAX_ARRAY_SIZE;
			if (array->size >= MAX_ARRAY_SIZE) {
				// TODO: RUNTIME ERROR
				return NIL_VAL;
			}
		}

		array->array = GROW_ARRAY(Value, array->array, array->capacity, newCapacity);
		array->capacity = newCapacity;
	}

#ifdef HASH_ARRAYS
	uint32_t newHash = hashBytes(&toAdd, sizeof(toAdd));
	array->object.hash ^= newHash;
	array->object.hash *= FNV_PRIME;
#endif

	array->array[array->size] = toAdd;
	array->size++;
	return NIL_VAL;
}

Value arrayRemove(int argCount, Value *args) {
	Value value = args[0];
	if (IS_ARRAY(value)) {
		ObjectArray *array = AS_ARRAY(value);
		if (array->size < 1) {
			// TODO: Runtime Error
			return NIL_VAL;
		} else {
#ifdef HASH_ARRAYS
			uint32_t oldHash = hashBytes(&array->array[array->size-1], sizeof(value));
			array->object.hash /= FNV_PRIME;
			array->object.hash ^= oldHash;
#endif
			array->array[array->size - 1] = NIL_VAL;
			array->size--;
			return NIL_VAL;
		}
	} else {
		// TODO: RUNTIME_ERROR
		return NIL_VAL;
	}
}
