#include "debug.h"
#include "value.h"

#include <stdio.h>

#include "object.h"

void disassembleChunk(Chunk *chunk, const char *name) {
	printf("======= %s =======\n", name); // So we can tell which chunk we are looking at

	for (int offset = 0; offset < chunk->count;) {
		offset = disassembleInstruction(chunk, offset);
		// This changes the size of offset, because instructions can have different sizes
	}
}

static int simpleInstruction(const char *name, const int offset) {
	printf(" %s\n", name);
	return offset + 1;
}

static int byteInstruction(const char *name, Chunk *chunk, int offset) {
	uint8_t slot = chunk->code[offset + 1];
	printf("%-16s %4d\n", name, slot);
	return offset + 2;
}

static int jumpInstruction(const char *name, int sign, Chunk *chunk, int offset) {
	uint16_t jump = (uint16_t) (chunk->code[offset + 1] << 8);
	jump |= chunk->code[offset + 2];
	printf("%-16s %4d -> %d\n", name, offset, offset + 3 + sign * jump);
	return offset + 3;
}

static int constantInstruction(const char *name, Chunk *chunk, int offset) {
	uint8_t constant = chunk->code[offset + 1]; // Get the constant index
	printf("%-16s %4d '", name, constant); // Print the name of the opcode
	printValue(chunk->constants.values[constant]); // print the constant's value
	printf("'\n");
	return offset + 2; // +2 because OP_CONSTANT is two bytes
}

static int invokeInstruction(const char *name, Chunk *chunk, int offset) {
	uint8_t constant = chunk->code[offset + 1];
	uint8_t argCount = chunk->code[offset + 2];
	printf("%-16s (%d args) %4d '", name, argCount, constant);
	printValue(chunk->constants.values[constant]);
	printf("'\n");
	return offset + 3;
}

int disassembleInstruction(Chunk *chunk, int offset) {
	printf("%04d", offset); // Prints the byte offset of the given instruction

	if (offset > 0 && chunk->lines[offset] == chunk->lines[offset - 1]) {
		printf("   | ");
	} else {
		printf("%4d ", chunk->lines[offset]);
	}

	uint8_t instruction = chunk->code[offset]; // read a single byte, that is the opcode

	switch (instruction) {
		case OP_RETURN:
			return simpleInstruction("OP_RETURN", offset);
		case OP_CONSTANT:
			return constantInstruction("OP_CONSTANT", chunk, offset);
		case OP_NEGATE:
			return simpleInstruction("OP_NEGATE", offset);
		case OP_NIL:
			return simpleInstruction("OP_NIL", offset);
		case OP_TRUE:
			return simpleInstruction("OP_TRUE", offset);
		case OP_FALSE:
			return simpleInstruction("OP_FALSE", offset);
		case OP_ADD:
			return simpleInstruction("OP_ADD", offset);
		case OP_SUBTRACT:
			return simpleInstruction("OP_SUBTRACT", offset);
		case OP_MULTIPLY:
			return simpleInstruction("OP_MULTIPLY", offset);
		case OP_DIVIDE:
			return simpleInstruction("OP_DIVIDE", offset);
		case OP_NOT:
			return simpleInstruction("OP_NOT", offset);
		case OP_EQUAL:
			return simpleInstruction("OP_EQUAL", offset);
		case OP_GREATER:
			return simpleInstruction("OP_GREATER", offset);
		case OP_LESS:
			return simpleInstruction("OP_LESS", offset);
		case OP_LESS_EQUAL:
			return simpleInstruction("OP_LESS_EQUAL", offset);
		case OP_GREATER_EQUAL:
			return simpleInstruction("OP_GREATER_EQUAL", offset);
		case OP_MODULUS:
			return simpleInstruction("OP_MODULUS", offset);
		case OP_LEFT_SHIFT:
			return simpleInstruction("OP_LEFT_SHIFT", offset);
		case OP_RIGHT_SHIFT:
			return simpleInstruction("OP_RIGHT_SHIFT", offset);
		case OP_PRINT:
			return simpleInstruction("OP_PRINT", offset);
		case OP_POP:
			return simpleInstruction("OP_POP", offset);
		case OP_DEFINE_GLOBAL:
			return simpleInstruction("OP_DEFINE_GLOBAL", offset);
		case OP_GET_GLOBAL:
			return constantInstruction("OP_GET_GLOBAL", chunk, offset);
		case OP_SET_GLOBAL:
			return constantInstruction("OP_SET_GLOBAL", chunk, offset);
		case OP_GET_LOCAL:
			return byteInstruction("OP_GET_LOCAL", chunk, offset);
		case OP_SET_LOCAL:
			return byteInstruction("OP_SET_LOCAL", chunk, offset);
		case OP_JUMP:
			return jumpInstruction("OP_JUMP", 1, chunk, offset);
		case OP_JUMP_IF_FALSE:
			return jumpInstruction("OP_JUMP_IF_FALSE", 1, chunk, offset);
		case OP_LOOP:
			return jumpInstruction("OP_LOOP", -1, chunk, offset);
		case OP_CALL:
			return byteInstruction("OP_CALL", chunk, offset);
		case OP_GET_UPVALUE:
			return byteInstruction("OP_GET_UPVALUE", chunk, offset);
		case OP_SET_UPVALUE:
			return byteInstruction("OP_SET_UPVALUE", chunk, offset);
		case OP_CLOSURE: {
			offset++;
			uint8_t constant = chunk->code[offset++];
			printf("%-16s %4d ", "OP_CLOSURE", constant);
			printValue(chunk->constants.values[constant]);
			printf("\n");

			ObjectFunction *function = AS_STL_FUNCTION(chunk->constants.values[constant]);
			for (int j = 0; j < function->upvalueCount; j++) {
				int isLocal = chunk->code[offset++];
				int index = chunk->code[offset++];
				printf("%04d        |                %s %d\n", offset - 2, isLocal ? "local" : "upvalue", index);
			}
			return offset;
		}
		case OP_CLOSE_UPVALUE:
			return simpleInstruction("OP_CLOSE_UPVALUE", offset);
		case OP_CLASS: {
			return constantInstruction("OP_CLASS", chunk, offset);
		}
		case OP_GET_PROPERTY:
			return constantInstruction("OP_GET_PROPERTY", chunk, offset);
		case OP_SET_PROPERTY:
			return constantInstruction("OP_SET_PROPERTY", chunk, offset);
		case OP_METHOD:
			return constantInstruction("OP_METHOD", chunk, offset);
		case OP_ARRAY:
			return constantInstruction("OP_ARRAY", chunk, offset);
		case OP_TABLE:
			return constantInstruction("OP_TABLE", chunk, offset);
		case OP_GET_COLLECTION:
			return constantInstruction("OP_GET_COLLECTION", chunk, offset);
		case OP_SET_COLLECTION:
			return constantInstruction("OP_SET_COLLECTION", chunk, offset);
		case OP_INVOKE:
			return invokeInstruction("OP_INVOKE", chunk, offset);
		case OP_INHERIT:
			return simpleInstruction("OP_INHERIT", offset);
		case OP_GET_SUPER:
			return constantInstruction("OP_GET_SUPER", chunk, offset);
		case OP_SUPER_INVOKE:
			return invokeInstruction("OP_SUPER_INVOKE", chunk, offset);
		case OP_SET_LOCAL_SLASH: {
			return simpleInstruction("OP_SET_LOCAL_SLASH", offset);
		}
		case OP_SET_LOCAL_STAR: {
			return simpleInstruction("OP_SET_LOCAL_STAR", offset);
		}
		case OP_SET_LOCAL_PLUS: {
			return simpleInstruction("OP_SET_LOCAL_PLUS", offset);
		}
		case OP_SET_LOCAL_MINUS: {
			return simpleInstruction("OP_SET_LOCAL_MINUS", offset);
		}
		case OP_SET_UPVALUE_SLASH: {
			return simpleInstruction("OP_SET_UPVALUE_SLASH", offset);
		}
		case OP_SET_UPVALUE_STAR: {
			return simpleInstruction("OP_SET_UPVALUE_STAR", offset);
		}
		case OP_SET_UPVALUE_PLUS: {
			return simpleInstruction("OP_SET_UPVALUE_PLUS", offset);
		}
		case OP_SET_UPVALUE_MINUS: {
			return simpleInstruction("OP_SET_UPVALUE_MINUS", offset);
		}
		case OP_SET_GLOBAL_SLASH: {
			return simpleInstruction("OP_SET_GLOBAL_SLASH", offset);
		}
		case OP_SET_GLOBAL_STAR: {
			return simpleInstruction("OP_SET_GLOBAL_STAR", offset);
		}
		case OP_SET_GLOBAL_PLUS: {
			return simpleInstruction("OP_SET_GLOBAL_PLUS", offset);
		}
		case OP_SET_GLOBAL_MINUS: {
			return simpleInstruction("OP_SET_GLOBAL_MINUS", offset);
		}
		case OP_UNPACK_TUPLE: {
			return byteInstruction("OP_UNPACK_TUPLE", chunk, offset);
		}
		case OP_ANON_FUNCTION: {
			return constantInstruction("OP_ANON_FUNCTION", chunk, offset);
		}
		case OP_USE: {
			uint8_t nameCount = chunk->code[offset + 1];
			printf("%-16s %4d name(s) from ", "OP_USE", nameCount);
			printValue(chunk->constants.values[chunk->code[offset + nameCount + 2]]);
			printf("\n");
			return offset + nameCount + 3;
		}
		case OP_PUB: {
			return simpleInstruction("OP_PUB", offset);
		}
		default:
			printf("Unknown opcode %d\n", instruction);
			return offset + 1;
	}
}

bool verifyNumbers(Value a, Value b, const char *operation) {
	if (!IS_NUMBER(a) || !IS_NUMBER(b)) {
		printf("Arithmetic error in '%s':\n", operation);
		printf("Left operand: ");
		printValue(a);
		printf(" (type: %s)\n", IS_NUMBER(a)	 ? "number"
														: IS_NIL(a)		 ? "nil"
														: IS_BOOL(a)	 ? "boolean"
														: IS_STL_STRING(a) ? "string"
																					 : "other");

		printf("Right operand: ");
		printValue(b);
		printf(" (type: %s)\n", IS_NUMBER(b)	 ? "number"
														: IS_NIL(b)		 ? "nil"
														: IS_BOOL(b)	 ? "boolean"
														: IS_STL_STRING(b) ? "string"
																					 : "other");
		return false;
	}
	return true;
}
