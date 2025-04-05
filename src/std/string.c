#include <stdlib.h>

#include "../memory.h"
#include "string.h"

static bool isUpper(char c) { return (c >= 'A' && c <= 'Z'); }
static bool isLower(char c) { return (c >= 'a' && c <= 'z'); }
static bool isSpace(char c) { return (c == ' ' || c == '\n' || c == '\t'); }

static char toUpper(char c) { return c + ('A' - 'a'); }

static char toLower(char c) { return c + ('a' - 'A'); }

void buildPrefixTable(const char *pattern, uint32_t patternLength, uint32_t *prefixTable) {
	uint32_t j = 0;
	prefixTable[0] = 0;

	for (int i = 1; i < patternLength; i++) {
		while (j > 0 && pattern[i] != pattern[j]) {
			j = prefixTable[j - 1];
		}
		if (pattern[i] == pattern[j]) {
			j++;
		}
		prefixTable[i] = j;
	}
}

ObjectResult* stringFirstMethod(VM *vm, int argCount, Value *args) {
	Value value = args[0];
	ObjectString *string = AS_CRUX_STRING(value);

	if (string->length == 0) {
		return stellaErr(vm,
				newError(vm, copyString(vm, "'string' must have at least one character to get the first character.", 69), VALUE,
								 false));
	}

	return stellaOk(vm, OBJECT_VAL(copyString(vm, &string->chars[0], 1)));
}

ObjectResult* stringLastMethod(VM *vm, int argCount, Value *args) {
	Value value = args[0];
	ObjectString *string = AS_CRUX_STRING(value);
	if (string->length == 0) {
		return stellaErr(vm, newError(
				vm, copyString(vm, "'string' must have at least one character to get the last character.", 68), VALUE, false));
	}
	return stellaOk(vm, OBJECT_VAL(copyString(vm, &string->chars[string->length - 1], 1)));

}

ObjectResult* stringGetMethod(VM *vm, int argCount, Value *args) {
	Value value = args[0];
	Value index = args[1];
	if (!IS_INT(index)) {
		return stellaErr(vm, newError(vm, copyString(vm, "<index> must be of type 'number'.", 33), TYPE, false));

	}
	ObjectString *string = AS_CRUX_STRING(value);
	if (AS_INT(index) < 0 || AS_INT(index) > (double) string->length) {
		return stellaErr(vm, newError(
				vm, copyString(vm, "<index> must be a non negative number that is less than the length of the string.", 81),
				INDEX_OUT_OF_BOUNDS, false));

	}

	if (string->length == 0) {
		return stellaErr(vm, newError(
				vm, copyString(vm, "'string' must have at least one character to get a character.", 61), VALUE, false));
	}

	return stellaOk(vm, OBJECT_VAL(copyString(vm, &string->chars[(uint32_t) AS_INT(index)], 1)));
}

ObjectResult* stringUpperMethod(VM *vm, int argCount, Value *args) {
	ObjectString *string = AS_CRUX_STRING(args[0]);
	if (string->length == 0) {
		return stellaErr(vm, newError(vm, copyString(vm, "Cannot make empty string uppercase.", 35), VALUE, false));
	}

	for (uint32_t i = 0; i < string->length; i++) {
		if (isLower(string->chars[i])) {
			string->chars[i] = toUpper(string->chars[i]);
		}
	}
	return stellaOk(vm, NIL_VAL);
}

ObjectResult* stringLowerMethod(VM *vm, int argCount, Value *args) {
	ObjectString *string = AS_CRUX_STRING(args[0]);

	if (string->length == 0) {
		return stellaErr(vm, newError(vm, copyString(vm, "Cannot make empty string lowercase.", 35), VALUE, false));
	}

	for (uint32_t i = 0; i < string->length; i++) {
		if (isUpper(string->chars[i])) {
			string->chars[i] = toLower(string->chars[i]);
		}
	}
	return stellaOk(vm, NIL_VAL);
}

ObjectResult* stringStripMethod(VM *vm, int argCount, Value *args) {
	ObjectString *string = AS_CRUX_STRING(args[0]);

	if (string->length == 0) {
		return stellaErr(vm, newError(vm, copyString(vm, "Cannot strip whitespace from an empty string.", 45), VALUE, false));
	}

	uint32_t start = 0;
	while (start < string->length && isSpace(string->chars[start])) {
		start++;
	}

	uint32_t end = string->length;
	while (end > start && isSpace(string->chars[end - 1])) {
		end--;
	}

	if (start == end) {
		return stellaOk(vm, OBJECT_VAL(copyString(vm, "", 0)));
	}

	return stellaOk(vm, OBJECT_VAL(copyString(vm, string->chars + start, end - start)));

}

ObjectResult* stringSubstringMethod(VM *vm, int argCount, Value *args) {
	ObjectString *string = AS_CRUX_STRING(args[0]);

	if (string->length == 0) {
		return stellaErr(vm, newError(vm, copyString(vm, "Cannot get substring of empty string.", 37), VALUE, false));

	}

	if (!IS_INT(args[1])) {
		return stellaErr(vm, newError(vm, copyString(vm, "<start> index must be of type 'int'.", 36), VALUE, false));

	}
	if (!IS_INT(args[2])) {
		return stellaErr(vm, newError(vm, copyString(vm, "<end> index must be of type 'int'.", 34), VALUE, false));
	}

	uint32_t startIndex = AS_INT(args[1]);
	uint32_t endIndex = AS_INT(args[2]);
	if (startIndex > string->length || endIndex > string->length || startIndex > endIndex) {
		return stellaErr(vm, newError(vm, copyString(vm, "Index out of bounds.", 20), INDEX_OUT_OF_BOUNDS, false));
	}

	char *substring = string->chars + startIndex;
	ObjectString *newSubstring = copyString(vm, substring, endIndex - startIndex);

	return stellaOk(vm, OBJECT_VAL(newSubstring));
}

ObjectResult* stringSplitMethod(VM *vm, int argCount, Value *args) {

	if (!IS_CRUX_STRING(args[0]) || !IS_CRUX_STRING(args[1])) {
		return stellaErr(vm, newError(vm, copyString(vm, "Both arguments must be strings.", 31), TYPE, false));

	}

	ObjectString *string = AS_CRUX_STRING(args[0]);
	ObjectString *delimiter = AS_CRUX_STRING(args[1]);

	if (string->length == 0 || delimiter->length == 0) {
		return stellaErr(vm, newError(vm,
														string->length == 0 ? copyString(vm, "Source string cannot be empty.", 30)
																								: copyString(vm, "Delimiter cannot be empty.", 26),
														VALUE, false));

	}

	if (delimiter->length > string->length) {
		ObjectArray *resultArray = newArray(vm, 1);
		arrayAdd(vm, resultArray, OBJECT_VAL(string), 0);
		return stellaOk(vm, OBJECT_VAL(resultArray));
	}

	uint32_t stringLength = string->length;
	uint32_t delimiterLength = delimiter->length;

	uint32_t *prefixTable = ALLOCATE(vm, uint32_t, delimiterLength);
	if (prefixTable == NULL) {
		return stellaErr(vm, newError(vm, copyString(vm, "Memory allocation failed.", 25), MEMORY, false));

	}

	buildPrefixTable(delimiter->chars, delimiterLength, prefixTable);

	// initial guess of splits size
	ObjectArray *resultArray = newArray(vm, stringLength / (delimiterLength + 1) + 1);

	uint32_t lastSplitIndex = 0;
	uint32_t j = 0;
	uint32_t lastAddIndex = 0;

	for (uint32_t i = 0; i < stringLength; i++) {
		while (j > 0 && string->chars[i] != delimiter->chars[j]) {
			j = prefixTable[j - 1];
		}

		if (string->chars[i] == delimiter->chars[j]) {
			j++;
		}

		if (j == delimiterLength) {
			uint32_t substringLength = i - lastSplitIndex - delimiterLength + 1;

			char *substringChars = ALLOCATE(vm, char, substringLength + 1);
			if (substringChars == NULL) {
				free(prefixTable);

				return stellaErr(vm, newError(vm, copyString(vm, "Memory allocation failed.", 25), MEMORY, false));

			}

			memcpy(substringChars, string->chars + lastSplitIndex, substringLength);
			substringChars[substringLength] = '\0';

			ObjectString *substring = copyString(vm, substringChars, substringLength);
			arrayAdd(vm, resultArray, OBJECT_VAL(substring), lastAddIndex);
			lastAddIndex++;

			lastSplitIndex = i + 1;

			j = 0;
		}
	}

	if (lastSplitIndex < stringLength) {
		uint32_t substringLength = stringLength - lastSplitIndex;
		char *substringChars = ALLOCATE(vm, char, substringLength + 1);

		if (substringChars == NULL) {
			free(prefixTable);
			return stellaErr(vm, newError(vm, copyString(vm, "Memory allocation failed.", 25), MEMORY, false));
		}

		memcpy(substringChars, string->chars + lastSplitIndex, substringLength);
		substringChars[substringLength] = '\0';

		ObjectString *substring = copyString(vm, substringChars, substringLength);
		arrayAdd(vm, resultArray, OBJECT_VAL(substring), lastAddIndex);
	}

	free(prefixTable);

	return stellaOk(vm, OBJECT_VAL(resultArray));

}

// KMP string-matching algorithm
ObjectResult* stringContainsMethod(VM *vm, int argCount, Value *args) {
	ObjectString *string = AS_CRUX_STRING(args[0]);

	if (!IS_CRUX_STRING(args[1])) {
		return stellaErr(vm, newError(vm, copyString(vm, "Argument 'goal' must be of type 'string'.", 41), TYPE, false));

	}
	ObjectString *goal = AS_CRUX_STRING(args[1]);

	if (string->length == 0 || goal->length == 0) {
		return stellaErr(vm, newError(vm,
														string->length == 0 ? copyString(vm, "String must have at least one character.", 40)
																								: copyString(vm, "<goal> string must have at least one character.", 47),
														VALUE, false));
	}

	if (goal->length > string->length) {
		return stellaOk(vm, BOOL_VAL(false));
	}

	uint32_t stringLength = string->length;
	uint32_t goalLength = goal->length;

	uint32_t *prefixTable = ALLOCATE(vm, uint32_t, goalLength);

	if (prefixTable == NULL) {
		return stellaErr(vm,
				newError(vm, copyString(vm, "Failed to allocate memory for string.contains().", 48), MEMORY, false));
	}

	buildPrefixTable(goal->chars, goalLength, prefixTable);

	uint32_t j = 0;
	for (uint32_t i = 0; i < stringLength; i++) {
		while (j > 0 && string->chars[i] != goal->chars[j]) {
			j = prefixTable[j - 1];
		}
		if (string->chars[i] == goal->chars[j]) {
			j++;
		}
		if (j == goalLength) {
			free(prefixTable);
			return stellaOk(vm, BOOL_VAL(true));
		}
	}

	free(prefixTable);
	return stellaOk(vm, BOOL_VAL(false));
}

ObjectResult* stringReplaceMethod(VM *vm, int argCount, Value *args) {

	if (!IS_CRUX_STRING(args[0]) || !IS_CRUX_STRING(args[1]) || !IS_CRUX_STRING(args[2])) {
		return stellaErr(vm, newError(vm, copyString(vm, "All arguments must be strings.", 30), TYPE, false));

	}

	ObjectString *string = AS_CRUX_STRING(args[0]);
	ObjectString *goal = AS_CRUX_STRING(args[1]);
	ObjectString *replacement = AS_CRUX_STRING(args[2]);

	if (string->length == 0 || goal->length == 0) {
		return stellaErr(vm,
				newError(vm,
								 string->length == 0 ? copyString(vm, "Source string must have at least one character.", 47)
																		 : copyString(vm, "<target> substring must have at least one character.", 52),
								 VALUE, false));

	}

	if (goal->length > string->length) {
		return stellaOk(vm, OBJECT_VAL(string));

	}

	uint32_t stringLength = string->length;
	uint32_t goalLength = goal->length;
	uint32_t replacementLength = replacement->length;

	uint32_t *prefixTable = ALLOCATE(vm, uint32_t, goalLength);
	if (prefixTable == NULL) {
		return stellaErr(vm, newError(vm, copyString(vm, "Memory allocation failed.", 25), MEMORY, false));

	}

	buildPrefixTable(goal->chars, goalLength, prefixTable);

	uint32_t matchCount = 0;
	uint32_t *matchIndices = ALLOCATE(vm, uint32_t, stringLength);
	if (matchIndices == NULL) {
		free(prefixTable);
		return stellaErr(vm, newError(vm, copyString(vm, "Memory allocation failed.", 25), MEMORY, false));

	}

	uint32_t j = 0;
	for (uint32_t i = 0; i < stringLength; i++) {
		while (j > 0 && string->chars[i] != goal->chars[j]) {
			j = prefixTable[j - 1];
		}
		if (string->chars[i] == goal->chars[j]) {
			j++;
		}
		if (j == goalLength) {
			matchIndices[matchCount++] = i - goalLength + 1;
			j = 0;
		}
	}

	if (matchCount == 0) {
		free(prefixTable);
		free(matchIndices);
		return stellaOk(vm, OBJECT_VAL(string));
	}

	uint32_t newStringLength = stringLength + (replacementLength - goalLength) * matchCount;

	char *newStringChars = ALLOCATE(vm, char, newStringLength + 1);
	if (newStringChars == NULL) {
		free(prefixTable);
		free(matchIndices);
		return stellaErr(vm, newError(vm, copyString(vm, "Memory allocation failed.", 25), MEMORY, false));

	}

	uint32_t writeIndex = 0;
	uint32_t lastCopyIndex = 0;
	for (uint32_t i = 0; i < matchCount; i++) {
		uint32_t copyLength = matchIndices[i] - lastCopyIndex;
		memcpy(newStringChars + writeIndex, string->chars + lastCopyIndex, copyLength);
		writeIndex += copyLength;

		memcpy(newStringChars + writeIndex, replacement->chars, replacementLength);
		writeIndex += replacementLength;

		lastCopyIndex = matchIndices[i] + goalLength;
	}

	uint32_t remainingLength = stringLength - lastCopyIndex;
	memcpy(newStringChars + writeIndex, string->chars + lastCopyIndex, remainingLength);
	writeIndex += remainingLength;

	newStringChars[newStringLength] = '\0';

	ObjectString *newString = copyString(vm, newStringChars, newStringLength);
	free(prefixTable);
	free(matchIndices);

	return stellaOk(vm, OBJECT_VAL(newString));
}

ObjectResult* stringStartsWithMethod(VM *vm, int argCount, Value *args) {
	ObjectString *string = AS_CRUX_STRING(args[0]);
	if (!IS_CRUX_STRING(args[1])) {
		return stellaErr(vm, newError(vm, copyString(vm, "First argument <char> must be of type 'string'.", 47), TYPE, false));

	}
	ObjectString *goal = AS_CRUX_STRING(args[1]);

	if (string->length == 0 || goal->length == 0) {
		return stellaErr(vm, newError(vm,
														string->length == 0 ? copyString(vm, "String must have at least one character.", 40)
																								: copyString(vm, "<goal> string must have at least one character.", 47),
														VALUE, false));
	}

	if (goal->length > string->length) {
		return stellaOk(vm, BOOL_VAL(false));
	}

	for (uint32_t i = 0; i < goal->length; i++) {
		if (string->chars[i] != goal->chars[i]) {
			return stellaOk(vm, BOOL_VAL(false));
		}
	}
	return stellaOk(vm, BOOL_VAL(true));
}

ObjectResult* stringEndsWithMethod(VM *vm, int argCount, Value *args) {
	ObjectString *string = AS_CRUX_STRING(args[0]);
	if (!IS_CRUX_STRING(args[1])) {
		return stellaErr(vm, newError(vm, copyString(vm, "First argument must be of type 'string'.", 40), TYPE, false));

	}
	ObjectString *goal = AS_CRUX_STRING(args[1]);

	if (string->length == 0 || goal->length == 0) {
		return stellaErr(vm, newError(vm,
														string->length == 0 ? copyString(vm, "String must have at least one character.", 40)
																								: copyString(vm, "<goal> string must have at least one character.", 47),
														VALUE, false));
	}
	if (goal->length > string->length) {
		return stellaOk(vm, BOOL_VAL(false));
	}

	uint32_t stringStart = string->length - goal->length;
	for (uint32_t i = 0; i < goal->length; i++) {
		if (string->chars[stringStart + i] != goal->chars[i]) {
			return stellaOk(vm, BOOL_VAL(false));
		}
	}
	return stellaOk(vm, BOOL_VAL(true));
}
