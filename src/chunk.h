#ifndef CHUNK_H
#define CHUNK_H

#include "common.h"
#include "value.h"
#include "vm.h"

typedef enum {
	OP_RETURN,
	OP_CONSTANT,
	OP_NIL,
	OP_TRUE,
	OP_FALSE,
	OP_NEGATE,
	OP_EQUAL,
	OP_GREATER,
	OP_LESS,
	OP_LESS_EQUAL,
	OP_GREATER_EQUAL,
	OP_NOT_EQUAL,
	OP_ADD,
	OP_NOT,
	OP_SUBTRACT,
	OP_MULTIPLY,
	OP_DIVIDE,
	OP_POP,
	OP_DEFINE_GLOBAL,
	OP_GET_GLOBAL,
	OP_SET_GLOBAL,
	OP_GET_LOCAL,
	OP_SET_LOCAL,
	OP_JUMP_IF_FALSE,
	OP_JUMP,
	OP_LOOP,
	OP_CALL,
	OP_CLOSURE,
	OP_GET_UPVALUE,
	OP_SET_UPVALUE,
	OP_CLOSE_UPVALUE,
	OP_CLASS,
	OP_GET_PROPERTY,
	OP_SET_PROPERTY,
	OP_INVOKE,
	OP_METHOD,
	OP_INHERIT,
	OP_GET_SUPER,
	OP_SUPER_INVOKE,
	OP_ARRAY,
	OP_GET_COLLECTION,
	OP_SET_COLLECTION,
	OP_MODULUS,
	OP_LEFT_SHIFT,
	OP_RIGHT_SHIFT,
	OP_SET_LOCAL_SLASH,
	OP_SET_LOCAL_STAR,
	OP_SET_LOCAL_PLUS,
	OP_SET_LOCAL_MINUS,
	OP_SET_UPVALUE_SLASH,
	OP_SET_UPVALUE_STAR,
	OP_SET_UPVALUE_PLUS,
	OP_SET_UPVALUE_MINUS,
	OP_SET_GLOBAL_SLASH,
	OP_SET_GLOBAL_STAR,
	OP_SET_GLOBAL_PLUS,
	OP_SET_GLOBAL_MINUS,
	OP_TABLE,
	OP_ANON_FUNCTION,
	OP_USE,
	OP_PUB,
	OP_MATCH,
	OP_MATCH_JUMP,
	OP_MATCH_END,
	OP_RESULT_MATCH_OK,
	OP_RESULT_MATCH_ERR,
	OP_RESULT_BIND,
	OP_GIVE,
	OP_INT_DIVIDE,
	OP_POWER,
	OP_SET_GLOBAL_INT_DIVIDE,
	OP_SET_GLOBAL_MODULUS,
	OP_SET_LOCAL_INT_DIVIDE,
	OP_SET_LOCAL_MODULUS,
	OP_SET_UPVALUE_INT_DIVIDE,
	OP_SET_UPVALUE_MODULUS,
} OpCode;

typedef struct {
	int count;
	int capacity;
	uint8_t *code;
	int *lines;
	ValueArray constants;
} Chunk;

/**
 * @brief Initializes a new chunk structure
 *
 * Sets up an empty Chunk with null code and lines pointers,
 * zero capacity and count, and initializes the constants value array.
 *
 * @param chunk Pointer to the Chunk to initialize
 */
void initChunk(Chunk *chunk);

/**
 * @brief Adds a byte to a chunk, growing the chunk if needed
 *
 * Appends the given byte to the end of the chunk's code array
 * and records the corresponding source line number. If the chunk is at capacity,
 * it will be resized to accommodate the new byte.
 *
 * @param vm Pointer to the virtual machine (used for memory management)
 * @param chunk Pointer to the Chunk to modify
 * @param byte The byte to append to the chunk
 * @param line The source code line number corresponding to this byte
 */
void writeChunk(VM *vm, Chunk *chunk, uint8_t byte, int line);

/**
 * @brief Frees memory allocated for a chunk
 *
 * Deallocates the memory used by the chunk's code and lines arrays,
 * frees the constants value array, and resets the chunk to an initialized state.
 *
 * @param vm Pointer to the virtual machine (used for memory management)
 * @param chunk Pointer to the Chunk to free
 */
void freeChunk(VM *vm, Chunk *chunk);

/**
 * @brief Adds a constant value to a chunk's constant pool
 *
 * Temporarily pushes the value onto the VM stack for GC safety,
 * then adds the value to the chunk's constants array.
 * Returns the index where the constant was stored for later reference.
 *
 * @param vm Pointer to the virtual machine (used for memory management and GC protection)
 * @param chunk Pointer to the Chunk to modify
 * @param value The Value to add to the constant pool
 * @return The index of the added constant in the constants array
 */
int addConstant(VM *vm, Chunk *chunk, Value value);

#endif // CHUNK_H
