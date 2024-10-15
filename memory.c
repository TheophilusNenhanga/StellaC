#include "memory.h"
#include <stdlib.h>
#include "object.h"
#include "vm.h"

void *reallocate(void *pointer, size_t oldSize, size_t newSize) {
	if (newSize == 0) {
		free(pointer);
		return NULL;
	}

	void *result = realloc(pointer, newSize); // When oldSize is zero this is equivalent to malloc
	if (result == NULL)
		exit(1);
	return result;
}

static void freeObject(Object *object) {
	switch (object->type) {
		case OBJECT_STRING: {
			ObjectString *string = (ObjectString *) object;
			FREE_ARRAY(char, string->chars, string->length + 1);
			FREE(ObjectString, object);
			break;
		}
		case OBJECT_FUNCTION: {
			ObjectFunction *function = (ObjectFunction *) object;
			freeChunk(&function->chunk);
			FREE(ObjectFunction, object);
			break;
		}
		case OBJECT_NATIVE: {
			FREE(ObjectNative, object);
			break;
		}
		case OBJECT_CLOSURE: {
			ObjectClosure *closure = (ObjectClosure *) object;
			FREE_ARRAY(ObjectUpvalue *, closure->upvalues, closure->upvalueCount);
			FREE(ObjectClosure, object);
			break;
		}
		case OBJECT_UPVALUE: {
			FREE(ObjectUpvalue, object);
			break;
		}
	}
}

void freeObjects() {
	Object *object = vm.objects;
	while (object != NULL) {
		Object *next = object->next;
		freeObject(object);
		object = next;
	}
}
