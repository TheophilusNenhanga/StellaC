#include "debug.h"
#include "value.h"

#include <stdio.h>

#include "object.h"

void disassembleChunk(Chunk *chunk, const char *name) {
	printf("======= %s =======\n", name);

	for (int offset = 0; offset < chunk->count;) {
		offset = disassembleInstruction(chunk, offset);
		// This changes the size of offset, because instructions can have different sizes
	}
}

/**
 * @brief Formats and prints a simple instruction (with no operands)
 *
 * Prints the instruction name and returns the offset of the next instruction.
 *
 * @param name The name of the instruction
 * @param offset The current byte offset in the chunk
 * @return The offset of the next instruction (current offset + 1)
 */
static int simpleInstruction(const char *name, const int offset) {
	printf(" %s\n", name);
	return offset + 1;
}

/**
 * @brief Formats and prints an instruction with a single byte operand
 *
 * Prints the instruction name and its byte operand, then returns
 * the offset of the next instruction.
 *
 * @param name The name of the instruction
 * @param chunk Pointer to the Chunk containing the instruction
 * @param offset The current byte offset in the chunk
 * @return The offset of the next instruction (current offset + 2)
 */
static int byteInstruction(const char *name, Chunk *chunk, int offset) {
	uint8_t slot = chunk->code[offset + 1];
	printf("%-16s %4d\n", name, slot);
	return offset + 2;
}

/**
 * @brief Formats and prints a jump instruction with a two-byte operand
 *
 * Decodes the two-byte jump offset, calculates and prints the target offset,
 * then returns the offset of the next instruction.
 *
 * @param name The name of the instruction
 * @param sign Direction of the jump (1 for forward, -1 for backward)
 * @param chunk Pointer to the Chunk containing the instruction
 * @param offset The current byte offset in the chunk
 * @return The offset of the next instruction (current offset + 3)
 */
static int jumpInstruction(const char *name, int sign, Chunk *chunk, int offset) {
	uint16_t jump = (uint16_t) (chunk->code[offset + 1] << 8);
	jump |= chunk->code[offset + 2];
	printf("%-16s %4d -> %d\n", name, offset, offset + 3 + sign * jump);
	return offset + 3;
}

/**
 * @brief Formats and prints an instruction with a constant operand
 *
 * Prints the instruction name, the constant index, and the constant's value,
 * then returns the offset of the next instruction.
 *
 * @param name The name of the instruction
 * @param chunk Pointer to the Chunk containing the instruction
 * @param offset The current byte offset in the chunk
 * @return The offset of the next instruction (current offset + 2)
 */
static int constantInstruction(const char *name, Chunk *chunk, int offset) {
	uint8_t constant = chunk->code[offset + 1]; // Get the constant index
	printf("%-16s %4d '", name, constant); // Print the name of the opcode
	printValue(chunk->constants.values[constant]); // print the constant's value
	printf("'\n");
	return offset + 2; // +2 because OP_CONSTANT is two bytes
}

/**
 * @brief Formats and prints a method invocation instruction
 *
 * Prints the instruction name, argument count, constant index, and the method name,
 * then returns the offset of the next instruction.
 *
 * @param name The name of the instruction
 * @param chunk Pointer to the Chunk containing the instruction
 * @param offset The current byte offset in the chunk
 * @return The offset of the next instruction (current offset + 3)
 */
static int invokeInstruction(const char *name, Chunk *chunk, int offset) {
	uint8_t constant = chunk->code[offset + 1];
	uint8_t argCount = chunk->code[offset + 2];
	printf("%-16s (%d args) %4d '", name, argCount, constant);
	printValue(chunk->constants.values[constant]);
	printf("'\n");
	return offset + 3;
}

// New helper functions that write to a file instead of stdout

/**
 * @brief Formats and writes a simple instruction to a file
 *
 * @param name The name of the instruction
 * @param offset The current byte offset in the chunk
 * @param file The file to write to
 * @return The offset of the next instruction (current offset + 1)
 */
static int fileSimpleInstruction(const char *name, const int offset, FILE *file) {
	fprintf(file, " %s\n", name);
	return offset + 1;
}

/**
 * @brief Formats and writes an instruction with a single byte operand to a file
 *
 * @param name The name of the instruction
 * @param chunk Pointer to the Chunk containing the instruction
 * @param offset The current byte offset in the chunk
 * @param file The file to write to
 * @return The offset of the next instruction (current offset + 2)
 */
static int fileByteInstruction(const char *name, Chunk *chunk, int offset, FILE *file) {
	uint8_t slot = chunk->code[offset + 1];
	fprintf(file, "%-16s %4d\n", name, slot);
	return offset + 2;
}

/**
 * @brief Formats and writes a jump instruction to a file
 *
 * @param name The name of the instruction
 * @param sign Direction of the jump (1 for forward, -1 for backward)
 * @param chunk Pointer to the Chunk containing the instruction
 * @param offset The current byte offset in the chunk
 * @param file The file to write to
 * @return The offset of the next instruction (current offset + 3)
 */
static int fileJumpInstruction(const char *name, int sign, Chunk *chunk, int offset, FILE *file) {
	uint16_t jump = (uint16_t) (chunk->code[offset + 1] << 8);
	jump |= chunk->code[offset + 2];
	fprintf(file, "%-16s %4d -> %d\n", name, offset, offset + 3 + sign * jump);
	return offset + 3;
}

/**
 * @brief Writes a value to a file
 *
 * Similar to printValue but writes to a file instead of stdout
 * 
 * @param value The value to write
 * @param file The file to write to
 */
static void fileWriteValue(Value value, FILE *file) {
	
		if (IS_BOOL(value)) {
			fprintf(file, AS_BOOL(value) ? "true" : "false");
		} else if (IS_NIL(value)) {
			fprintf(file, "nil");
		}else if (IS_INT(value)) {
			fprintf(file, "%d", AS_INT(value));
		} else if (IS_FLOAT(value)) {
			fprintf(file, "%g", AS_FLOAT(value));
		} else if (IS_CRUX_OBJECT(value)) {
			switch (OBJECT_TYPE(value)) {
				case OBJECT_STRING:
					fprintf(file, "%s", AS_C_STRING(value));
					break;
				case OBJECT_FUNCTION: {
					ObjectFunction *function = AS_CRUX_FUNCTION(value);
					if (function->name == NULL) {
						fprintf(file, "<script>");
					} else {
						fprintf(file, "<fn %s>", function->name->chars);
					}
					break;
				}
				case OBJECT_NATIVE_FUNCTION:
					fprintf(file, "<native fn>");
					break;
				case OBJECT_NATIVE_METHOD:
					fprintf(file, "<native method>");
					break;
				case OBJECT_NATIVE_INFALLIBLE_FUNCTION:
					fprintf(file, "<native infallible fn>");
					break;
				case OBJECT_NATIVE_INFALLIBLE_METHOD:
					fprintf(file, "<native infallible method>");
					break;
				case OBJECT_CLOSURE:
					fprintf(file, "<closure>");
					break;
				case OBJECT_UPVALUE:
					fprintf(file, "<upvalue>");
					break;
				case OBJECT_CLASS:
					fprintf(file, "<class>");
					break;
				case OBJECT_INSTANCE:
					fprintf(file, "<instance>");
					break;
				case OBJECT_BOUND_METHOD:
					fprintf(file, "<bound method>");
					break;
				case OBJECT_ARRAY:
					fprintf(file, "<array>");
					break;
				case OBJECT_TABLE:
					fprintf(file, "<table>");
					break;
				case OBJECT_ERROR:
					fprintf(file, "<error>");
					break;
				case OBJECT_MODULE:
					fprintf(file, "<module>");
					break;
				case OBJECT_RESULT:
					fprintf(file, "<result>");
					break;
				case OBJECT_RANDOM:
					fprintf(file, "<random>");
					break;
				case OBJECT_FILE:
					fprintf(file, "<file>");
					break;
				default:
				fprintf(file, "<unknown object>");
		}
	}
}

/**
 * @brief Formats and writes a constant instruction to a file
 *
 * @param name The name of the instruction
 * @param chunk Pointer to the Chunk containing the instruction
 * @param offset The current byte offset in the chunk
 * @param file The file to write to
 * @return The offset of the next instruction (current offset + 2)
 */
static int fileConstantInstruction(const char *name, Chunk *chunk, int offset, FILE *file) {
	uint8_t constant = chunk->code[offset + 1];
	fprintf(file, "%-16s %4d '", name, constant);
	fileWriteValue(chunk->constants.values[constant], file);
	fprintf(file, "'\n");
	return offset + 2;
}

/**
 * @brief Formats and writes an invoke instruction to a file
 *
 * @param name The name of the instruction
 * @param chunk Pointer to the Chunk containing the instruction
 * @param offset The current byte offset in the chunk
 * @param file The file to write to
 * @return The offset of the next instruction (current offset + 3)
 */
static int fileInvokeInstruction(const char *name, Chunk *chunk, int offset, FILE *file) {
	uint8_t constant = chunk->code[offset + 1];
	uint8_t argCount = chunk->code[offset + 2];
	fprintf(file, "%-16s (%d args) %4d '", name, argCount, constant);
	fileWriteValue(chunk->constants.values[constant], file);
	fprintf(file, "'\n");
	return offset + 3;
}

/**
 * @brief Disassembles and writes a single bytecode instruction to a file
 *
 * @param chunk Pointer to the Chunk containing the instruction
 * @param offset The byte offset of the instruction to disassemble
 * @param file The file to write to
 * @return The byte offset of the next instruction
 */
static int fileDisassembleInstruction(Chunk *chunk, int offset, FILE *file) {
	fprintf(file, "%04d", offset);

	if (offset > 0 && chunk->lines[offset] == chunk->lines[offset - 1]) {
		fprintf(file, "   | ");
	} else {
		fprintf(file, "%4d ", chunk->lines[offset]);
	}

	uint8_t instruction = chunk->code[offset];

	switch (instruction) {
		case OP_RETURN:
			return fileSimpleInstruction("OP_RETURN", offset, file);
		case OP_CONSTANT:
			return fileConstantInstruction("OP_CONSTANT", chunk, offset, file);
		case OP_NEGATE:
			return fileSimpleInstruction("OP_NEGATE", offset, file);
		case OP_NIL:
			return fileSimpleInstruction("OP_NIL", offset, file);
		case OP_TRUE:
			return fileSimpleInstruction("OP_TRUE", offset, file);
		case OP_FALSE:
			return fileSimpleInstruction("OP_FALSE", offset, file);
		case OP_ADD:
			return fileSimpleInstruction("OP_ADD", offset, file);
		case OP_SUBTRACT:
			return fileSimpleInstruction("OP_SUBTRACT", offset, file);
		case OP_MULTIPLY:
			return fileSimpleInstruction("OP_MULTIPLY", offset, file);
		case OP_DIVIDE:
			return fileSimpleInstruction("OP_DIVIDE", offset, file);
		case OP_NOT:
			return fileSimpleInstruction("OP_NOT", offset, file);
		case OP_EQUAL:
			return fileSimpleInstruction("OP_EQUAL", offset, file);
		case OP_GREATER:
			return fileSimpleInstruction("OP_GREATER", offset, file);
		case OP_LESS:
			return fileSimpleInstruction("OP_LESS", offset, file);
		case OP_LESS_EQUAL:
			return fileSimpleInstruction("OP_LESS_EQUAL", offset, file);
		case OP_GREATER_EQUAL:
			return fileSimpleInstruction("OP_GREATER_EQUAL", offset, file);
		case OP_MODULUS:
			return fileSimpleInstruction("OP_MODULUS", offset, file);
		case OP_LEFT_SHIFT:
			return fileSimpleInstruction("OP_LEFT_SHIFT", offset, file);
		case OP_RIGHT_SHIFT:
			return fileSimpleInstruction("OP_RIGHT_SHIFT", offset, file);
		case OP_POP:
			return fileSimpleInstruction("OP_POP", offset, file);
		case OP_DEFINE_GLOBAL:
			return fileConstantInstruction("OP_DEFINE_GLOBAL", chunk, offset, file);
		case OP_GET_GLOBAL:
			return fileConstantInstruction("OP_GET_GLOBAL", chunk, offset, file);
		case OP_SET_GLOBAL:
			return fileConstantInstruction("OP_SET_GLOBAL", chunk, offset, file);
		case OP_GET_LOCAL:
			return fileByteInstruction("OP_GET_LOCAL", chunk, offset, file);
		case OP_SET_LOCAL:
			return fileByteInstruction("OP_SET_LOCAL", chunk, offset, file);
		case OP_JUMP:
			return fileJumpInstruction("OP_JUMP", 1, chunk, offset, file);
		case OP_JUMP_IF_FALSE:
			return fileJumpInstruction("OP_JUMP_IF_FALSE", 1, chunk, offset, file);
		case OP_LOOP:
			return fileJumpInstruction("OP_LOOP", -1, chunk, offset, file);
		case OP_CALL:
			return fileByteInstruction("OP_CALL", chunk, offset, file);
		case OP_GET_UPVALUE:
			return fileByteInstruction("OP_GET_UPVALUE", chunk, offset, file);
		case OP_SET_UPVALUE:
			return fileByteInstruction("OP_SET_UPVALUE", chunk, offset, file);
		case OP_CLOSURE: {
			offset++;
			uint8_t constant = chunk->code[offset++];
			fprintf(file, "%-16s %4d ", "OP_CLOSURE", constant);
			fileWriteValue(chunk->constants.values[constant], file);
			fprintf(file, "\n");

			ObjectFunction *function = AS_CRUX_FUNCTION(chunk->constants.values[constant]);
			for (int j = 0; j < function->upvalueCount; j++) {
				int isLocal = chunk->code[offset++];
				int index = chunk->code[offset++];
				fprintf(file, "%04d        |                %s %d\n", offset - 2, isLocal ? "local" : "upvalue", index);
			}
			return offset;
		}
		case OP_CLOSE_UPVALUE:
			return fileSimpleInstruction("OP_CLOSE_UPVALUE", offset, file);
		case OP_CLASS: {
			return fileConstantInstruction("OP_CLASS", chunk, offset, file);
		}
		case OP_GET_PROPERTY:
			return fileConstantInstruction("OP_GET_PROPERTY", chunk, offset, file);
		case OP_SET_PROPERTY:
			return fileConstantInstruction("OP_SET_PROPERTY", chunk, offset, file);
		case OP_METHOD:
			return fileConstantInstruction("OP_METHOD", chunk, offset, file);
		case OP_ARRAY:
			return fileConstantInstruction("OP_ARRAY", chunk, offset, file);
		case OP_TABLE:
			return fileConstantInstruction("OP_TABLE", chunk, offset, file);
		case OP_GET_COLLECTION:
			return fileConstantInstruction("OP_GET_COLLECTION", chunk, offset, file);
		case OP_SET_COLLECTION:
			return fileConstantInstruction("OP_SET_COLLECTION", chunk, offset, file);
		case OP_INVOKE:
			return fileInvokeInstruction("OP_INVOKE", chunk, offset, file);
		case OP_INHERIT:
			return fileSimpleInstruction("OP_INHERIT", offset, file);
		case OP_GET_SUPER:
			return fileConstantInstruction("OP_GET_SUPER", chunk, offset, file);
		case OP_SUPER_INVOKE:
			return fileInvokeInstruction("OP_SUPER_INVOKE", chunk, offset, file);
		case OP_SET_LOCAL_SLASH:
			return fileSimpleInstruction("OP_SET_LOCAL_SLASH", offset, file);
		case OP_SET_LOCAL_STAR:
			return fileSimpleInstruction("OP_SET_LOCAL_STAR", offset, file);
		case OP_SET_LOCAL_PLUS:
			return fileSimpleInstruction("OP_SET_LOCAL_PLUS", offset, file);
		case OP_SET_LOCAL_MINUS:
			return fileSimpleInstruction("OP_SET_LOCAL_MINUS", offset, file);
		case OP_SET_UPVALUE_SLASH:
			return fileSimpleInstruction("OP_SET_UPVALUE_SLASH", offset, file);
		case OP_SET_UPVALUE_STAR:
			return fileSimpleInstruction("OP_SET_UPVALUE_STAR", offset, file);
		case OP_SET_UPVALUE_PLUS:
			return fileSimpleInstruction("OP_SET_UPVALUE_PLUS", offset, file);
		case OP_SET_UPVALUE_MINUS:
			return fileSimpleInstruction("OP_SET_UPVALUE_MINUS", offset, file);
		case OP_SET_GLOBAL_SLASH:
			return fileSimpleInstruction("OP_SET_GLOBAL_SLASH", offset, file);
		case OP_SET_GLOBAL_STAR:
			return fileSimpleInstruction("OP_SET_GLOBAL_STAR", offset, file);
		case OP_SET_GLOBAL_PLUS:
			return fileSimpleInstruction("OP_SET_GLOBAL_PLUS", offset, file);
		case OP_SET_GLOBAL_MINUS:
			return fileSimpleInstruction("OP_SET_GLOBAL_MINUS", offset, file);
		case OP_SET_GLOBAL_INT_DIVIDE:
			return fileSimpleInstruction("OP_SET_GLOBAL_INT_DIVIDE", offset, file);
		case OP_SET_GLOBAL_MODULUS:
			return fileSimpleInstruction("OP_SET_GLOBAL_MODULUS", offset, file);
		case OP_SET_LOCAL_INT_DIVIDE:
			return fileSimpleInstruction("OP_SET_LOCAL_INT_DIVIDE", offset, file);
		case OP_SET_LOCAL_MODULUS:
			return fileSimpleInstruction("OP_SET_LOCAL_MODULUS", offset, file);
		case OP_SET_UPVALUE_INT_DIVIDE:
			return fileSimpleInstruction("OP_SET_UPVALUE_INT_DIVIDE", offset, file);
		case OP_SET_UPVALUE_MODULUS:
			return fileSimpleInstruction("OP_SET_UPVALUE_MODULUS", offset, file);
		case OP_ANON_FUNCTION:
			return fileConstantInstruction("OP_ANON_FUNCTION", chunk, offset, file);
		case OP_USE: {
			uint8_t nameCount = chunk->code[offset + 1];
			fprintf(file, "%-16s %4d name(s) from ", "OP_USE", nameCount);
			fileWriteValue(chunk->constants.values[chunk->code[offset + nameCount + 2]], file);
			fprintf(file, "\n");
			return offset + nameCount + 3;
		}
		case OP_PUB:
			return fileSimpleInstruction("OP_PUB", offset, file);
		case OP_MATCH:
			return fileSimpleInstruction("OP_MATCH", offset, file);
		case OP_MATCH_JUMP:
			return fileJumpInstruction("OP_MATCH_JUMP", 1, chunk, offset, file);
		case OP_MATCH_END:
			return fileSimpleInstruction("OP_MATCH_END", offset, file);
		case OP_RESULT_MATCH_OK:
			return fileSimpleInstruction("OP_RESULT_MATCH_OK", offset, file);
		case OP_RESULT_MATCH_ERR:
			return fileSimpleInstruction("OP_RESULT_MATCH_ERR", offset, file);
		case OP_RESULT_BIND:
			return fileConstantInstruction("OP_RESULT_BIND", chunk, offset, file);
		case OP_GIVE:
			return fileSimpleInstruction("OP_GIVE", offset, file);
		case OP_INT_DIVIDE:
			return fileSimpleInstruction("OP_INT_DIVIDE", offset, file);
		case OP_POWER:
			return fileSimpleInstruction("OP_POWER", offset, file);
		default:
			fprintf(file, "Unknown opcode %d\n", instruction);
			return offset + 1;
	}
}

void dumpBytecodeToFile(Chunk *chunk, const char *name, FILE *file) {
	fprintf(file, "======= %s =======\n", name);

	for (int offset = 0; offset < chunk->count;) {
		offset = fileDisassembleInstruction(chunk, offset, file);
	}
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
		case OP_POP:
			return simpleInstruction("OP_POP", offset);
		case OP_DEFINE_GLOBAL:
			return constantInstruction("OP_DEFINE_GLOBAL", chunk, offset);
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

			ObjectFunction *function = AS_CRUX_FUNCTION(chunk->constants.values[constant]);
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
		case OP_SET_GLOBAL_INT_DIVIDE: {
			return simpleInstruction("OP_SET_GLOBAL_INT_DIVIDE", offset);
		}
		case OP_SET_GLOBAL_MODULUS: {
			return simpleInstruction("OP_SET_GLOBAL_MODULUS", offset);
		}
		case OP_SET_LOCAL_INT_DIVIDE: {
			return simpleInstruction("OP_SET_LOCAL_INT_DIVIDE", offset);
		}
		case OP_SET_LOCAL_MODULUS: {
			return simpleInstruction("OP_SET_LOCAL_MODULUS", offset);
		}
		case OP_SET_UPVALUE_INT_DIVIDE: {
			return simpleInstruction("OP_SET_UPVALUE_INT_DIVIDE", offset);
		}
		case OP_SET_UPVALUE_MODULUS: {
			return simpleInstruction("OP_SET_UPVALUE_MODULUS", offset);
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
		case OP_MATCH: {
			return simpleInstruction("OP_MATCH", offset);
		}
		case OP_MATCH_JUMP: {
			return jumpInstruction("OP_MATCH_JUMP", 1, chunk, offset);
		}
		case OP_MATCH_END: {
			return simpleInstruction("OP_MATCH_END", offset);
		}
		case OP_RESULT_MATCH_OK: {
			return simpleInstruction("OP_RESULT_MATCH_OK", offset);
		}
		case OP_RESULT_MATCH_ERR: {
			return simpleInstruction("OP_RESULT_MATCH_ERR", offset);
		}
		case OP_RESULT_BIND: {
			return constantInstruction("OP_RESULT_BIND", chunk, offset);
		}
		case OP_GIVE: {
			return simpleInstruction("OP_GIVE", offset);
		}
		case OP_INT_DIVIDE: {
			return simpleInstruction("OP_INT_DIVIDE", offset);
		}
		case OP_POWER: {
			return simpleInstruction("OP_POWER", offset);
		}
		default:
			printf("Unknown opcode %d\n", instruction);
			return offset + 1;
	}
}
