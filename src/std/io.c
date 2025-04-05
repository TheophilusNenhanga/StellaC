#include "io.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../file_handler.h"
#include "../memory.h"
#include "../object.h"
#include "../vm.h"

#define MAX_LINE_LENGTH 4096


static FILE *getChannel(const char *channel) {
	if (strcmp(channel, "stdin") == 0)
		return stdin;
	if (strcmp(channel, "stdout") == 0)
		return stdout;
	if (strcmp(channel, "stderr") == 0)
		return stderr;
	return NULL;
}

void valuePrint(Value value);

void printArray(ObjectArray *array) {
	printf("[");
	for (int i = 0; i < array->size; i++) {
		valuePrint(array->array[i]);
		if (i != array->size - 1) {
			printf(", ");
		}
	}
	printf("]");
}

void printTable(ObjectTable *table) {
	uint16_t printed = 0;
	printf("{");
	for (int i = 0; i < table->capacity; i++) {
		if (table->entries[i].isOccupied) {
			valuePrint(table->entries[i].key);
			printf(":");
			valuePrint(table->entries[i].value);
			if (printed != table->size - 1) {
				printf(", ");
			}
			printed++;
		}
	}
	printf("}");
}

void printResult(ObjectResult* result) {
	if (result->isOk) {
			printf("Ok<");
			printType(result->as.value);
			printf(">");
	}
	else {
		printf("Err<");
		// TODO: Make this print the error's type
		printf("%s", result->as.error->message->chars);
		printf(">");
	}
}

void valuePrint(Value value) {
	if (IS_BOOL(value)) {
		printf(AS_BOOL(value) ? "true" : "false");
	} else if (IS_NIL(value)) {
		printf("nil");
	} else if (IS_INT(value)) {
		printf("%d", AS_INT(value));
	} else if (IS_FLOAT(value)) {
		printf("%f", AS_FLOAT(value));
	} else if (IS_CRUX_ARRAY(value)) {
		printArray(AS_CRUX_ARRAY(value));
	} else if (IS_CRUX_TABLE(value)) {
		printTable(AS_CRUX_TABLE(value));
	}else if (IS_CRUX_RESULT(value)) {
		printResult(AS_CRUX_RESULT(value));
	} else if (IS_CRUX_OBJECT(value)) {
		printObject(value);
	}
}

// Standard I/O Functions
Value printFunction(VM *vm, int argCount, Value *args) {
	valuePrint(args[0]);
	return  NIL_VAL;
}

Value printlnFunction(VM* vm, int argCount, Value* args) {
	printFunction(vm, argCount, args);
	printf("\n");
	return NIL_VAL;
}

ObjectResult* printToFunction(VM *vm, int argCount, Value *args) {

	if (!IS_CRUX_STRING(args[0]) || !IS_CRUX_STRING(args[1])) {
		return stellaErr(vm, newError(vm, copyString(vm, "Channel and content must be strings.", 36), TYPE, false));
	}

	const char *channel = AS_C_STRING(args[0]);
	const char *content = AS_C_STRING(args[1]);

	FILE *stream = getChannel(channel);
	if (stream == NULL) {
		return stellaErr(vm, newError(vm, copyString(vm, "Invalid channel specified.", 26), VALUE, false));
	}

	if (fprintf(stream, "%s", content) < 0) {
		return stellaErr(vm, newError(vm, copyString(vm, "Error writing to stream.", 26), IO, false));
	}

	return stellaOk(vm, BOOL_VAL(true));
}

ObjectResult* scanFunction(VM *vm, int argCount, Value *args) {

	int ch = getchar();
	if (ch == EOF) {
		return stellaErr(vm, newError(vm, copyString(vm, "Error reading from stdin.", 25), IO, false));
	}

	int overflow;
	while ((overflow = getchar()) != '\n' && overflow != EOF);

	char str[2] = {ch, '\0'};
	return stellaOk(vm, OBJECT_VAL(copyString(vm, str, 1)));
}

ObjectResult* scanlnFunction(VM *vm, int argCount, Value *args) {

	char buffer[1024];
	if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
		return stellaErr(vm, newError(vm, copyString(vm, "Error reading from stdin.", 26), IO, false));
	}

	size_t len = strlen(buffer);
	if (len > 0 && buffer[len - 1] == '\n') {
		buffer[len - 1] = '\0';
		len--;
	}

	return stellaOk(vm, OBJECT_VAL(copyString(vm, buffer, len)));
}

ObjectResult* scanFromFunction(VM *vm, int argCount, Value *args) {

	if (!IS_CRUX_STRING(args[0])) {
		return stellaErr(vm, newError(vm, copyString(vm, "Channel must be a string.", 25), TYPE, false));
	}

	const char *channel = AS_C_STRING(args[0]);
	FILE *stream = getChannel(channel);
	if (stream == NULL) {
		return stellaErr(vm, newError(vm, copyString(vm, "Invalid channel specified.", 26), VALUE, false));
	}

	int ch = fgetc(stream);
	if (ch == EOF) {
		return stellaErr(vm, newError(vm, copyString(vm, "Error reading from stream.", 26), IO, false));
	}

	int overflow;
	while ((overflow = fgetc(stream)) != '\n' && overflow != EOF);

	char str[2] = {ch, '\0'};
	return stellaOk(vm, OBJECT_VAL(copyString(vm, str, 1)));
}

ObjectResult* scanlnFromFunction(VM *vm, int argCount, Value *args) {

	if (!IS_CRUX_STRING(args[0])) {
		return stellaErr(vm, newError(vm, copyString(vm, "Channel must be a string.", 25), TYPE, false));
	}

	const char *channel = AS_C_STRING(args[0]);
	FILE *stream = getChannel(channel);
	if (stream == NULL) {
		return stellaErr(vm, newError(vm, copyString(vm, "Invalid channel specified.", 26), VALUE, false));
	}

	char buffer[1024];
	if (fgets(buffer, sizeof(buffer), stream) == NULL) {
		return stellaErr(vm, newError(vm, copyString(vm, "Error reading from stream.", 26), IO, false));
	}


	size_t len = strlen(buffer);
	// discard extra characters
	if (len > 0 && buffer[len - 1] != '\n') {
		int ch;
		while ((ch = fgetc(stream)) != '\n' && ch != EOF);
	}

	// Removing trailing newline character
	if (len > 0 && buffer[len - 1] == '\n') {
		buffer[len - 1] = '\0';
		len--;
	}

	return stellaOk(vm, OBJECT_VAL(copyString(vm, buffer, len)));
}

ObjectResult* nscanFunction(VM *vm, int argCount, Value *args) {

	if (!IS_INT(args[0])) {
		return stellaErr(vm, newError(vm, copyString(vm, "Number of characters must be a number.", 38), TYPE, false));
	}

	int32_t n = AS_INT(args[0]);
	if (n <= 0) {
		return stellaErr(vm, newError(vm, copyString(vm, "Number of characters must be positive.", 38), VALUE, false));
	}

	char *buffer = ALLOCATE(vm, char, n + 1);
	if (buffer == NULL) {
		return stellaErr(vm, newError(vm, copyString(vm, "Failed to allocate memory for input buffer.", 43), MEMORY, false));
	}

	size_t read = 0;
	while (read < n) {
		int ch = getchar();
		if (ch == EOF) {
			FREE_ARRAY(vm, char, buffer, n + 1);
			return stellaErr(vm, newError(vm, copyString(vm, "Error reading from stdin.", 25), IO, false));
		}
		buffer[read++] = ch;
		if (ch == '\n') {
			break;
		}
	}
	buffer[read] = '\0';

	if (read == n && buffer[read - 1] != '\n') {
		int ch;
		while ((ch = getchar()) != '\n' && ch != EOF);
	}

	Value string = OBJECT_VAL(copyString(vm, buffer, read));
	FREE_ARRAY(vm, char, buffer, n + 1);
	return stellaOk(vm, string);
}

ObjectResult* nscanFromFunction(VM *vm, int argCount, Value *args) {

	if (!IS_CRUX_STRING(args[0])) {
		return stellaErr(vm, newError(vm, copyString(vm, "Channel must be a string.", 25), TYPE, false));
	}

	if (!IS_INT(args[1])) {
		return stellaErr(vm, newError(vm, copyString(vm, "<char_count> must be of type 'int'.", 38), TYPE, false));
	}

	const char *channel = AS_C_STRING(args[0]);
	FILE *stream = getChannel(channel);
	if (stream == NULL) {
		return stellaErr(vm, newError(vm, copyString(vm, "Invalid channel specified.", 26), VALUE, false));
	}

	int32_t n = AS_INT(args[1]);
	if (n <= 0) {
		return stellaErr(vm, newError(vm, copyString(vm, "Number of characters must be positive.", 38), VALUE, false));
	}

	char *buffer = ALLOCATE(vm, char, n + 1);
	if (buffer == NULL) {
		return stellaErr(vm, newError(vm, copyString(vm, "Failed to allocate memory for input buffer.", 43), MEMORY, false));
	}

	size_t read = 0;
	while (read < n) {
		int ch = fgetc(stream);
		if (ch == EOF) {
			FREE_ARRAY(vm, char, buffer, n + 1);
			return stellaErr(vm, newError(vm, copyString(vm, "Error reading from stream.", 26), IO, false));
		}
		buffer[read++] = ch;
		if (ch == '\n') {
			break;
		}
	}
	buffer[read] = '\0';

	if (read == n && buffer[read - 1] != '\n') {
		int ch;
		while ((ch = fgetc(stream)) != '\n' && ch != EOF);
	}

	Value string =  OBJECT_VAL(copyString(vm, buffer, read));
	FREE_ARRAY(vm, char, buffer, n + 1);
	return stellaOk(vm, string);
}

ObjectResult* openFileFunction(VM *vm, int argCount, Value *args) {
	if (!IS_CRUX_STRING(args[0])) {
		return stellaErr(vm, newError(vm, copyString(vm, "<file_path> must be of type 'string'.", 37), IO, false));
	}

	if (!IS_CRUX_STRING(args[1])) {
		return stellaErr(vm, newError(vm, copyString(vm, "<file_mode> must be of type 'string'.", 37), IO, false));
	}

	ObjectString* path = AS_CRUX_STRING(args[0]);
	ObjectString* mode = AS_CRUX_STRING(args[1]);

	char* resolvedPath = resolvePath(vm->module->path->chars, path->chars);
	if (resolvedPath == NULL) {
		return stellaErr(vm, newError(vm, copyString(vm, "Could not resolve path to file.", 31), IO, false));
	}

	ObjectString* newPath = takeString(vm, resolvedPath, strlen(resolvedPath));

	ObjectFile* file = newObjectFile(vm, newPath, mode);
	if (file->file == NULL) {
		return stellaErr(vm, newError(vm, copyString(vm, "Failed to open file.", 20), IO, false));
	}

	return stellaOk(vm, OBJECT_VAL(file));
}

static bool isReadable(ObjectString* mode) {
	return strcmp(mode->chars, "r") == 0 || strcmp(mode->chars, "rb") == 0 || strcmp(mode->chars, "r+") == 0 || strcmp(mode->chars, "rb+") == 0
	|| strcmp(mode->chars, "a+") == 0 || strcmp(mode->chars, "ab+") == 0 || strcmp(mode->chars, "w+") == 0 || strcmp(mode->chars, "wb+") == 0;
}

static bool isWritable(ObjectString* mode) {
	return strcmp(mode->chars, "w") == 0 || strcmp(mode->chars, "wb") == 0 || strcmp(mode->chars, "w+") == 0 || strcmp(mode->chars, "wb+") == 0
	|| strcmp(mode->chars, "a") == 0 || strcmp(mode->chars, "ab") == 0 || strcmp(mode->chars, "a+") == 0 || strcmp(mode->chars, "ab+") == 0 
	|| strcmp(mode->chars, "r+") == 0 || strcmp(mode->chars, "rb+") == 0;
}

static bool isAppendable(ObjectString* mode) {
	return strcmp(mode->chars, "a") == 0 || strcmp(mode->chars, "ab") == 0 || strcmp(mode->chars, "a+") == 0 || strcmp(mode->chars, "ab+") == 0
	|| strcmp(mode->chars, "r+") == 0 || strcmp(mode->chars, "rb+") == 0 || strcmp(mode->chars, "w+") == 0 || strcmp(mode->chars, "wb+") == 0
	|| strcmp(mode->chars, "rb") == 0;
}

ObjectResult* readlnFileMethod(VM *vm, int argCount, Value *args) {
	ObjectFile* file = AS_CRUX_FILE(args[0]);
	if (file->file == NULL) {
		return stellaErr(vm, newError(vm, copyString(vm, "Could not read file.", 20), IO, false));
	}

	if (!file->isOpen) {
		return stellaErr(vm, newError(vm, copyString(vm, "File is not open.", 17), IO, false));
	}

	if (!isReadable(file->mode) && !isAppendable(file->mode)) {
		return stellaErr(vm, newError(vm, copyString(vm, "File is not readable.", 21), IO, false));
	}

	char* buffer = ALLOCATE(vm, char, MAX_LINE_LENGTH);
	if (buffer == NULL) {
		return stellaErr(vm, newError(vm, copyString(vm, "Failed to allocate memory for file content.", 43), MEMORY, false));
	}

	int readCount = 0;
	while (readCount < MAX_LINE_LENGTH) {
		int ch = fgetc(file->file);
		if (ch == EOF || ch == '\n') {
			break;
		}
		buffer[readCount++] = ch;
	}
	buffer[readCount] = '\0';
	file->position += readCount;
	return stellaOk(vm, OBJECT_VAL(takeString(vm, buffer, readCount)));
}

ObjectResult* readAllFileMethod(VM *vm, int argCount, Value *args) {
	ObjectFile* file = AS_CRUX_FILE(args[0]);
	if (file->file == NULL) {
		return stellaErr(vm, newError(vm, copyString(vm, "Could not read file.", 20), IO, false));
	}

	if (!file->isOpen) {
		return stellaErr(vm, newError(vm, copyString(vm, "File is not open.", 17), IO, false));
	}

	if (!isReadable(file->mode) && !isAppendable(file->mode)) {
		return stellaErr(vm, newError(vm, copyString(vm, "File is not readable.", 21), IO, false));
	}

	fseek(file->file, 0, SEEK_END);
	long fileSize = ftell(file->file);
	fseek(file->file, 0, SEEK_SET);

	char* buffer = ALLOCATE(vm, char, fileSize + 1);
	if (buffer == NULL) {
		return stellaErr(vm, newError(vm, copyString(vm, "Failed to allocate memory for file content.", 43), MEMORY, false));
	}

	fread(buffer, 1, fileSize, file->file);
	buffer[fileSize] = '\0';

	return stellaOk(vm, OBJECT_VAL(takeString(vm, buffer, fileSize)));
}

ObjectResult* closeFileMethod(VM *vm, int argCount, Value *args) {
	ObjectFile* file = AS_CRUX_FILE(args[0]);
	if (file->file == NULL) {
		return stellaErr(vm, newError(vm, copyString(vm, "Could not close file.", 21), IO, false));
	}

	if (!file->isOpen) {
		return stellaErr(vm, newError(vm, copyString(vm, "File is not open.", 17), IO, false));
	}

	fclose(file->file);	
	file->isOpen = false;
	file->position = 0;
	return stellaOk(vm, NIL_VAL);
}

ObjectResult* writeFileMethod(VM *vm, int argCount, Value *args) {
	ObjectFile* file = AS_CRUX_FILE(args[0]);
	
	if (file->file == NULL) {
		return stellaErr(vm, newError(vm, copyString(vm, "Could not write to file.", 21), IO, false));
	}

	if (!IS_CRUX_STRING(args[1])) {
		return stellaErr(vm, newError(vm, copyString(vm, "<content> must be of type 'string'.", 37), IO, false));
	}

	if (!file->isOpen) {
		return stellaErr(vm, newError(vm, copyString(vm, "File is not open.", 17), IO, false));
	}

	if (!isWritable(file->mode) && !isAppendable(file->mode)) {
		return stellaErr(vm, newError(vm, copyString(vm, "File is not writable.", 21), IO, false));
	}

	ObjectString* content = AS_CRUX_STRING(args[1]);

	fwrite(content->chars, sizeof(char), content->length, file->file);
	file->position += content->length;
	return stellaOk(vm, NIL_VAL);
}	

ObjectResult* writelnFileMethod(VM *vm, int argCount, Value *args) {
	ObjectFile* file = AS_CRUX_FILE(args[0]);
	
	if (file->file == NULL) {
		return stellaErr(vm, newError(vm, copyString(vm, "Could not write to file.", 21), IO, false));
	}

	if (!file->isOpen) {
		return stellaErr(vm, newError(vm, copyString(vm, "File is not open.", 17), IO, false));
	}

	if (!isWritable(file->mode) && !isAppendable(file->mode)) {
		return stellaErr(vm, newError(vm, copyString(vm, "File is not writable.", 21), IO, false));
	}
	
	if (!IS_CRUX_STRING(args[1])){
		return stellaErr(vm, newError(vm, copyString(vm, "<content> must be of type 'string'.", 37), IO, false));
	}

	ObjectString* content = AS_CRUX_STRING(args[1]);
	fwrite(content->chars, sizeof(char), content->length, file->file);
	fwrite("\n", sizeof(char), 1, file->file);
	file->position += content->length + 1;
	return stellaOk(vm, NIL_VAL);
}
