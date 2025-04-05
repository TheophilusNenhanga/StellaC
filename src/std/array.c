#include <stdlib.h>

#include "../memory.h"
#include "array.h"


ObjectResult* arrayPushMethod(VM *vm, int argCount, Value *args) {
	ObjectArray *array = AS_CRUX_ARRAY(args[0]);
	Value toAdd = args[1];

	if (!arrayAdd(vm, array, toAdd, array->size)) {
		return stellaErr(vm, newError(vm, copyString(vm, "Failed to add to array.", 23), RUNTIME, false));
	}

	return stellaOk(vm, NIL_VAL);
}

ObjectResult* arrayPopMethod(VM *vm, int argCount, Value *args) {
	ObjectArray *array = AS_CRUX_ARRAY(args[0]);

	if (array->size == 0) {
		return stellaErr(vm, newError(vm, copyString(vm, "Cannot remove a value from an empty array.", 42), INDEX_OUT_OF_BOUNDS, false));
	}

	Value popped = array->array[array->size - 1];
	array->array[array->size - 1] = NIL_VAL;
	array->size--;

	return stellaOk(vm, popped);
}

ObjectResult* arrayInsertMethod(VM *vm, int argCount, Value *args) {
	ObjectArray *array = AS_CRUX_ARRAY(args[0]);

	if (!IS_INT(args[2])) {
		return stellaErr(vm, newError(vm, copyString(vm, "<index> must be of type 'number'.", 33), TYPE, false));
	}

	Value toInsert = args[1];
	uint32_t insertAt = AS_INT(args[2]);

	if (insertAt > array->size) {
		return stellaErr(vm, newError(vm, copyString(vm, "<index> is out of bounds.", 25), INDEX_OUT_OF_BOUNDS, false));
	}

	if (ensureCapacity(vm, array, array->size + 1)) {
		for (uint32_t i = array->size; i > insertAt; i--) {
			array->array[i] = array->array[i - 1];
		}

		array->array[insertAt] = toInsert;
		array->size++;
	} else {
		return stellaErr(vm, newError(vm, copyString(vm, "Failed to allocate enough memory for new array.", 47), MEMORY, false));
	}
	return stellaOk(vm, NIL_VAL);
}

ObjectResult* arrayRemoveAtMethod(VM *vm, int argCount, Value *args) {
	ObjectArray *array = AS_CRUX_ARRAY(args[0]);

	if (!IS_INT(args[1])) {
		return stellaErr(vm, newError(vm, copyString(vm, "<index> must be of type 'number'.", 33), TYPE, false));
	}

	uint32_t removeAt = AS_INT(args[1]);

	if (removeAt >= array->size) {
		return stellaErr(vm, newError(vm, copyString(vm, "<index> is out of bounds.", 25), INDEX_OUT_OF_BOUNDS, false));
	}

	Value removedElement = array->array[removeAt];

	for (uint32_t i = removeAt; i < array->size - 1; i++) {
		array->array[i] = array->array[i + 1];
	}

	array->size--;
	return stellaOk(vm, removedElement);
}

ObjectResult* arrayConcatMethod(VM *vm, int argCount, Value *args) {
	ObjectArray *array = AS_CRUX_ARRAY(args[0]);

	if (!IS_CRUX_ARRAY(args[1])) {
		return stellaErr(vm, newError(vm, copyString(vm, "<target> must be of type 'array'.", 33), TYPE, false));
	}

	ObjectArray *targetArray = AS_CRUX_ARRAY(args[1]);

	uint32_t combinedSize = targetArray->size + array->size;
	if (combinedSize > MAX_ARRAY_SIZE) {
		return stellaErr(vm, newError(vm, copyString(vm, "Size of resultant array out of bounds.", 38), INDEX_OUT_OF_BOUNDS, false));
	}

	ObjectArray *resultArray = newArray(vm, combinedSize);

	for (uint32_t i = 0; i < combinedSize; i++) {
		resultArray->array[i] = (i < array->size) ? array->array[i] : targetArray->array[i - array->size];
	}

	resultArray->size = combinedSize;

	return stellaOk(vm,  OBJECT_VAL(resultArray));
}

ObjectResult* arraySliceMethod(VM *vm, int argCount, Value *args) {
	ObjectArray *array = AS_CRUX_ARRAY(args[0]);

	if (!IS_INT(args[1])) {
		return stellaErr(vm, newError(vm, copyString(vm, "<start_index> must be of type 'number'.", 39), TYPE, false));
	}

	if (!IS_INT(args[2])) {
		return stellaErr(vm,  newError(vm, copyString(vm, "<end_index> must be of type 'number'.", 37), TYPE, false));
	}

	uint32_t startIndex = IS_INT(args[1]);
	uint32_t endIndex = IS_INT(args[2]);

	if (startIndex > array->size) {
		return stellaErr(vm, newError(vm, copyString(vm, "<start_index> out of bounds.", 28), INDEX_OUT_OF_BOUNDS, false));
	}

	if (endIndex > array->size) {
		return stellaErr(vm,newError(vm, copyString(vm, "<end_index> out of bounds.", 26), INDEX_OUT_OF_BOUNDS, false));
	}

	if (endIndex < startIndex) {
		return stellaErr(vm, newError(vm, copyString(vm, "indexes out of bounds.", 22), INDEX_OUT_OF_BOUNDS, false));
	}

	size_t sliceSize = endIndex - startIndex;
	ObjectArray *slicedArray = newArray(vm, sliceSize);

	for (size_t i = 0; i < sliceSize; i++) {
		slicedArray->array[i] = array->array[startIndex + i];
		slicedArray->size += 1;
	}

	return stellaOk(vm, OBJECT_VAL(slicedArray));
}

ObjectResult* arrayReverseMethod(VM *vm, int argCount, Value *args) {
	ObjectArray *array = AS_CRUX_ARRAY(args[0]);

	Value *values = ALLOCATE(vm, Value, array->size);

	if (values == NULL) {
		return stellaErr(vm, newError(vm, copyString(vm, "Failed to allocate memory when reversing array.", 47), MEMORY, false));
	}

	for (uint32_t i = 0; i < array->size; i++) {
		values[i] = array->array[i];
	}

	for (uint32_t i = 0; i < array->size; i++) {
		array->array[i] = values[array->size - 1 - i];
	}

	free(values);

	return stellaOk(vm, NIL_VAL);
}

ObjectResult* arrayIndexOfMethod(VM *vm, int argCount, Value *args) {
	ObjectArray *array = AS_CRUX_ARRAY(args[0]);
	Value target = args[1];

	for (uint32_t i = 0; i < array->size; i++) {
		if (valuesEqual(target, array->array[i])) {
			return stellaOk(vm, INT_VAL(i));
		}
	}
	return stellaErr(vm, newError(vm, copyString(vm, "Value could not be found in the array.", 38), VALUE, false));
}

Value arrayContainsMethod(VM *vm, int argCount, Value *args) {
	ObjectArray *array = AS_CRUX_ARRAY(args[0]);
	Value target = args[1];

	for (uint32_t i = 0; i < array->size; i++) {
		if (valuesEqual(target, array->array[i])) {
			return BOOL_VAL(true);
		}
	}
	return BOOL_VAL(false);
}

Value arrayClearMethod(VM *vm, int argCount, Value *args) {
	ObjectArray *array = AS_CRUX_ARRAY(args[0]);

	for (uint32_t i = 0; i < array->size; i++) {
		array->array[i] = NIL_VAL;
	}
	array->size = 0;

	return NIL_VAL;
}

Value arrayEqualsMethod(VM *vm, int argCount, Value *args) {
	if (!IS_CRUX_ARRAY(args[1])) {
		return BOOL_VAL(false);
	}

	ObjectArray *array = AS_CRUX_ARRAY(args[0]);
	ObjectArray *targetArray = AS_CRUX_ARRAY(args[1]);
	
	if (array->size != targetArray->size) {
		return BOOL_VAL(false);
	}

	for (uint32_t i = 0; i < array->size; i++) {
		if (!valuesEqual(array->array[i], targetArray->array[i])) {
			return BOOL_VAL(false);
		}
	}

	return BOOL_VAL(true);
}

