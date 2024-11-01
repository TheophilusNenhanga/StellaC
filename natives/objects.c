#include "objects.h"

#include <stdlib.h>

Value hashValue(int argCount, Value *args) {
	Value value = args[0];
	if (IS_STRING(value)) {
		return NUMBER_VAL(AS_STRING(value)->object.hash);
	}
	if (IS_ARRAY(value)) {
#ifdef HASH_ARRAYS
		return NUMBER_VAL(AS_ARRAY(value)->object.hash);
#else
		return NIL_VAL;
#endif
	}
	if (IS_NIL(value)) {
		return NUMBER_VAL(4321);
	}
	if (IS_BOOL(value)) {
		return NUMBER_VAL(AS_BOOL(value) ? 1 : 0);
	}
	if (IS_NUMBER(value)) {
		return value;
	}
	if (IS_CLASS(value)) {
		return NUMBER_VAL(AS_CLASS(value)->object.hash);
	}
	if (IS_FUNCTION(value)) {
		return NUMBER_VAL(AS_FUNCTION(value)->object.hash);
	}
	return NIL_VAL;
}

Value valueType(int argCount, Value *args) {
	Value value = args[0];
	if (IS_NUMBER(value)) {
		return OBJECT_VAL(copyString("<type number>", sizeof("<type number>") - 1));
	}
	if (IS_STRING(value)) {
		return OBJECT_VAL(copyString("<type string>", sizeof("<type string>") - 1));
	}
	if (IS_ARRAY(value)) {
		return OBJECT_VAL(copyString("<type array>", sizeof("<type array>") - 1));
	}
	if (IS_BOOL(value)) {
		return OBJECT_VAL(copyString("<type bool>", sizeof("<type bool>") - 1));
	}
	if (IS_FUNCTION(value)) {
		return OBJECT_VAL(copyString("<type function>", sizeof("<type function>") - 1));
	}
	if (IS_CLASS(value)) {
		return OBJECT_VAL(copyString("<type class>", sizeof("<type class>") - 1));
	}
	if (IS_TABLE(value)) {
		return OBJECT_VAL(copyString("<type table>", sizeof("<type table>") - 1));
	}
	if (IS_NATIVE(value)) {
		return OBJECT_VAL(copyString("<native function>", sizeof("<native function>") - 1));
	}
	if (IS_INSTANCE(value)) {
		ObjectClass *klass = AS_INSTANCE(value)->klass;

		char* str1 = "<";
		char* str2 = klass->name->chars;
		char* str3 = " instance>";

		size_t length = strlen(str1) + strlen(str2) + strlen(str3) + 1;
		char* combined = (char*)malloc(length);
		if (combined == NULL) {
			free(combined);
			return NIL_VAL;
		}
		strcpy(combined, str1);
		strcpy(combined, str2);
		strcpy(combined, str3);

		Value result = OBJECT_VAL(copyString(combined, length));
		free(combined);
		return result;
	}
	if (IS_NIL(value)) {
		return OBJECT_VAL(copyString("<type nil>", sizeof("<type nil>") - 1));
	}
	if (IS_CLOSURE(value)) {
		return OBJECT_VAL(copyString("<type function>", sizeof("<type function>") - 1));
	}
	// TODO: RUNTIME ERROR
}
