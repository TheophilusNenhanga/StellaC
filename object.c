#include "object.h"
#include <stdio.h>
#include <string.h>
#include "memory.h"
#include "value.h"
#include "vm.h"

#define ALLOCATE_OBJECT(type, objectType) (type *) allocateObject(sizeof(type), objectType)

static Object *allocateObject(size_t size, ObjectType type) {
	Object *object = (Object *) reallocate(NULL, 0, size);

#ifdef DEBUG_LOG_GC
	printf("%p mark ", (void *) object);
	printValue(OBJECT_VAL(object));
	printf("\n");
#endif

	object->type = type;
	object->next = vm.objects;
	object->isMarked = false;
	object->hash = 2166136261u;
	vm.objects = object;

#ifdef DEBUG_LOG_GC
	printf("%p allocate %zu for %d\n", (void *) object, size, type); // TODO: GET THE NAME OF ENUM FIELD
#endif

	return object;
}

ObjectBoundMethod *newBoundMethod(Value receiver, ObjectClosure *method) {
	ObjectBoundMethod *bound = ALLOCATE_OBJECT(ObjectBoundMethod, OBJECT_BOUND_METHOD);
	bound->receiver = receiver;
	bound->method = method;
	return bound;
}

ObjectClass *newClass(ObjectString *name) {
	ObjectClass *klass = ALLOCATE_OBJECT(ObjectClass, OBJECT_CLASS);
	initTable(&klass->methods);
	klass->name = name;
	klass->object.hash = hashBytes(&klass, sizeof(*klass));
	return klass;
}

ObjectUpvalue *newUpvalue(Value *slot) {
	ObjectUpvalue *upvalue = ALLOCATE_OBJECT(ObjectUpvalue, OBJECT_UPVALUE);
	upvalue->location = slot;
	upvalue->next = NULL;
	upvalue->closed = NIL_VAL;
	return upvalue;
}

ObjectClosure *newClosure(ObjectFunction *function) {
	ObjectUpvalue **upvalues = ALLOCATE(ObjectUpvalue *, function->upvalueCount);
	for (int i = 0; i < function->upvalueCount; i++) {
		upvalues[i] = NULL;
	}

	ObjectClosure *closure = ALLOCATE_OBJECT(ObjectClosure, OBJECT_CLOSURE);
	closure->function = function;
	closure->upvalues = upvalues;
	closure->upvalueCount = function->upvalueCount;
	closure->object.hash = hashBytes(&closure, sizeof(*closure));
	return closure;
}

static ObjectString *allocateString(char *chars, int length, uint32_t hash) {
	// creates a copy of the characters on the heap
	// that the ObjectString can own
	ObjectString *string = ALLOCATE_OBJECT(ObjectString, OBJECT_STRING);
	string->length = length;
	string->chars = chars;
	string->object.hash = hash;
	push(OBJECT_VAL(string));
	tableSet(&vm.strings, string, NIL_VAL);
	pop();
	return string;
}

uint32_t hashBytes(const void *data, size_t size) {
	uint32_t hash = 2166136261u;
	const uint8_t *bytes = (const uint8_t *) data;

	for (int i = 0; i < size; i++) {
		hash ^= bytes[i];
		hash *= FNV_PRIME;
	}
	return hash;
}


ObjectString *copyString(const char *chars, int length) {
	uint32_t hash = hashBytes(chars, length);

	ObjectString *interned = tableFindString(&vm.strings, chars, length, hash);
	if (interned != NULL)
		return interned;

	char *heapChars = ALLOCATE(char, length + 1);
	memcpy(heapChars, chars, length);
	heapChars[length] = '\0';
	return allocateString(heapChars, length, hash);
}

static void printFunction(ObjectFunction *function) {
	if (function->name == NULL) {
		printf("<script>");
		return;
	}
	printf("<fn %s>", function->name->chars);
}

void printObject(Value value) {
	switch (OBJECT_TYPE(value)) {
		case OBJECT_CLASS: {
			printf("'%s' <class>", AS_CLASS(value)->name->chars);
			break;
		}
		case OBJECT_STRING: {
			printf("%s", AS_CSTRING(value));
			break;
		}
		case OBJECT_FUNCTION: {
			printFunction(AS_FUNCTION(value));
			break;
		}
		case OBJECT_NATIVE: {
			printf("<native fn>");
			break;
		}
		case OBJECT_CLOSURE: {
			printFunction(AS_CLOSURE(value)->function);
			break;
		}
		case OBJECT_UPVALUE: {
			printf("<upvalue>");
			break;
		}
		case OBJECT_INSTANCE: {
			printf("'%s' <instance>", AS_INSTANCE(value)->klass->name->chars);
			break;
		}
		case OBJECT_BOUND_METHOD: {
			printFunction(AS_BOUND_METHOD(value)->method->function);
			break;
		}
		case OBJECT_ARRAY: {
			printf("<array>");
			break;
		}
		case OBJECT_TABLE: {
			printf("<table>");
			break;
		}
	}
}

ObjectString *takeString(char *chars, int length) {
	uint32_t hash = hashBytes(chars, length);

	ObjectString *interned = tableFindString(&vm.strings, chars, length, hash);
	if (interned != NULL) {
		FREE_ARRAY(char, chars, length + 1);
		return interned;
	}

	return allocateString(chars, length, hash);
}

ObjectFunction *newFunction() {
	ObjectFunction *function = ALLOCATE_OBJECT(ObjectFunction, OBJECT_FUNCTION);
	function->arity = 0;
	function->name = NULL;
	function->upvalueCount = 0;
	initChunk(&function->chunk);
	function->object.hash = hashBytes(&function, sizeof(*function));
	return function;
}

ObjectInstance *newInstance(ObjectClass *klass) {
	ObjectInstance *instance = ALLOCATE_OBJECT(ObjectInstance, OBJECT_INSTANCE);
	instance->klass = klass;
	initTable(&instance->fields);
	return instance;
}

ObjectNative *newNative(NativeFn function, int arity) {
	ObjectNative *native = ALLOCATE_OBJECT(ObjectNative, OBJECT_NATIVE);
	native->function = function;
	native->arity = arity;
	native->object.hash = hashBytes(&native, sizeof(*native));
	return native;
}

static int calculateCollectionCapacity(int n) {
	if (n >= UINT16_MAX - 1) {
		return UINT16_MAX - 1;
	}

	if (n < 1)
		return 1;
	n--;
	n |= n >> 1;
	n |= n >> 2;
	n |= n >> 4;
	n |= n >> 8;
	n |= n >> 16;

	return n + 1;
}

ObjectArray *newArray(int elementCount) {
	ObjectArray *array = ALLOCATE_OBJECT(ObjectArray, OBJECT_ARRAY);
	array->capacity = calculateCollectionCapacity(elementCount);
	array->size = 0;
	array->array = ALLOCATE(Value, array->capacity);
	return array;
}

ObjectArray *growArray(ObjectArray *array) {
	array->array = GROW_ARRAY(Value, array->array, array->capacity, array->capacity * 2);
	array->capacity *= 2;
	return array;
}


ObjectTable *newTable(int elementCount) {
	ObjectTable *table = ALLOCATE_OBJECT(ObjectTable, OBJECT_TABLE);
	if (elementCount < 8) {
		table->capacity = 8;
	} else {
		table->capacity = calculateCollectionCapacity(elementCount);
	}
	table->size = 0;
	table->values = ALLOCATE(ValueEntry, table->capacity);
	for (int i = 0; i < table->capacity; i++) {
		table->values[i].key = NIL_VAL;
		table->values[i].value = NIL_VAL;
		table->values[i].isOccupied = false;
	}
	return table;
}

static ValueEntry *findEntry(ValueEntry *entries, int capacity, Value key) {
	uint64_t hash;
	if (IS_NUMBER(key)) {
		hash = (uint64_t) AS_NUMBER(key);
	} else if (IS_BOOL(key)) {
		hash = AS_BOOL(key) ? 1 : 0;
	} else if (IS_NIL(key)) {
		hash = 0;
	} else if (IS_OBJECT(key)) {
		hash = AS_OBJECT(key)->hash;
	} else {
		return NULL;
	}
	uint64_t index = hash & (capacity - 1);
	ValueEntry *tombstone = NULL;

	while (true) {
		ValueEntry *entry = &entries[index];
		if (!entry->isOccupied) {
			if (IS_NIL(entry->value)) {
				return tombstone != NULL ? tombstone : entry;
			} else if (tombstone == NULL) {
				tombstone = entry;
			}
		} else if (valuesEqual(entry->key, key)) {
			return entry;
		}
		index = (index + 1) & (capacity - 1);
	}
}

static bool adjustCapacity(ObjectTable *table, int capacity) {
	ValueEntry *values = ALLOCATE(ValueEntry, capacity);
	for (int i = 0; i < capacity; i++) {
		values[i].key = NIL_VAL;
		values[i].value = NIL_VAL;
		values[i].isOccupied = false;
	}

	table->size = 0;
	for (int i = 0; i < table->capacity; i++) {
		ValueEntry *entry = &table->values[i];
		if (!entry->isOccupied) {
			continue;
		}

		ValueEntry *dest = findEntry(values, capacity, entry->key);
		if (dest == NULL) {
			FREE_ARRAY(ValueEntry, values, capacity);
			return false;
		}
		dest->key = entry->key;
		dest->value = entry->value;
		dest->isOccupied = true;
		table->size++;
	}
	FREE_ARRAY(ValueEntry, table->values, table->capacity);
	table->values = values;
	table->capacity = capacity;
	return true;
}

bool objectTableSet(ObjectTable *table, Value key, Value value) {
	if (table->size + 1 > table->capacity * TABLE_MAX_LOAD) {
		int capacity = GROW_CAPACITY(table->capacity);
		if (!adjustCapacity(table, capacity))
			return false;
	}

	ValueEntry *entry = findEntry(table->values, table->capacity, key);
	if (entry == NULL) {
		return false;
	}
	bool isNewKey = !entry->isOccupied;

	if (isNewKey) {
		table->size++;
	}

	entry->key = key;
	entry->value = value;
	entry->isOccupied = true;
	return true;
}

bool objectTableGet(ObjectTable *table, Value key, Value *value) {
	if (table->size == 0) {
		return false;
	}

	ValueEntry *entry = findEntry(table->values, table->capacity, key);
	if (!entry->isOccupied) {
		return false;
	}
	*value = entry->value;
	return true;
}

bool objectTableDelete(ObjectTable *table, Value key) {
	if (table->size == 0) {
		return false;
	}

	ValueEntry *entry = findEntry(table->values, table->capacity, key);
	if (!entry->isOccupied) {
		return false;
	}
	entry->key = NIL_VAL;
	entry->value = BOOL_VAL(true); // tombstone
	entry->isOccupied = false;
	table->size--;
	return true;
}

void objectTableRemoveWhite(ObjectTable *table) {
	for (int i = 0; i < table->capacity; i++) {
		ValueEntry *entry = &table->values[i];
		if (entry->isOccupied && !AS_OBJECT(entry->key)->isMarked) {
			objectTableDelete(table, entry->key);
		}
	}
}

void markObjectTable(ObjectTable *table) {
	if (table->values != NULL) {
		for (int i = 0; i < table->capacity; i++) {
			ValueEntry *entry = &table->values[i];
			if (entry->isOccupied) {
				markValue(entry->key);
				markValue(entry->value);
			}
		}
	}
}

void freeObjectTable(ObjectTable *table) {
	FREE_ARRAY(ValueEntry, table->values, table->capacity);
	FREE(ObjectTable, table);
}

bool isValidObject(Object *object) {
	if (object == NULL)
		return false;
	if ((uintptr_t) object % sizeof(void *) != 0)
		return false;
	switch (object->type) {
		case OBJECT_STRING:
		case OBJECT_FUNCTION:
		case OBJECT_NATIVE:
		case OBJECT_CLOSURE:
		case OBJECT_UPVALUE:
		case OBJECT_CLASS:
		case OBJECT_INSTANCE:
		case OBJECT_BOUND_METHOD:
		case OBJECT_ARRAY:
		case OBJECT_TABLE:
			return true;
		default:
			return false;
	}
}
