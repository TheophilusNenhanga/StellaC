#include "core.h"

#include <stdio.h>
#include <stdlib.h>

#include "../memory.h"
#include "../object.h"

static Value getLength(Value value) {
	if (IS_STL_ARRAY(value)) {
		return NUMBER_VAL(AS_STL_ARRAY(value)->size);
	}
	if (IS_STL_STRING(value)) {
		return NUMBER_VAL(AS_STL_STRING(value)->length);
	}
	if (IS_STL_TABLE(value)) {
		return NUMBER_VAL(AS_STL_TABLE(value)->size);
	}
	return NIL_VAL;
}

ObjectResult* lengthNative(VM *vm, int argCount, Value *args) {
	Value value = args[0];
	Value length = getLength(value);
	if (IS_NIL(length)) {
		return stellaErr(vm, newError(vm, copyString(vm, "Expected either a collection type ('string', 'array', 'table').", 46), TYPE, false));
	}
	return stellaOk(vm, length);
}

Value lengthNative_(VM *vm, int argCount, Value *args) {
	Value value = args[0];
	return getLength(value);
}

/**
 * Infallible function that returns a string representing the value's type
 * 
 * @param vm The virtual machine
 * @param argCount Number of arguments
 * @param args Array of arguments
 * @return A string value representing the type
 */
Value typeNative(VM *vm, int argCount, Value *args) {
    Value value = args[0];
    
    if (IS_NUMBER(value)) {
        return OBJECT_VAL(copyString(vm, "number", 6));
    }
    
    if (IS_BOOL(value)) {
        return OBJECT_VAL(copyString(vm, "boolean", 7));
    }
    
    if (IS_NIL(value)) {
        return OBJECT_VAL(copyString(vm, "nil", 3));
    }
    
    if (IS_STL_STRING(value)) {
        return OBJECT_VAL(copyString(vm, "string", 6));
    }
    
    if (IS_STL_ARRAY(value)) {
        return OBJECT_VAL(copyString(vm, "array", 5));
    }
    
    if (IS_STL_TABLE(value)) {
        return OBJECT_VAL(copyString(vm, "table", 5));
    }
    
    if (IS_STL_FUNCTION(value) || IS_STL_CLOSURE(value) || 
        IS_STL_NATIVE_FUNCTION(value) || IS_STL_NATIVE_INFALLIBLE_FUNCTION(value)) {
        return OBJECT_VAL(copyString(vm, "function", 8));
    }
    
    if (IS_STL_NATIVE_METHOD(value) || IS_STL_NATIVE_INFALLIBLE_METHOD(value) || 
        IS_STL_BOUND_METHOD(value)) {
        return OBJECT_VAL(copyString(vm, "method", 6));
    }
    
    if (IS_STL_CLASS(value)) {
        return OBJECT_VAL(copyString(vm, "class", 5));
    }
    
    if (IS_STL_INSTANCE(value)) {
        return OBJECT_VAL(copyString(vm, "instance", 8));
    }
    
    if (IS_STL_ERROR(value)) {
        return OBJECT_VAL(copyString(vm, "error", 5));
    }
    
    if (IS_STL_RESULT(value)) {
        return OBJECT_VAL(copyString(vm, "result", 6));
    }
    
    return OBJECT_VAL(copyString(vm, "unknown", 7));
}

static Value castArray(VM *vm, Value *args, bool* success) {
    Value value = args[0];

    if (IS_STL_ARRAY(value)) {
        return value;
    }

    if (IS_STL_STRING(value)) {
        ObjectString* string = AS_STL_STRING(value);
        ObjectArray* array =  newArray(vm, string->length);
        for (int i = 0; i < string->length; i++) {
            if (!arrayAddBack(vm, array, OBJECT_VAL(copyString(vm, &string->chars[i], 1)))) {
                *success = false;
                return NIL_VAL;
            }
        }
        return OBJECT_VAL(array);
    }

    if (IS_STL_TABLE(value)) {
        ObjectTable* table = AS_STL_TABLE(value);
        ObjectArray* array = newArray(vm, table->size*2);
        int index = 0;
        for (int i = 0; i < table->capacity; i++) {
            if (index == table->size) {
                break;
            }
            if (table->entries[i].isOccupied) {
                if (!arrayAddBack(vm, array, table->entries[i].key) ||
                    !arrayAddBack(vm, array, table->entries[i].value)) {
                    *success = false;
                    return NIL_VAL;
                }
                index++;
            }
        }
        return OBJECT_VAL(array);
    }
    ObjectArray* array = newArray(vm, 1);
    arrayAdd(vm, array, value, 0);
    return  OBJECT_VAL(array);
}

static Value castTable(VM *vm, Value *args) {
    Value value = args[0];

    if (IS_STL_TABLE(value)) {
        return  value;
    }

    if (IS_STL_ARRAY(value)) {
        ObjectArray* array = AS_STL_ARRAY(value);
        ObjectTable* table = newTable(vm, (int) array->size);
        for (int i = 0; i < array->size; i++) {
            Value k = NUMBER_VAL(i);
            Value v = array->array[i];
            objectTableSet(vm, table, k, v);
        }
        return OBJECT_VAL(table);
    }

    if (IS_STL_STRING(value)) {
        ObjectString* string = AS_STL_STRING(value);
        ObjectTable* table = newTable(vm, string->length);
        for (int i = 0; i < string->length; i++) {
            objectTableSet(vm, table, NUMBER_VAL(i), OBJECT_VAL(copyString(vm, &string->chars[i], 1)));
        }
        return OBJECT_VAL(table);
    }

    ObjectTable* table = newTable(vm, 1);
    objectTableSet(vm, table, NUMBER_VAL(0), value);
    return OBJECT_VAL(table);
}

static Value castNumber(VM *vm, Value *args, bool* success) {
    Value value = args[0];

    if (IS_NUMBER(value)) {
        return value;
    }

    if (IS_STL_STRING(value)) {
        char* str = AS_C_STRING(value);
        char* end;
        double num = strtod(str, &end);

        // can check for overflow or underflow with errno

        if (end != str) {
            return NUMBER_VAL(num);
        }
    }

    if (IS_BOOL(value)) {
        return NUMBER_VAL(AS_BOOL(value) ? 1 : 0);
    }

    if (IS_NIL(value)) {
        return NUMBER_VAL(0);
    }

    *success = false;
    return NIL_VAL;
}

ObjectResult* numberNative(VM *vm, int argCount, Value *args) {
    bool success = true;
    Value value = castNumber(vm, args, &success);
    if (!success) {
    return stellaErr(vm, newError(vm, copyString(vm, "Cannot convert value to number.", 30), TYPE, false));
    }
    return stellaOk(vm, value);
}

ObjectResult* stringNative(VM *vm, int argCount, Value *args) {
    Value value = args[0];
    return stellaOk(vm, OBJECT_VAL(toString(vm, value)));
}

ObjectResult* arrayNative(VM *vm, int argCount, Value *args) {
    bool success = true;
    Value array = castArray(vm, args, &success);
    if (!success) {
        return stellaErr(vm, newError(vm, copyString(vm, "Failed to convert value to array.", 33), RUNTIME, false));
    }
    return stellaOk(vm, OBJECT_VAL(array));
}

ObjectResult* tableNative(VM *vm, int argCount, Value *args) {
    return stellaOk(vm, castTable(vm, args));
}

Value numberNative_(VM *vm, int argCount, Value *args) {
    bool success = true;
    return castNumber(vm, args, &success);
}

Value stringNative_(VM *vm, int argCount, Value *args) {
    return OBJECT_VAL(toString(vm, args[0]));
}

Value arrayNative_(VM *vm, int argCount, Value *args) {
    bool success = true;
    Value array = castArray(vm, args, &success);
    if (!success) {
        return NIL_VAL;
    }
    return OBJECT_VAL(array);
}

Value tableNative_(VM *vm, int argCount, Value *args) {
    return castTable(vm, args);
}
