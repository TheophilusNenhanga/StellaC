#include "collections.h"

#include "../memory.h"
#include "../object.h"

ObjectResult* lengthNative(VM *vm, int argCount, Value *args) {
	Value value = args[0];
	if (IS_STL_ARRAY(value)) {
		return stellaOk(vm, NUMBER_VAL(AS_STL_ARRAY(value)->size));
	}
	if (IS_STL_STRING(value)) {
		return stellaOk(vm, NUMBER_VAL(AS_STL_STRING(value)->length));
	}
	if (IS_STL_TABLE(value)) {
		return stellaOk(vm, NUMBER_VAL(AS_STL_TABLE(value)->size));
	}
	return stellaErr(vm, newError(vm, copyString(vm, "Expected either a collection type or a string.", 46), TYPE, false));
}
