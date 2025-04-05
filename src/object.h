#ifndef OBJECT_H
#define OBJECT_H

#include <stdio.h>

#include "chunk.h"
#include "common.h"
#include "table.h"
#include "value.h"

#define OBJECT_TYPE(value) (AS_CRUX_OBJECT(value)->type)

#define IS_CRUX_STRING(value) isObjectType(value, OBJECT_STRING)
#define IS_CRUX_FUNCTION(value) isObjectType(value, OBJECT_FUNCTION)
#define IS_CRUX_NATIVE_FUNCTION(value) isObjectType(value, OBJECT_NATIVE_FUNCTION)
#define IS_CRUX_NATIVE_METHOD(value) isObjectType(value, OBJECT_NATIVE_METHOD)
#define IS_CRUX_CLOSURE(value) isObjectType(value, OBJECT_CLOSURE)
#define IS_CRUX_CLASS(value) isObjectType(value, OBJECT_CLASS)
#define IS_CRUX_INSTANCE(value) isObjectType(value, OBJECT_INSTANCE)
#define IS_CRUX_BOUND_METHOD(value) isObjectType(value, OBJECT_BOUND_METHOD)
#define IS_CRUX_ARRAY(value) isObjectType(value, OBJECT_ARRAY)
#define IS_CRUX_TABLE(value) isObjectType(value, OBJECT_TABLE)
#define IS_CRUX_ERROR(value) isObjectType(value, OBJECT_ERROR)
#define IS_CRUX_RESULT(value) isObjectType(value, OBJECT_RESULT)
#define IS_CRUX_NATIVE_INFALLIBLE_FUNCTION(value) isObjectType(value, OBJECT_NATIVE_INFALLIBLE_FUNCTION)
#define IS_CRUX_NATIVE_INFALLIBLE_METHOD(value) isObjectType(value, OBJECT_NATIVE_INFALLIBLE_METHOD)
#define IS_CRUX_RANDOM(value) isObjectType(value, OBJECT_RANDOM)
#define IS_CRUX_FILE(value) isObjectType(value, OBJECT_FILE)

#define AS_CRUX_STRING(value) ((ObjectString *) AS_CRUX_OBJECT(value))
#define AS_C_STRING(value) (((ObjectString *) AS_CRUX_OBJECT(value))->chars)
#define AS_CRUX_FUNCTION(value) ((ObjectFunction *) AS_CRUX_OBJECT(value))
#define AS_CRUX_NATIVE_FUNCTION(value) ((ObjectNativeFunction *) AS_CRUX_OBJECT(value))
#define AS_CRUX_NATIVE_METHOD(value) ((ObjectNativeMethod *) AS_CRUX_OBJECT(value))
#define AS_CRUX_CLOSURE(value) ((ObjectClosure *) AS_CRUX_OBJECT(value))
#define AS_CRUX_CLASS(value) ((ObjectClass *) AS_CRUX_OBJECT(value))
#define AS_CRUX_INSTANCE(value) ((ObjectInstance *) AS_CRUX_OBJECT(value))
#define AS_CRUX_BOUND_METHOD(value) ((ObjectBoundMethod *) AS_CRUX_OBJECT(value))
#define AS_CRUX_ARRAY(value) ((ObjectArray *) AS_CRUX_OBJECT(value))
#define AS_CRUX_TABLE(value) ((ObjectTable *) AS_CRUX_OBJECT(value))
#define AS_CRUX_ERROR(value) ((ObjectError *) AS_CRUX_OBJECT(value))
#define AS_CRUX_RESULT(value) ((ObjectResult *) AS_CRUX_OBJECT(value))
#define AS_CRUX_NATIVE_INFALLIBLE_FUNCTION(value) ((ObjectNativeInfallibleFunction *) AS_CRUX_OBJECT(value))
#define AS_CRUX_NATIVE_INFALLIBLE_METHOD(value) ((ObjectNativeInfallibleMethod *) AS_CRUX_OBJECT(value))
#define AS_CRUX_RANDOM(value) ((ObjectRandom *) AS_CRUX_OBJECT(value))
#define AS_CRUX_FILE(value) ((ObjectFile *) AS_CRUX_OBJECT(value))

typedef enum {
	OBJECT_STRING,
	OBJECT_FUNCTION,
	OBJECT_NATIVE_FUNCTION,
	OBJECT_NATIVE_METHOD,
	OBJECT_CLOSURE,
	OBJECT_UPVALUE,
	OBJECT_CLASS,
	OBJECT_INSTANCE,
	OBJECT_BOUND_METHOD,
	OBJECT_ARRAY,
	OBJECT_TABLE,
	OBJECT_ERROR,
	OBJECT_MODULE,
	OBJECT_RESULT,
	OBJECT_NATIVE_INFALLIBLE_FUNCTION,
	OBJECT_NATIVE_INFALLIBLE_METHOD,
	OBJECT_RANDOM,
	OBJECT_FILE,
} ObjectType;

struct Object {
	ObjectType type;
	Object *next;
	bool isMarked;
};

struct ObjectString {
	Object Object;
	uint32_t length;
	char *chars;
	uint32_t hash;
};

typedef struct {
	Object object;
	int arity;
	int upvalueCount;
	Chunk chunk;
	ObjectString *name;
} ObjectFunction;

typedef struct ObjectUpvalue {
	Object object;
	Value *location;
	Value closed;
	struct ObjectUpvalue *next;
} ObjectUpvalue;

typedef struct ObjectClosure {
	Object object;
	ObjectFunction *function;
	ObjectUpvalue **upvalues;
	int upvalueCount;
} ObjectClosure;

typedef struct {
	Object object;
	ObjectString *name;
	Table methods;
} ObjectClass;

typedef struct {
	Object object;
	ObjectClass *klass;
	Table fields;
} ObjectInstance;

typedef struct {
	Object object;
	Value receiver;
	ObjectClosure *method;
} ObjectBoundMethod;

typedef struct {
	Object object;
	Value *array;
	uint32_t size;
	uint32_t capacity;
} ObjectArray;

typedef struct {
	Object object;
	uint64_t seed;
} ObjectRandom;

typedef enum {
	SYNTAX,
	DIVISION_BY_ZERO,
	INDEX_OUT_OF_BOUNDS,
	RUNTIME,
	TYPE,
	LOOP_EXTENT,
	LIMIT,
	BRANCH_EXTENT,
	CLOSURE_EXTENT,
	LOCAL_EXTENT,
	ARGUMENT_EXTENT,
	NAME,
	COLLECTION_EXTENT,
	VARIABLE_EXTENT,
	VARIABLE_DECLARATION_MISMATCH,
	RETURN_EXTENT,
	ARGUMENT_MISMATCH,
	STACK_OVERFLOW,
	COLLECTION_GET,
	COLLECTION_SET,
	UNPACK_MISMATCH,
	MEMORY,
	VALUE, // correct type, but incorrect value
	ASSERT,
	IMPORT_EXTENT,
	IO,
	IMPORT,
} ErrorType;

typedef struct {
	Object object;
	ObjectString *message;
	ErrorType type;
	bool isPanic;
} ObjectError;

typedef struct {
	Object object;
	bool isOk;
	union {
		Value value;
		ObjectError* error;
	} as;
} ObjectResult;

typedef ObjectResult* (*StellaNativeCallable)(VM *vm, int argCount, Value *args);
typedef Value (*StellaInfallibleCallable)(VM *vm, int argCount, Value *args);

typedef struct {
	Object object;
	StellaNativeCallable function;
	ObjectString *name;
	int arity;
} ObjectNativeFunction;

typedef struct {
	Object object;
	StellaNativeCallable function;
	ObjectString *name;
	int arity;
} ObjectNativeMethod;

typedef struct {
	Object object;
	StellaInfallibleCallable function;
	ObjectString *name;
	int arity;
} ObjectNativeInfallibleFunction;

typedef struct {
	Object object;
	StellaInfallibleCallable function;
	ObjectString *name;
	int arity;
} ObjectNativeInfallibleMethod;

typedef struct {
	Value key;
	Value value;
	bool isOccupied;
} ObjectTableEntry;

typedef struct {
	ObjectString **paths; // module paths
	int count;
	int capacity;
} ImportSet;

typedef struct {
	Object object;
	ObjectTableEntry *entries;
	uint16_t capacity;
	uint16_t size;
} ObjectTable;

typedef enum {
	IMPORTED, 
	IN_PROGRESS,
	INITIAL,
} ModuleState;

struct ObjectModule{
	Object object;
	ObjectString *path;
	ImportSet importedModules;
	ModuleState state;
	int vmDepth;
};

typedef struct {
	Object object;
	ObjectString *path;
	ObjectString *mode;
	FILE *file;
	uint64_t position;
	bool isOpen;
} ObjectFile;


static bool isObjectType(Value value, ObjectType type) { return IS_CRUX_OBJECT(value) && AS_CRUX_OBJECT(value)->type == type; }

/**
 * @brief Creates a new error object.
 *
 * This function allocates and initializes a new ObjectError, representing
 * an error in the Stella language.
 *
 * @param vm The virtual machine.
 * @param message The error message as an ObjectString.
 * @param type The ErrorType enum value representing the type of error.
 * @param isPanic A boolean indicating whether this error represents a panic (fatal error).
 *
 * @return A pointer to the newly created ObjectError.
 */
ObjectError *newError(VM *vm, ObjectString *message, ErrorType type, bool isPanic);

/**
 * @brief Creates a new bound method object.
 *
 * A bound method combines a receiver object (the instance) with a method (closure)
 * to be called on that receiver. This function allocates and initializes a new
 * ObjectBoundMethod.
 *
 * @param vm The virtual machine.
 * @param receiver The receiver Value (the instance).
 * @param method The method ObjectClosure.
 *
 * @return A pointer to the newly created ObjectBoundMethod.
 */
ObjectBoundMethod *newBoundMethod(VM *vm, Value receiver, ObjectClosure *method);

/**
 * @brief Creates a new upvalue object.
 *
 * Upvalues are used to implement closures, capturing variables from enclosing scopes.
 * This function allocates and initializes a new ObjectUpvalue.
 *
 * @param vm The virtual machine.
 * @param slot A pointer to the stack slot where the upvalue captures a variable.
 *
 * @return A pointer to the newly created ObjectUpvalue.
 */
ObjectUpvalue *newUpvalue(VM *vm, Value *slot);

/**
 * @brief Creates a new closure object.
 *
 * Closures are function objects that capture their surrounding environment.
 * This function allocates and initializes a new ObjectClosure, linking it
 * to its underlying ObjectFunction and allocating space for upvalues.
 *
 * @param vm The virtual machine.
 * @param function The ObjectFunction to be wrapped in the closure.
 *
 * @return A pointer to the newly created ObjectClosure.
 */
ObjectClosure *newClosure(VM *vm, ObjectFunction *function);

/**
 * @brief Creates a new native function object.
 *
 * Native functions are implemented in C and callable from Stella code.
 * This function allocates and initializes a new ObjectNativeFunction.
 *
 * @param vm The virtual machine.
 * @param function The C function pointer representing the native function's implementation.
 * @param arity The expected number of arguments for the native function.
 * @param name The name of the native function as an ObjectString (for debugging).
 *
 * @return A pointer to the newly created ObjectNativeFunction.
 */
ObjectNativeFunction *newNativeFunction(VM *vm, StellaNativeCallable function, int arity, ObjectString *name);

/**
 * @brief Creates a new native method object.
 *
 * Native methods are similar to native functions but are associated with classes.
 * This function allocates and initializes a new ObjectNativeMethod.
 *
 * @param vm The virtual machine.
 * @param function The C function pointer representing the native method's implementation.
 * @param arity The expected number of arguments for the native method.
 * @param name The name of the native method as an ObjectString (for debugging).
 *
 * @return A pointer to the newly created ObjectNativeMethod.
 */
ObjectNativeMethod *newNativeMethod(VM *vm, StellaNativeCallable function, int arity, ObjectString *name);

/**
 * @brief Creates a new native infallible function object.
 *
 * Native infallible functions are implemented in C, callable from Stella code,
 * and guaranteed not to return errors. This function allocates and initializes 
 * a new ObjectNativeInfallibleFunction.
 *
 * @param vm The virtual machine.
 * @param function The C function pointer representing the native function's implementation.
 * @param arity The expected number of arguments for the native function.
 * @param name The name of the native function as an ObjectString (for debugging).
 *
 * @return A pointer to the newly created ObjectNativeInfallibleFunction.
 */
ObjectNativeInfallibleFunction *newNativeInfallibleFunction(VM *vm, StellaInfallibleCallable function, int arity, ObjectString *name);

/**
 * @brief Creates a new native infallible method object.
 *
 * Native infallible methods are similar to native infallible functions but are associated with classes.
 * This function allocates and initializes a new ObjectNativeInfallibleMethod.
 */
ObjectNativeInfallibleMethod *newNativeInfallibleMethod(VM *vm, StellaInfallibleCallable function, int arity, ObjectString *name);

/**
 * @brief Creates a new function object.
 *
 * This function allocates and initializes a new ObjectFunction, representing
 * a function in the Stella language. It sets default values for arity, name,
 * and upvalue count, and initializes the function's chunk.
 *
 * @param vm The virtual machine.
 *
 * @return A pointer to the newly created ObjectFunction.
 */
ObjectFunction *newFunction(VM *vm);

/**
 * @brief Creates a new class object.
 *
 * This function allocates and initializes a new ObjectClass, representing a class
 * in the Stella language. It initializes the class's method table and sets its name.
 *
 * @param vm The virtual machine.
 * @param name The name of the class as an ObjectString.
 *
 * @return A pointer to the newly created ObjectClass.
 */
ObjectClass *newClass(VM *vm, ObjectString *name);

/**
 * @brief Creates a new instance object.
 *
 * This function allocates and initializes a new ObjectInstance, representing
 * an instance of a class in the Stella language. It associates the instance
 * with its class and initializes its field table.
 *
 * @param vm The virtual machine.
 * @param klass The ObjectClass of which this is an instance.
 *
 * @return A pointer to the newly created ObjectInstance.
 */
ObjectInstance *newInstance(VM *vm, ObjectClass *klass);

/**
 * @brief Creates a new module object.
 *
 * Modules encapsulate code and data for separate compilation and organization.
 * This function allocates and initializes a new ObjectModule.
 *
 * @param vm The virtual machine.
 * @param path The module path as a C-style string.
 *
 * @return A pointer to the newly created ObjectModule.
 */
ObjectModule *newModule(VM *vm, const char* path);

/**
 * @brief Creates a new object table.
 *
 * Object tables are hash tables used to store key-value pairs. This function
 * allocates and initializes a new ObjectTable with a given initial capacity.
 *
 * @param vm The virtual machine.
 * @param elementCount Hint for initial table size. The capacity will be the next power of 2 greater than or equal to `elementCount` (or 16 if `elementCount` is less than 16).
 *
 * @return A pointer to the newly created ObjectTable.
 */
ObjectTable *newTable(VM *vm, int elementCount);

/**
 * @brief Creates a new ok result with a boxed value.
 *
 * This function creates a new ObjectResult representing a successful operation,
 * boxing a given Value within it.
 *
 * @param vm the virtual machine (for garbage collection purposes)
 * @param value The Value to box in the result.
 *
 * @return A pointer to the result with the value.
 */
ObjectResult* stellaOk(VM *vm, Value value);

/**
 * @brief Creates a new error result with a boxed error.
 *
 * This function creates a new ObjectResult representing a failed operation,
 * boxing a given ObjectError within it.
 *
 * @param vm the virtual machine (for garbage collection purposes)
 * @param error The error to box in the result.
 *
 * @return A pointer to the result with the error.
 */
ObjectResult* stellaErr(VM *vm, ObjectError *error);

/**
 * @brief Creates a new array object.
 *
 * This function allocates and initializes a new ObjectArray, representing
 * a dynamic array in the Stella language. It allocates memory for the array
 * elements and sets initial capacity and size.
 *
 * @param vm The virtual machine.
 * @param elementCount Hint for initial array size. The capacity will be the next power of 2 greater than or equal to `elementCount`.
 *
 * @return A pointer to the newly created ObjectArray.
 */
ObjectArray *newArray(VM *vm, uint32_t elementCount);

/**
 * @brief Creates a new string object and takes ownership of the given character buffer.
 *
 * This function creates a new ObjectString, taking ownership of the provided
 * character buffer. This means the function is responsible for freeing the
 * memory pointed to by `chars` when the string object is no longer needed.
 * It also interns the string. If an identical string already exists in the string table,
 * the provided `chars` buffer is freed, and the existing interned string is returned.
 *
 * @param vm The Stella Virtual Machine.
 * @param chars The character buffer to be owned by the ObjectString. This buffer will be freed if the string is interned or when the ObjectString is garbage collected.
 * @param length The length of the string.
 *
 * @return A pointer to the interned ObjectString.
 */
ObjectString *takeString(VM *vm, char *chars, uint32_t length);

/**
 * @brief Creates a new string object by copying a C-style string.
 *
 * This function creates a copy of a given C-style string on the heap and
 * wraps it in an ObjectString. It also interns the string to ensure that
 * identical strings share the same object in memory.
 *
 * @param vm The virtual machine.
 * @param chars The null-terminated C-style string to copy.
 * @param length The length of the string (excluding null terminator).
 *
 * @return A pointer to the interned ObjectString.
 */
ObjectString *copyString(VM *vm, const char *chars, uint32_t length);

/**
 * @brief Converts a Value to its string representation.
 *
 * This function converts a Value to its string representation, returning an
 * ObjectString. It handles different value types, including numbers, booleans,
 * nil, and various object types, creating a string representation suitable
 * for display or string operations. For composite objects like arrays and tables,
 * it recursively converts their elements/entries to strings.
 *
 * @param vm The virtual machine.
 * @param value The Value to convert to a string.
 *
 * @return An ObjectString representing the Value.
 */
ObjectString *toString(VM *vm, Value value);

/**
 * @brief Prints a Value to the console for debugging purposes.
 *
 * This function provides a human-readable representation of a Value, dispatching
 * to different printing logic based on the Value's type. It handles strings,
 * functions, native functions, closures, upvalues, classes, instances, bound methods, arrays, tables and errors.
 *
 * @param value The Value to print.
 */
void printObject(Value value);

void printType(Value value);

/**
 * @brief Frees the memory associated with an object table.
 *
 * This function frees the memory allocated for the entries array of an ObjectTable.
 * It is called during garbage collection when the table is no longer reachable.
 *
 * @param vm The virtual machine.
 * @param table The ObjectTable to free.
 */
void freeObjectTable(VM *vm, ObjectTable *table);

/**
 * @brief Sets a key-value pair in an object table.
 *
 * This function inserts or updates a key-value pair in an ObjectTable. It
 * resizes the table if necessary to maintain performance and handles key
 * collisions using open addressing.
 *
 * @param vm The virtual machine.
 * @param table The ObjectTable to modify.
 * @param key The key Value.
 * @param value The value Value.
 *
 * @return true if the set operation was successful, false otherwise (e.g., memory allocation failure during resizing).
 */
bool objectTableSet(VM *vm, ObjectTable *table, Value key, Value value);

/**
 * @brief Retrieves a value from an object table by key.
 *
 * This function looks up a value in an ObjectTable based on a given key.
 *
 * @param table The ObjectTable to search.
 * @param key The key Value to look up.
 * @param value A pointer to a Value where the retrieved value will be stored if the key is found.
 *
 * @return true if the key was found and the value was retrieved, false otherwise.
 */
bool objectTableGet(ObjectTable *table, Value key, Value *value);
void markObjectTable(VM *vm, ObjectTable *table);

/**
 * @brief Ensures that an array has enough capacity.
 *
 * This function checks if an ObjectArray has enough capacity to hold a
 * required number of elements. If not, it resizes the array, doubling its
 * capacity until it meets the requirement.
 *
 * @param vm The virtual machine.
 * @param array The ObjectArray to check and resize.
 * @param capacityNeeded The minimum capacity required.
 *
 * @return true if capacity is ensured (either already sufficient or resizing successful), false if resizing failed (e.g., memory allocation failure).
 */
bool ensureCapacity(VM *vm, ObjectArray *array, uint32_t capacityNeeded);

/**
 * @brief Sets a value at a specific index in an array.
 *
 * This function sets a Value at a given index in an ObjectArray. It performs
 * bounds checking to ensure the index is within the array's current size.
 *
 * @param vm The virtual machine.
 * @param array The ObjectArray to modify.
 * @param index The index at which to set the value.
 * @param value The Value to set.
 *
 * @return true if the value was set successfully (index within bounds), false otherwise (index out of bounds).
 */
bool arraySet(VM *vm, ObjectArray *array, uint32_t index, Value value);

/**
 * @brief Retrieves a value from an array at a specific index.
 *
 * This function retrieves a Value from an ObjectArray at a given index.
 * It does not perform bounds checking.
 *
 * @param array The ObjectArray to access.
 * @param index The index from which to retrieve the value.
 * @param value A pointer to a Value where the retrieved value will be stored.
 *
 * @return true if the array is not NULL and the value was retrieved, false otherwise (array is NULL).
 */
bool arrayGet(ObjectArray *array, uint32_t index, Value *value);

/**
 * @brief Adds a value to the end of an array.
 *
 * This function appends a Value to the end of an ObjectArray, increasing its size.
 * It ensures sufficient capacity before adding the element.
 *
 * @param vm The virtual machine.
 * @param array The ObjectArray to modify.
 * @param value The Value to add.
 * @param index The index at which to add the value, this should be `array->size`.
 *
 * @return true if the value was added successfully, false otherwise (e.g., memory allocation failure during resizing).
 */
bool arrayAdd(VM *vm, ObjectArray *array, Value value, uint32_t index);

/**
 * @brief Adds a value to the end of an array.
 *
 * This function appends a Value to the end of an ObjectArray, increasing its size.
 * It ensures sufficient capacity before adding the element.
 */
bool arrayAddBack(VM *vm, ObjectArray *array, Value value);

/**
 * @brief Checks if an import set contains a path.
 *
 * This function checks if a given module path is already present in an ImportSet.
 *
 * @param set The ImportSet to search.
 * @param path The ObjectString representing the module path to check for.
 *
 * @return true if the path is in the set, false otherwise.
 */
bool importSetContains(ImportSet *set, ObjectString *path);

/**
 * @brief Adds a path to an import set.
 *
 * This function adds a module path to an ImportSet, tracking imported modules.
 * It resizes the set's path array if necessary.
 *
 * @param vm The virtual machine.
 * @param set The ImportSet to modify.
 * @param path The ObjectString representing the module path to add.
 *
 * @return true if the path was added successfully, false otherwise (e.g., memory allocation failure during resizing).
 */
bool importSetAdd(VM *vm, ImportSet *set, ObjectString *path);

/**
 * @brief Initializes an import set.
 *
 * An import set tracks modules that have been imported to prevent cyclic imports.
 * This function initializes an ImportSet struct.
 *
 * @param set The ImportSet to initialize.
 */
void initImportSet(ImportSet *set);

/**
 * @brief Frees the memory associated with an import set.
 *
 * This function frees the memory allocated for the path array in an ImportSet.
 *
 * @param vm The virtual machine.
 * @param set The ImportSet to free.
 */
void freeImportSet(VM *vm, ImportSet *set);

/**
 * @brief Creates a new random object.
 *
 * This function allocates and initializes a new ObjectRandom, representing
 * a random number generator in the Stella language.
 *
 * @param vm The virtual machine.
 *
 * @return A pointer to the newly created ObjectRandom.
 */
ObjectRandom *newRandom(VM *vm);

ObjectFile* newObjectFile(VM *vm, ObjectString *path, ObjectString *mode);

#endif
