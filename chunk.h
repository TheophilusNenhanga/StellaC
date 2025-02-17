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
	OP_PRINT,
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
	OP_UNPACK_TUPLE,
	OP_USE,
	OP_PUB,
} OpCode;

typedef struct {
	int count;
	int capacity;
	uint8_t *code;
	int *lines;
	ValueArray constants;
} Chunk;

void initChunk(Chunk *chunk);

void writeChunk(VM *vm, Chunk *chunk, uint8_t byte, int line);

void freeChunk(VM *vm, Chunk *chunk);

int addConstant(VM *vm, Chunk *chunk, Value value);

#endif // CHUNK_H
