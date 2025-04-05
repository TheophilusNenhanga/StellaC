#include "file_handler.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief Extracts the directory name from a file path
 *
 * Parses a file path and returns the directory portion.
 * Handles platform-specific path separators and edge cases.
 *
 * @param path The file path to process
 * @return Dynamically allocated string containing the directory name (caller must free)
 */
static char *dirName(char *path) {
	if (path == NULL) {
		return NULL;
	}

	char *pathCopy = _strdup(path);
	if (pathCopy == NULL) {
		return NULL;
	}

	size_t pathLen = strlen(pathCopy);
	if (pathLen == 0) {
		free(pathCopy);
		return _strdup(".");
	}

	while (pathLen > 1 && (pathCopy[pathLen - 1] == '/' || pathCopy[pathLen - 1] == '\\')) {
		pathCopy[pathLen--] = '\0';
	}

	char *lastSlash = strrchr(pathCopy, '/');
#ifdef _WIN32
	char *lastBackslash = strrchr(pathCopy, '\\');
	if (lastBackslash > lastSlash) {
		lastSlash = lastBackslash;
	}
#endif
	if (lastSlash == NULL) {
		free(pathCopy);
		return _strdup(".");
	}

	if (lastSlash == pathCopy) {
#ifdef _WIN32
		char *result;
		if (pathCopy[1] == ':') {
			result = _strdup(pathCopy);
		} else {
			result = _strdup("\\");
		}
#else
		char *result = strdup("/");
#endif
		free(pathCopy);
		return result;
	}
#ifdef _WIN32
	if (lastSlash == pathCopy + 2 && pathCopy[1] == ':') {
		char *result = (char *) malloc(4);
		if (result == NULL) {
			free(pathCopy);
			return NULL;
		}
		result[0] = pathCopy[0];
		result[1] = ':';
		result[2] = '\\';
		result[3] = '\0';
		free(pathCopy);
		return result;
	}
#endif
	*lastSlash = '\0';
	char *result = _strdup(pathCopy);
	free(pathCopy);
	return result;
}

/**
 * @brief Gets the directory component from a path
 *
 * Creates a copy of the input path and uses dirName() to extract
 * the directory component. Handles memory management by making
 * a separate copy of the result.
 *
 * @param path The file path to process
 * @return Dynamically allocated string containing the directory (caller must free)
 */
static char *getDirectoryFromPath(const char *path) {
	if (path == NULL) {
		return NULL;
	}

	char *pathCopy = _strdup(path);
	if (pathCopy == NULL) {
		return NULL;
	}

	char *dir = dirName(pathCopy);
	if (dir == NULL) {
		free(pathCopy);
		return NULL;
	}

	char *result = _strdup(dir);
	free(pathCopy);
	free(dir);
	return result;
}

/**
 * @brief Combines a base path with a relative path
 *
 * Joins two path components together, handling path separators appropriately.
 * If the relative path is absolute, returns a copy of the relative path.
 *
 * @param base The base directory path
 * @param relative The relative path to append
 * @return Dynamically allocated string containing the combined path (caller must free)
 */
static char *combinePaths(const char *base, const char *relative) {
	if (base == NULL || relative == NULL)
		return NULL;

	if (relative[0] == '/'
#ifdef _WIN32
			|| (strlen(relative) > 2 && relative[1] == ':')
#endif
	) {
		return _strdup(relative);
	}

	size_t baseLen = strlen(base);
	size_t relativeLen = strlen(relative);
	size_t totalLen = baseLen + 1 + relativeLen + 1; // +1 : '/' +1 '\0'

	char *result = (char *) malloc(totalLen);
	if (result == NULL)
		return NULL;

	strcpy(result, base);

	if (baseLen > 0 && base[baseLen - 1] != '/' && base[baseLen - 1] != '\\') {
#ifdef _WIN32
		strcat(result, "\\");
#else
		strcat(result, "/");
#endif
	}

	strcat(result, relative);
	return result;
}

char *resolvePath(const char *basePath, const char *importPath) {
	if (importPath == NULL)
		return NULL;

	if (basePath == NULL || importPath[0] == '/'
#ifdef _WIN32
			|| (strlen(importPath) > 2 && importPath[1] == ':')
#endif
	) {
#ifdef _WIN32
		char *resolvedPath = (char *) malloc(MAX_PATH_LENGTH);
		if (_fullpath(resolvedPath, importPath, MAX_PATH_LENGTH) == NULL) {
			free(resolvedPath);
			return NULL;
		}
		return resolvedPath;
#else
		char resolvedPath[MAX_PATH_LENGTH];
		if (realpath(importPath, resolvedPath) == NULL) {
			return _strdup(importPath);
		}
		return _strdup(resolvedPath);
#endif
	}

	char *baseDir = getDirectoryFromPath(basePath);
	if (baseDir == NULL)
		return NULL;

	char *combinedPath = combinePaths(baseDir, importPath);
	free(baseDir);
	if (combinedPath == NULL)
		return NULL;

#ifdef _WIN32
	char *resolvedPath = (char *) malloc(MAX_PATH_LENGTH);
	if (_fullpath(resolvedPath, combinedPath, MAX_PATH_LENGTH) == NULL) {
		free(combinedPath);
		free(resolvedPath);
		return NULL;
	}
	free(combinedPath);
	return resolvedPath;
#else
	char resolvedPath[MAX_PATH_LENGTH];
	if (realpath(combinedPath, resolvedPath) == NULL) {
		free(combinedPath);
		return _strdup(combinedPath);
	}
	free(combinedPath);
	return strdup(resolvedPath);
#endif
}

FileResult readFile(const char *path) {
	FileResult result = {NULL, NULL};
	FILE *file = fopen(path, "rb");

	if (file == NULL) {
		size_t errorLen = strlen(path) + 32;
		result.error = (char *) malloc(errorLen);
		if (result.error != NULL) {
			snprintf(result.error, errorLen, "Could not open file \"%s\"", path);
		}
		return result;
	}

	fseek(file, 0L, SEEK_END);
	size_t fileSize = ftell(file);
	rewind(file);

	result.content = (char *) malloc(fileSize + 1);
	if (result.content == NULL) {
		result.error = _strdup("Not enough memory to read file");
		fclose(file);
		return result;
	}

	size_t bytesRead = fread(result.content, 1, fileSize, file);
	if (bytesRead < fileSize) {
		free(result.content);
		result.content = NULL;
		result.error = _strdup("Could not read file completely");
		fclose(file);
		return result;
	}

	result.content[bytesRead] = '\0';
	fclose(file);
	return result;
}

void freeFileResult(FileResult result) {
	free(result.content);
	free(result.error);
}
