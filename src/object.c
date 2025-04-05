#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <sys/time.h>
#endif

#include "memory.h"
#include "object.h"

/**
 * @brief Allocates a new object of the specified type.
 *
 * This is a static helper function used to allocate memory for a new object
 * and initialize its basic properties. It uses the `reallocate` function
 * for memory management, integrates with the garbage collector by linking
 * the new object into the VM's object list, and sets the object's type and
 * initial marking status.
 *
 * @param vm The virtual machine.
 * @param size The size of the object to allocate in bytes.
 * @param type The type of the object being allocated (ObjectType enum).
 *
 * @return A pointer to the newly allocated and initialized Object.
 */
static Object *allocateObject(VM *vm, size_t size, ObjectType type) {
	Object *object = (Object *) reallocate(vm, NULL, 0, size);

#ifdef DEBUG_LOG_GC
	printf("%p mark ", (void *) object);
	printValue(OBJECT_VAL(object));
	printf("\n");
#endif

	object->type = type;
	object->next = vm->objects;
	object->isMarked = false;
	vm->objects = object;

#ifdef DEBUG_LOG_GC
	printf("%p allocate %zu for %d\n", (void *) object, size, type);
#endif

	return object;
}
/**
 * @brief Macro to allocate a specific type of object.
 *
 * This macro simplifies object allocation by wrapping the `allocateObject`
 * function. It casts the result of `allocateObject` to the desired object type,
 * reducing code duplication and improving readability.
 *
 * @param vm The virtual machine.
 * @param type The C type of the object to allocate (e.g., ObjectString).
 * @param objectType The ObjectType enum value for the object.
 *
 * @return A pointer to the newly allocated object, cast to the specified type.
 */
#define ALLOCATE_OBJECT(vm, type, objectType) (type *) allocateObject(vm, sizeof(type), objectType)

/**
 * @brief Calculates the next power of 2 capacity for a collection.
 *
 * This static helper function calculates the next power of 2 capacity for
 * collections like tables and arrays. It ensures efficient hash table
 * resizing and memory allocation by always using power-of-two capacities.
 * If the input `n` is close to `UINT16_MAX`, it returns `UINT16_MAX - 1` to avoid potential overflow.
 *
 * @param n The desired minimum capacity.
 *
 * @return The next power of 2 capacity greater than or equal to `n`, or `UINT16_MAX - 1` if `n` is close to the maximum.
 */
static uint64_t calculateCollectionCapacity(uint64_t n) {
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

/**
 * @brief Generates a hash code for a Value.
 *
 * This static helper function calculates a hash code for a given `Value`.
 * It handles different value types (strings, numbers, booleans, nil) to
 * produce a suitable hash value for use in hash tables.
 *
 * @param value The Value to hash.
 *
 * @return A 32-bit hash code for the Value.
 */
static uint32_t hashValue(Value value) {
	if (IS_CRUX_STRING(value)) {
		return AS_CRUX_STRING(value)->hash;
	}
	if (IS_INT(value)) {
		return (uint32_t) AS_INT(value);
	}
	if (IS_FLOAT(value)) {
		double num = AS_FLOAT(value);
		uint64_t bits;
		memcpy(&bits, &num, sizeof(bits));
		return (uint32_t) (bits ^ (bits >> 32));
	}
	if (IS_BOOL(value)) {
		return AS_BOOL(value) ? 1u : 0u;
	}
	if (IS_NIL(value)) {
		return 4321u;
	}
	return 0u;
}

void printType(Value value) {
	if (!IS_CRUX_OBJECT(value)) {
		printValue(value);
		return;
	}
	switch (OBJECT_TYPE(value)) {
		case OBJECT_STRING:
			printf("'string'");
			break;
		case OBJECT_FUNCTION:
		case OBJECT_NATIVE_FUNCTION:
		case OBJECT_NATIVE_INFALLIBLE_FUNCTION:
		case OBJECT_NATIVE_METHOD:
		case OBJECT_NATIVE_INFALLIBLE_METHOD:
		case OBJECT_CLOSURE:
		case OBJECT_BOUND_METHOD:
			printf("'function'");
			break;
		case OBJECT_UPVALUE:
			printf("'upvalue'");
			break;
		case OBJECT_CLASS:
			printf("'class'");
			break;
		case OBJECT_INSTANCE:
			printf("'instance'");
			break;
		case OBJECT_ARRAY:
			printf("'array'");
			break;
		case OBJECT_TABLE:
			printf("'table'");
			break;
		case OBJECT_ERROR:
			printf("'error'");
			break;
		case OBJECT_RESULT:
			printf("'result'");
			break;
		case OBJECT_RANDOM:
			printf("'random'");
			break;
		case OBJECT_FILE:
			printf("'file'");
			break;
		default:
			printf("'unknown'");
	}
}

ObjectBoundMethod *newBoundMethod(VM *vm, Value receiver, ObjectClosure *method) {
	ObjectBoundMethod *bound = ALLOCATE_OBJECT(vm, ObjectBoundMethod, OBJECT_BOUND_METHOD);
	bound->receiver = receiver;
	bound->method = method;
	return bound;
}

ObjectClass *newClass(VM *vm, ObjectString *name) {
	ObjectClass *klass = ALLOCATE_OBJECT(vm, ObjectClass, OBJECT_CLASS);
	initTable(&klass->methods);
	klass->name = name;
	return klass;
}

ObjectUpvalue *newUpvalue(VM *vm, Value *slot) {
	ObjectUpvalue *upvalue = ALLOCATE_OBJECT(vm, ObjectUpvalue, OBJECT_UPVALUE);
	upvalue->location = slot;
	upvalue->next = NULL;
	upvalue->closed = NIL_VAL;
	return upvalue;
}

ObjectClosure *newClosure(VM *vm, ObjectFunction *function) {
	ObjectUpvalue **upvalues = ALLOCATE(vm, ObjectUpvalue *, function->upvalueCount);
	for (int i = 0; i < function->upvalueCount; i++) {
		upvalues[i] = NULL;
	}

	ObjectClosure *closure = ALLOCATE_OBJECT(vm, ObjectClosure, OBJECT_CLOSURE);
	closure->function = function;
	closure->upvalues = upvalues;
	closure->upvalueCount = function->upvalueCount;
	return closure;
}

/**
 * @brief Allocates a new string object.
 *
 * This static helper function allocates a new ObjectString and copies the given
 * character array into the object's memory. It also calculates and stores the
 * string's hash value and interns the string in the VM's string table.
 *
 * @param vm The virtual machine.
 * @param chars The character array for the string. This memory is assumed to be managed externally and copied.
 * @param length The length of the string.
 * @param hash The pre-calculated hash value of the string.
 *
 * @return A pointer to the newly created and interned ObjectString.
 */
static ObjectString *allocateString(VM *vm, char *chars, uint32_t length, uint32_t hash) {
	ObjectString *string = ALLOCATE_OBJECT(vm, ObjectString, OBJECT_STRING);
	string->length = length;
	string->chars = chars;
	string->hash = hash;
	// intern the string
	push(vm, OBJECT_VAL(string));
	tableSet(vm, &vm->strings, string, NIL_VAL, false);
	pop(vm);
	return string;
}

/**
 * @brief Calculates the hash value of a C-style string.
 *
 * This function implements the FNV-1a hash algorithm to generate a hash
 * code for a given C-style string.
 *
 * @param key The null-terminated C-style string to hash.
 * @param length The length of the string (excluding null terminator).
 *
 * @return A 32-bit hash code for the string.
 */
static inline uint32_t hashString(const char *key, size_t length) {
	static const uint32_t FNV_OFFSET_BIAS = 2166136261u;
	static const uint32_t FNV_PRIME = 16777619u;

	uint32_t hash = FNV_OFFSET_BIAS;
	for (int i = 0; i < length; i++) {
		hash ^= (uint8_t) key[i];
		hash *= FNV_PRIME;
	}
	return hash;
}

ObjectString *copyString(VM *vm, const char *chars, uint32_t length) {
	uint32_t hash = hashString(chars, length);

	ObjectString *interned = tableFindString(&vm->strings, chars, length, hash);
	if (interned != NULL)
		return interned;

	char *heapChars = ALLOCATE(vm, char, length + 1);
	memcpy(heapChars, chars, length);
	heapChars[length] = '\0'; // terminating the string because it is not terminated in the source
	return allocateString(vm, heapChars, length, hash);
}

/**
 * @brief Prints the name of a function object.
 *
 * This static helper function prints the name of a function object to the console,
 * used for debugging and representation purposes. If the function is anonymous
 * (name is NULL), it prints "<script>".
 *
 * @param function The ObjectFunction to print the name of.
 */
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
			printf("'%s' <class>", AS_CRUX_CLASS(value)->name->chars);
			break;
		}
		case OBJECT_STRING: {
			printf("%s", AS_C_STRING(value));
			break;
		}
		case OBJECT_FUNCTION: {
			printFunction(AS_CRUX_FUNCTION(value));
			break;
		}
		case OBJECT_NATIVE_FUNCTION: {
			ObjectNativeFunction *native = AS_CRUX_NATIVE_FUNCTION(value);
			if (native->name != NULL) {
				printf("<native fn %s>", native->name->chars);
			} else {
				printf("<native fn>");
			}
			break;
		}
		case OBJECT_NATIVE_METHOD: {
			ObjectNativeMethod *native = AS_CRUX_NATIVE_METHOD(value);
			if (native->name != NULL) {
				printf("<native method %s>", native->name->chars);
			} else {
				printf("<native method>");
			}
			break;
		}
		case OBJECT_NATIVE_INFALLIBLE_FUNCTION: {
			ObjectNativeInfallibleFunction *native = AS_CRUX_NATIVE_INFALLIBLE_FUNCTION(value);
			if (native->name != NULL) {
				printf("<native infallible fn %s>", native->name->chars);
			} else {
				printf("<native infallible fn>");
			}
			break;
		}
		case OBJECT_CLOSURE: {
			printFunction(AS_CRUX_CLOSURE(value)->function);
			break;
		}
		case OBJECT_UPVALUE: {
			printf("<upvalue>");
			break;
		}
		case OBJECT_INSTANCE: {
			printf("'%s' <instance>", AS_CRUX_INSTANCE(value)->klass->name->chars);
			break;
		}
		case OBJECT_BOUND_METHOD: {
			printFunction(AS_CRUX_BOUND_METHOD(value)->method->function);
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
		case OBJECT_ERROR: {
			printf("<error>");
			break;
		}
		case OBJECT_MODULE: {
			printf("<module>");
			break;
		}
		case OBJECT_RESULT: {
			printf("<result>");
			break;
		}
		case OBJECT_RANDOM: {
			printf("<random>");
			break;
		}
		case OBJECT_FILE: {
			printf("<file>");
			break;
		}
		case OBJECT_NATIVE_INFALLIBLE_METHOD:
			printf("<native infallible method>");
			break;
	}
}

ObjectString *takeString(VM *vm, char *chars, uint32_t length) {
	uint32_t hash = hashString(chars, length);

	ObjectString *interned = tableFindString(&vm->strings, chars, length, hash);
	if (interned != NULL) {
		// free the string that was passed to us.
		FREE_ARRAY(vm, char, chars, length + 1);
		return interned;
	}

	return allocateString(vm, chars, length, hash);
}


ObjectString *toString(VM *vm, Value value) {
	if (!IS_CRUX_OBJECT(value)) {
		char buffer[32];
		if (IS_FLOAT(value)) {
			double num = AS_FLOAT(value);
			snprintf(buffer, sizeof(buffer), "%g", num);
		}else if (IS_INT(value)) {
			int32_t num = AS_INT(value);
			snprintf(buffer, sizeof(buffer), "%d", num);
		} else if (IS_BOOL(value)) {
			strcpy(buffer, AS_BOOL(value) ? "true" : "false");
		} else if (IS_NIL(value)) {
			strcpy(buffer, "nil");
		}
		return copyString(vm, buffer, (int) strlen(buffer));
	}

	switch (OBJECT_TYPE(value)) {
		case OBJECT_STRING:
			return AS_CRUX_STRING(value);

		case OBJECT_FUNCTION: {
			ObjectFunction *function = AS_CRUX_FUNCTION(value);
			if (function->name == NULL) {
				return copyString(vm, "<script>", 8);
			}
			char buffer[64];
			int length = snprintf(buffer, sizeof(buffer), "<fn %s>", function->name->chars);
			return copyString(vm, buffer, length);
		}

		case OBJECT_NATIVE_FUNCTION: {
			ObjectNativeFunction *native = AS_CRUX_NATIVE_FUNCTION(value);
			if (native->name != NULL) {
				char *start = "<native fn ";
				char *end = ">";
				char *buffer = ALLOCATE(vm, char, strlen(start) + strlen(end) + native->name->length + 1);
				strcpy(buffer, start);
				strcat(buffer, native->name->chars);
				strcat(buffer, end);
				ObjectString *result = takeString(vm, buffer, strlen(buffer));
				FREE_ARRAY(vm, char, buffer, strlen(buffer) + 1);
				return result;
			}
			return copyString(vm, "<native fn>", 11);
		}

		case OBJECT_NATIVE_METHOD: {
			ObjectNativeMethod *native = AS_CRUX_NATIVE_METHOD(value);
			if (native->name != NULL) {
				char *start = "<native method ";
				char *end = ">";
				char *buffer = ALLOCATE(vm, char, strlen(start) + strlen(end) + native->name->length + 1);
				strcpy(buffer, start);
				strcat(buffer, native->name->chars);
				strcat(buffer, end);
				ObjectString *result = takeString(vm, buffer, strlen(buffer));
				FREE_ARRAY(vm, char, buffer, strlen(buffer) + 1);
				return result;
			}
			return copyString(vm, "<native method>", 15);
		}

		case OBJECT_NATIVE_INFALLIBLE_FUNCTION: {
			ObjectNativeInfallibleFunction *native = AS_CRUX_NATIVE_INFALLIBLE_FUNCTION(value);
			if (native->name != NULL) {
				char *start = "<native infallible fn ";
				char *end = ">";
				char *buffer = ALLOCATE(vm, char, strlen(start) + strlen(end) + native->name->length + 1);
				strcpy(buffer, start);
				strcat(buffer, native->name->chars);
				strcat(buffer, end);
				ObjectString *result = takeString(vm, buffer, strlen(buffer));
				FREE_ARRAY(vm, char, buffer, strlen(buffer) + 1);
				return result;
			}
			return copyString(vm, "<native infallible fn>", 21);
		}
		
		case OBJECT_NATIVE_INFALLIBLE_METHOD: {
			ObjectNativeInfallibleMethod *native = AS_CRUX_NATIVE_INFALLIBLE_METHOD(value);
			if (native->name != NULL) {
				char *start = "<native infallible method ";
				char *end = ">";
				char *buffer = ALLOCATE(vm, char, strlen(start) + strlen(end) + native->name->length + 1);
				strcpy(buffer, start);
				strcat(buffer, native->name->chars);
				strcat(buffer, end);
				ObjectString *result = takeString(vm, buffer, strlen(buffer));
				FREE_ARRAY(vm, char, buffer, strlen(buffer) + 1);
				return result;
			}
			return copyString(vm, "<native infallible method>", 25);
		}

		case OBJECT_CLOSURE: {
			ObjectFunction *function = AS_CRUX_CLOSURE(value)->function;
			if (function->name == NULL) {
				return copyString(vm, "<script>", 8);
			}
			char buffer[256];
			int length = snprintf(buffer, sizeof(buffer), "<fn %s>", function->name->chars);
			return copyString(vm, buffer, length);
		}

		case OBJECT_UPVALUE: {
			return copyString(vm, "<upvalue>", 9);
		}

		case OBJECT_CLASS: {
			ObjectClass *klass = AS_CRUX_CLASS(value);
			char buffer[256];
			int length = snprintf(buffer, sizeof(buffer), "%s <class>", klass->name->chars);
			return copyString(vm, buffer, length);
		}

		case OBJECT_INSTANCE: {
			ObjectInstance *instance = AS_CRUX_INSTANCE(value);
			char buffer[256];
			int length = snprintf(buffer, sizeof(buffer), "%s <instance>", instance->klass->name->chars);
			return copyString(vm, buffer, length);
		}

		case OBJECT_BOUND_METHOD: {
			ObjectBoundMethod *bound = AS_CRUX_BOUND_METHOD(value);
			char buffer[256];
			int length = snprintf(buffer, sizeof(buffer), "<bound fn %s>", bound->method->function->name->chars);
			return copyString(vm, buffer, length);
		}

		case OBJECT_ARRAY: {
			ObjectArray *array = AS_CRUX_ARRAY(value);
			size_t bufSize = 2; // [] minimum
			for (int i = 0; i < array->size; i++) {
				ObjectString *element = toString(vm, array->array[i]);
				bufSize += element->length + 2; // element + ", "
			}

			char *buffer = ALLOCATE(vm, char, bufSize);
			char *ptr = buffer;
			*ptr++ = '[';

			for (int i = 0; i < array->size; i++) {
				if (i > 0) {
					*ptr++ = ',';
					*ptr++ = ' ';
				}
				ObjectString *element = toString(vm, array->array[i]);
				memcpy(ptr, element->chars, element->length);
				ptr += element->length;
			}
			*ptr++ = ']';

			ObjectString *result = takeString(vm, buffer, ptr - buffer);
			return result;
		}

		case OBJECT_TABLE: {
			ObjectTable *table = AS_CRUX_TABLE(value);
			size_t bufSize = 2; // {} minimum
			for (int i = 0; i < table->capacity; i++) {
				if (table->entries[i].isOccupied) {
					ObjectString *k = toString(vm, table->entries[i].key);
					ObjectString *v = toString(vm, table->entries[i].value);
					bufSize += k->length + v->length + 4; // key:value,
				}
			}

			char *buffer = ALLOCATE(vm, char, bufSize);
			char *ptr = buffer;
			*ptr++ = '{';

			bool first = true;
			for (int i = 0; i < table->capacity; i++) {
				if (table->entries[i].isOccupied) {
					if (!first) {
						*ptr++ = ',';
						*ptr++ = ' ';
					}
					first = false;

					ObjectString *key = toString(vm, table->entries[i].key);
					ObjectString *val = toString(vm, table->entries[i].value);

					memcpy(ptr, key->chars, key->length);
					ptr += key->length;
					*ptr++ = ':';
					memcpy(ptr, val->chars, val->length);
					ptr += val->length;
				}
			}
			*ptr++ = '}';

			ObjectString *result = takeString(vm, buffer, ptr - buffer);
			return result;
		}

		case OBJECT_ERROR: {
			ObjectError *error = AS_CRUX_ERROR(value);
			char buffer[1024];
			int length = snprintf(buffer, sizeof(buffer), "<error: %s>", error->message->chars);
			return copyString(vm, buffer, length);
		}

		case OBJECT_RESULT: {
			ObjectResult *result = AS_CRUX_RESULT(value);
			if (result->isOk) {
				return copyString(vm, "<Ok>", 4);
			}
			return copyString(vm, "<Err>", 5);
		}

		case OBJECT_RANDOM: {
			return copyString(vm, "<random>", 8);
		}

		case OBJECT_FILE: {
			return copyString(vm, "<file>", 6);
		}

		default:
			return copyString(vm, "<stella object>", 15);
	}
}

ObjectFunction *newFunction(VM *vm) {
	ObjectFunction *function = ALLOCATE_OBJECT(vm, ObjectFunction, OBJECT_FUNCTION);
	function->arity = 0;
	function->name = NULL;
	function->upvalueCount = 0;
	initChunk(&function->chunk);
	return function;
}

ObjectInstance *newInstance(VM *vm, ObjectClass *klass) {
	ObjectInstance *instance = ALLOCATE_OBJECT(vm, ObjectInstance, OBJECT_INSTANCE);
	instance->klass = klass;
	initTable(&instance->fields);
	return instance;
}

ObjectNativeFunction *newNativeFunction(VM *vm, StellaNativeCallable function, int arity, ObjectString *name) {
	ObjectNativeFunction *native = ALLOCATE_OBJECT(vm, ObjectNativeFunction, OBJECT_NATIVE_FUNCTION);
	native->function = function;
	native->arity = arity;
	native->name = name;
	return native;
}

ObjectNativeMethod *newNativeMethod(VM *vm, StellaNativeCallable function, int arity, ObjectString *name) {
	ObjectNativeMethod *native = ALLOCATE_OBJECT(vm, ObjectNativeMethod, OBJECT_NATIVE_METHOD);
	native->function = function;
	native->arity = arity;
	native->name = name;
	return native;
}

ObjectNativeInfallibleFunction *newNativeInfallibleFunction(VM *vm, StellaInfallibleCallable function, int arity, ObjectString *name) {
	ObjectNativeInfallibleFunction *native = ALLOCATE_OBJECT(vm, ObjectNativeInfallibleFunction, OBJECT_NATIVE_INFALLIBLE_FUNCTION);
	native->function = function;
	native->arity = arity;
	native->name = name;
	return native;
}

ObjectNativeInfallibleMethod *newNativeInfallibleMethod(VM *vm, StellaInfallibleCallable function, int arity, ObjectString *name) {
	ObjectNativeInfallibleMethod *native = ALLOCATE_OBJECT(vm, ObjectNativeInfallibleMethod, OBJECT_NATIVE_INFALLIBLE_METHOD);
	native->function = function;
	native->arity = arity;
	native->name = name;
	return native;
}
ObjectTable *newTable(VM *vm, int elementCount) {
	ObjectTable *table = ALLOCATE_OBJECT(vm, ObjectTable, OBJECT_TABLE);
	table->capacity = elementCount < 16 ? 16 : calculateCollectionCapacity(elementCount);
	table->size = 0;
	table->entries = ALLOCATE(vm, ObjectTableEntry, table->capacity);
	for (int i = 0; i < table->capacity; i++) {
		table->entries[i].value = NIL_VAL;
		table->entries[i].key = NIL_VAL;
		table->entries[i].isOccupied = false;
	}
	return table;
}

void freeObjectTable(VM *vm, ObjectTable *table) {
	FREE_ARRAY(vm, ObjectTableEntry, table->entries, table->capacity);
	table->entries = NULL;
	table->capacity = 0;
	table->size = 0;
}

ObjectFile* newObjectFile(VM *vm, ObjectString *path, ObjectString *mode) {
	ObjectFile* file = ALLOCATE_OBJECT(vm,ObjectFile, OBJECT_FILE);
	file->path = path;
	file->mode = mode;
	file->file = fopen(path->chars, mode->chars);
	file->isOpen = file->file != NULL;
	file->position = 0;
	return file;
}

/**
 * @brief Finds an entry in an object table.
 *
 * This static helper function finds an entry in an ObjectTable's entry array
 * based on a given key. It uses open addressing with quadratic probing to
 * resolve collisions. It also handles tombstones (entries that were previously
 * occupied but are now deleted) to allow for rehashing after deletions.
 *
 * @param entries The array of ObjectTableEntry.
 * @param capacity The capacity of the table's entry array.
 * @param key The key Value to search for.
 *
 * @return A pointer to the ObjectTableEntry for the key, or a pointer to an empty entry (possibly a tombstone) if the key is not found.
 */
static ObjectTableEntry *findEntry(ObjectTableEntry *entries, uint16_t capacity, Value key) {
	uint32_t hash = hashValue(key);
	uint32_t index = hash & (capacity - 1);
	ObjectTableEntry *tombstone = NULL;

	while (1) {
		ObjectTableEntry *entry = &entries[index];
		if (!entry->isOccupied) {
			if (IS_NIL(entry->value)) {
				return tombstone != NULL ? tombstone : entry;
			} else if (tombstone == NULL) {
				tombstone = entry;
			}
		} else if (valuesEqual(entry->key, key)) {
			return entry;
		}
		// index = (index + 1) & (capacity - 1); // old probe
		index = (index * 5 + 1) & (capacity - 1); // new probe
	}
}

/**
 * @brief Adjusts the capacity of an object table.
 *
 * This static helper function resizes an ObjectTable to a new capacity. It
 * allocates a new entry array with the new capacity, rehashes all existing
 * entries into the new array, and frees the old entry array.
 *
 * @param vm The virtual machine.
 * @param table The ObjectTable to resize.
 * @param capacity The new capacity for the table.
 *
 * @return true if the capacity adjustment was successful, false otherwise (e.g., memory allocation failure).
 */
static bool adjustCapacity(VM *vm, ObjectTable *table, int capacity) {
	ObjectTableEntry *entries = ALLOCATE(vm, ObjectTableEntry, capacity);
	if (entries == NULL) {
		return false;
	}

	for (int i = 0; i < capacity; i++) {
		entries[i].key = NIL_VAL;
		entries[i].value = NIL_VAL;
		entries[i].isOccupied = false;
	}

	table->size = 0;

	for (int i = 0; i < table->capacity; i++) {
		ObjectTableEntry *entry = &table->entries[i];
		if (!entry->isOccupied) {
			continue;
		}

		ObjectTableEntry *dest = findEntry(entries, capacity, entry->key);

		dest->key = entry->key;
		dest->value = entry->value;
		dest->isOccupied = true;
		table->size++;
	}

	FREE_ARRAY(vm, ObjectTableEntry, table->entries, table->capacity);
	table->entries = entries;
	table->capacity = capacity;
	return true;
}

bool objectTableSet(VM *vm, ObjectTable *table, Value key, Value value) {
	if (table->size + 1 > table->capacity * TABLE_MAX_LOAD) {
		int capacity = GROW_CAPACITY(table->capacity);
		if (!adjustCapacity(vm, table, capacity))
			return false;
	}

	ObjectTableEntry *entry = findEntry(table->entries, table->capacity, key);
	bool isNewKey = !entry->isOccupied;

	if (isNewKey && IS_NIL(entry->value)) {
		table->size++;
	}

	if (IS_CRUX_OBJECT(key))
		markValue(vm, key);
	if (IS_CRUX_OBJECT(value))
		markValue(vm, value);

	entry->key = key;
	entry->value = value;
	entry->isOccupied = true;

	return true;
}


bool objectTableGet(ObjectTable *table, Value key, Value *value) {
	if (table->size == 0) {
		return false;
	}

	ObjectTableEntry *entry = findEntry(table->entries, table->capacity, key);
	if (!entry->isOccupied) {
		return false;
	}
	*value = entry->value;
	return true;
}


ObjectArray *newArray(VM *vm, uint32_t elementCount) {
	ObjectArray *array = ALLOCATE_OBJECT(vm, ObjectArray, OBJECT_ARRAY);
	array->capacity = calculateCollectionCapacity(elementCount);
	array->size = 0;
	array->array = ALLOCATE(vm, Value, array->capacity);
	for (int i = 0; i < array->capacity; i++) {
		array->array[i] = NIL_VAL;
	}
	return array;
}


bool ensureCapacity(VM *vm, ObjectArray *array, uint32_t capacityNeeded) {
	if (capacityNeeded <= array->capacity) {
		return true;
	}
	uint32_t newCapacity = array->capacity;
	while (newCapacity < capacityNeeded) {
		if (newCapacity > INT_MAX / 2) {
			return false;
		}
		newCapacity *= 2;
	}
	Value *newArray = GROW_ARRAY(vm, Value, array->array, array->capacity, newCapacity);
	if (newArray == NULL) {
		return false;
	}
	for (uint32_t i = array->capacity; i < newCapacity; i++) {
		newArray[i] = NIL_VAL;
	}
	array->array = newArray;
	array->capacity = newCapacity;
	return true;
}


bool arraySet(VM *vm, ObjectArray *array, uint32_t index, Value value) {
	if (index >= array->size) {
		return false;
	}
	if (IS_CRUX_OBJECT(value)) {
		markValue(vm, value);
	}
	array->array[index] = value;
	return true;
}

bool arrayGet(ObjectArray *array, uint32_t index, Value *value) {
	if (array == NULL) {
		return false;
	}
	*value = array->array[index];
	return true;
}

bool arrayAdd(VM *vm, ObjectArray *array, Value value, uint32_t index) {
	if (!ensureCapacity(vm, array, array->size + 1)) {
		return false;
	}
	if (IS_CRUX_OBJECT(value)) {
		markValue(vm, value);
	}
	array->array[index] = value;
	array->size++;
	return true;
}

bool arrayAddBack(VM *vm, ObjectArray *array, Value value) {
	if (!ensureCapacity(vm, array, array->size + 1)) {
		return false;
	}
	array->array[array->size] = value;
	array->size++;
	return true;
}

ObjectError *newError(VM *vm, ObjectString *message, ErrorType type, bool isPanic) {
	ObjectError *error = ALLOCATE_OBJECT(vm, ObjectError, OBJECT_ERROR);
	error->message = message;
	error->type = type;
	error->isPanic = isPanic;
	return error;
}

ObjectModule *newModule(VM *vm, const char *path) {
	ObjectModule *module = ALLOCATE_OBJECT(vm, ObjectModule, OBJECT_MODULE);
	module->path = copyString(vm, path, strlen(path));
	module->state = INITIAL;
	module->vmDepth = 0;
	initImportSet(&module->importedModules);
	return module;
}

void initImportSet(ImportSet* set) {
	set->paths = NULL;
	set->count = 0;
	set->capacity = 0;
}

bool importSetContains(ImportSet* set, ObjectString* path) {
	if (set->count == 0) {
		return false;
	}
	for (int i = 0; i < set->count; i++) {
		if (path == set->paths[i]) {
			return true;
		}
	}
	return false;
}

bool importSetAdd(VM* vm, ImportSet* set, ObjectString* path) {
	if (set->count + 1 > set->capacity) {
		int oldCapacity = set->capacity;
		set->capacity = GROW_CAPACITY(oldCapacity);
		set->paths = GROW_ARRAY(vm, ObjectString*, set->paths, oldCapacity, set->capacity);
	}
	set->paths[set->count++] = path;
	return true;
}

void freeImportSet(VM* vm, ImportSet* set) {
	FREE_ARRAY(vm, ObjectString*, set->paths, set->capacity);
	initImportSet(set);
}


ObjectResult* stellaOk(VM* vm, Value value) {
	ObjectResult *result = ALLOCATE_OBJECT(vm, ObjectResult, OBJECT_RESULT);
	result->isOk = true;
	result->as.value = value;
	return result;
}

ObjectResult* stellaErr(VM *vm, ObjectError* error) {
	ObjectResult *result = ALLOCATE_OBJECT(vm, ObjectResult, OBJECT_RESULT);
	result->isOk = false;
	result->as.error = error;
	return result;
}

ObjectRandom *newRandom(VM *vm) {
	ObjectRandom *random = ALLOCATE_OBJECT(vm, ObjectRandom, OBJECT_RANDOM);
	#ifdef _WIN32
    random->seed = (uint64_t)time(NULL) ^ GetTickCount() ^ (uint64_t)GetCurrentProcessId();
	#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    random->seed = tv.tv_sec ^ tv.tv_usec ^ (uint64_t)getpid();
	#endif
	return random;
}
