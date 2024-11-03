#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "chunk.h"
#include "common.h"
#include "debug.h"
#include "vm.h"

static char *readFile(const char *path) {
	FILE *file = fopen(path, "rb");

	if (file == NULL) {
		// If the file does not exist of the user does not have access to it
		fprintf(stderr, "Could not open file \"%s\".\n", path);
		exit(74);
	}

	fseek(file, 0L, SEEK_END); // go to the end of the file
	const size_t fileSize = ftell(file); // Tell us how far away the end is
	rewind(file); // bring us back to the beginning

	char *buffer = (char *) malloc(fileSize + 1);

	if (buffer == NULL) {
		// If the file does not exist of the user does not have access to it
		fprintf(stderr, "Not enough memory to read \"%s\".\n", path);
		exit(74);
	}

	const size_t bytesRead = fread(buffer, sizeof(char), fileSize, file);

	if (bytesRead < fileSize) {
		fprintf(stderr, "Could not read file \"%s\".\n", path);
	}

	buffer[bytesRead] = '\0';

	fclose(file);
	return buffer;
}

static void repl() {
	char line[1024];
	while (true) {
		printf("\n> ");

		if (!fgets(line, sizeof(line), stdin)) {
			break;
		}
		interpret(line);
	}
}

static void runFile(const char *path) {
	char *source = readFile(path);
	const InterpretResult result = interpret(source);
	free(source);

	if (result == INTERPRET_COMPILE_ERROR)
		exit(65);
	if (result == INTERPRET_RUNTIME_ERROR)
		exit(70);
}

int main(const int argc, const char *argv[]) {
	initVM();

	if (argc == 1) {
		// arg 0 is always the name of the executable being run
		repl();
	} else if (argc == 2) {
		runFile(argv[1]);
	} else {
		fprintf(stderr, "Usage: stella [path]\n");
		exit(64);
	}

	freeVM();
	return 0;
}
