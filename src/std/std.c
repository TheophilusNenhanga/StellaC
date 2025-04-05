#include "std.h"
#include "../memory.h"
#include "array.h"
#include "core.h"
#include "error.h"
#include "io.h"
#include "math.h"
#include "time.h"
#include "string.h"
#include "tables.h"
#include "random.h"


static Callable stringMethodsArray[] = {
	{"first", stringFirstMethod, 1},
	{"last", stringLastMethod, 1},
	{"get", stringGetMethod, 2},
	{"upper", stringUpperMethod, 1},
	{"lower", stringLowerMethod, 1},
	{"strip", stringStripMethod, 1},
	{"starts_with", stringStartsWithMethod, 2},
	{"ends_with", stringEndsWithMethod, 2},
	{"contains", stringContainsMethod, 2},
	{"replace", stringReplaceMethod, 3},
	{"split", stringSplitMethod, 2},
	{"substring", stringSubstringMethod, 3},
	{NULL, NULL, 0}
};

static Callable arrayMethodsArray[] = {
	{"pop", arrayPopMethod, 1},
	{"push", arrayPushMethod, 2},
	{"insert", arrayInsertMethod, 3},
	{"remove", arrayRemoveAtMethod, 2},
	{"concat", arrayConcatMethod, 2},
	{"slice", arraySliceMethod, 3},
	{"reverse", arrayReverseMethod, 1},
	{"index", arrayIndexOfMethod, 2},
	{NULL, NULL, 0}
};

static InfallibleCallable arrayInfallibleMethodsArray[] = {
	{"_contains", arrayContainsMethod, 2},
	{"_clear", arrayClearMethod, 1},
	{"_equals", arrayEqualsMethod, 2},
	{NULL, NULL, 0}
};

static Callable tableMethodsArray[] = {
	{"values", tableValuesMethod, 1},
	{"keys", tableKeysMethod, 1},
	{"pairs", tablePairsMethod, 1},
	{NULL, NULL, 0}
};

static Callable errorMethodsArray[] = {
	{"message", errorMessageMethod, 1},
	{"type", errorTypeMethod, 1},
	{NULL, NULL, 0}
};

static Callable randomMethodsArray[] = {
	{"seed", randomSeedMethod, 2},
	{"int", randomIntMethod, 3},
	{"double", randomDoubleMethod, 3},
	{"bool", randomBoolMethod, 2},
	{"choice", randomChoiceMethod, 2},
	{NULL, NULL, 0}
};

static InfallibleCallable randomInfallibleMethodsArray[] = {
	{"_next", randomNextMethod, 1},
	{NULL, NULL, 0}
};

static Callable fileMethodsArray[] = {
	{"readln", readlnFileMethod, 1},
	{"read_all", readAllFileMethod, 1},
	{"write", writeFileMethod, 2},
	{"writeln", writelnFileMethod, 2},
	{"close", closeFileMethod, 1},
	{NULL, NULL, 0}
};

static Callable coreFunctionsArray[] = {
	{"scanln", scanlnFunction, 0}, {"panic", panicFunction, 1}, {"len", lengthFunction, 1},
	{"error", errorFunction, 1}, {"assert", assertFunction, 2}, {"err", errorFunction, 1}, {"ok", okFunction, 1},
	{"int", intFunction, 1},
	{"float", floatFunction, 1},
	{"string", stringFunction, 1},
	{"table", tableFunction, 1},
	{"array", arrayFunction, 1},
	{NULL, NULL, 0}
};

static InfallibleCallable coreInfallibleFunctionsArray[] = {
	{"_len", lengthFunction_, 1},
	{"println", printlnFunction, 1},
	{"print", printFunction, 1},
	{"type", typeFunction_, 1},
	{"_int", intFunction_, 1},
	{"_float", floatFunction_, 1},
	{"_string", stringFunction_, 1},
	{"_table", tableFunction_, 1},
	{"_array", arrayFunction_, 1},
	{NULL, NULL, 0}
};


static Callable mathFunctionsArray[] = {
	{"pow", powFunction, 2}, 
	{"sqrt", sqrtFunction, 1}, 
	{"ceil", ceilFunction, 1}, 
	{"floor", floorFunction, 1},
	{"abs", absFunction, 1}, 
	{"sin", sinFunction, 1}, 
	{"cos", cosFunction, 1}, 
	{"tan", tanFunction, 1},
	{"atan", atanFunction, 1}, 
	{"acos", acosFunction, 1}, 
	{"asin", asinFunction, 1}, 
	{"exp", expFunction, 1},
	{"ln", lnFunction, 1}, 
	{"log", log10Function, 1}, 
	{"round", roundFunction, 1},
	{NULL, NULL, 0}
};

static InfallibleCallable mathInfallibleFunctionsArray[] = {
	{"_e", eFunction, 0},
	{"_pi", piFunction, 0},
	{NULL, NULL, 0}
};

static Callable ioFunctionsArray[] = {
	{"print_to", printToFunction, 2},
	{"scan", scanFunction, 0},
	{"scanln", scanlnFunction, 0},
	{"scan_from", scanFromFunction, 1},
	{"scanln_from", scanlnFromFunction, 1},
	{"nscan", nscanFunction, 1},
	{"nscan_from", nscanFromFunction, 2},
	{"open_file", openFileFunction, 2},
	{NULL, NULL, 0}
};

static Callable timeFunctionsArray[] = {
	{"sleep_s", sleepSecondsFunction, 1},
	{"sleep_ms", sleepMillisecondsFunction, 1},
	{NULL, NULL, 0}
};

static InfallibleCallable timeInfallibleFunctionsArray[] = {
	{"_time_s", timeSecondsFunction_, 0}, 
	{"_time_ms", timeMillisecondsFunction_, 0},
	{"_year", yearFunction_, 0}, 
	{"_month", monthFunction_, 0}, 
	{"_day", dayFunction_, 0}, 
	{"_hour", hourFunction_, 0},
	{"_minute", minuteFunction_, 0}, 
	{"_second", secondFunction_, 0}, 
	{"_weekday", weekdayFunction_, 0}, 
	{"_day_of_year", dayOfYearFunction_, 0},
	{NULL, NULL, 0}
};

static InfallibleCallable randomInfallibleFunctionsArray[] = {
	{"Random", randomInitFunction, 0},
	{NULL, NULL, 0}
};

bool registerNativeMethod(VM *vm, Table *methodTable, const char *methodName, 
						 StellaNativeCallable methodFunction, int arity) {
	ObjectString *name = copyString(vm, methodName, (int)strlen(methodName));
	if (!tableSet(vm, methodTable, name, 
		OBJECT_VAL(newNativeMethod(vm, methodFunction, arity, name)), false)) {
		return false;
	}
	return true;
}

bool registerNativeInfallibleMethod(VM *vm, Table *methodTable, const char *methodName,
								   StellaInfallibleCallable methodFunction, int arity) {
	ObjectString *name = copyString(vm, methodName, (int)strlen(methodName));
	if (!tableSet(vm, methodTable, name, 
		OBJECT_VAL(newNativeInfallibleMethod(vm, methodFunction, arity, name)), false)) {
		return false;
	}
	return true;
}

static bool registerMethods(VM *vm, Table *methodTable, Callable *methods) {
	for (int i = 0; methods[i].name != NULL; i++) {
		Callable method = methods[i];
		if (!registerNativeMethod(vm, methodTable, method.name, method.function, method.arity)) {
			return false;
		}
	}
	return true;
}

static bool registerInfallibleMethods(VM *vm, Table *methodTable, InfallibleCallable *methods) {
	for (int i = 0; methods[i].name != NULL; i++) {
		InfallibleCallable method = methods[i];
		if (!registerNativeInfallibleMethod(vm, methodTable, method.name, method.function, method.arity)) {
			return false;
		}
	}
	return true;
}

static bool registerNativeFunction(VM *vm, Table *functionTable, const char *functionName, 
								 StellaNativeCallable function, int arity) {
	ObjectString *name = copyString(vm, functionName, (int)strlen(functionName));
	if (!tableSet(vm, functionTable, name, 
		OBJECT_VAL(newNativeFunction(vm, function, arity, name)), false)) {
		return false;
	}
	return true;
}

static bool registerNativeInfallibleFunction(VM *vm, Table *functionTable, const char *functionName,
										   StellaInfallibleCallable function, int arity) {
	ObjectString *name = copyString(vm, functionName, (int)strlen(functionName));
	if (!tableSet(vm, functionTable, name, 
		OBJECT_VAL(newNativeInfallibleFunction(vm, function, arity, name)), false)) {
		return false;
	}
	return true;
}

static bool registerNativeFunctions(VM *vm, Table *functionTable, Callable *functions) {
	for (int i = 0; functions[i].name != NULL; i++) {
		Callable function = functions[i];
		if (!registerNativeFunction(vm, functionTable, function.name, function.function, function.arity)) {
			return false;
		}
	}
	return true;
}

static bool registerNativeInfallibleFunctions(VM *vm, Table *functionTable, 
											InfallibleCallable *functions) {
	for (int i = 0; functions[i].name != NULL; i++) {
		InfallibleCallable function = functions[i];
		if (!registerNativeInfallibleFunction(vm, functionTable, function.name, 
											function.function, function.arity)) {
			return false;
		}
	}
	return true;
}

static bool initModule(VM *vm, const char *moduleName, Callable *functions, 
					  InfallibleCallable *infallibles) {

	Table *moduleTable = ALLOCATE(vm, Table, 1);
	if (moduleTable == NULL) return false;
	
	initTable(moduleTable);

	if (functions != NULL) {
		if (!registerNativeFunctions(vm, moduleTable, functions)) {
			return false;
		}
	}

	if (infallibles != NULL) {
		if (!registerNativeInfallibleFunctions(vm, moduleTable, infallibles)) {
			return false;
		}
	}

	if (vm->nativeModules.count + 1 > vm->nativeModules.capacity) {
		int newCapacity = vm->nativeModules.capacity == 0 ? 8 : vm->nativeModules.capacity * 2;
		GROW_ARRAY(vm, NativeModule, vm->nativeModules.modules, 
				  vm->nativeModules.capacity, newCapacity);
		vm->nativeModules.capacity = newCapacity;
	}
	

	char *nameCopy = ALLOCATE(vm, char, strlen(moduleName) + 1);
	if (nameCopy == NULL) return false;
	strcpy(nameCopy, moduleName);
	
	// Add the module to the VM
	vm->nativeModules.modules[vm->nativeModules.count] = (NativeModule){
		.name = nameCopy,
		.names = moduleTable
	};
	vm->nativeModules.count++;
	
	return true;
}

static bool initTypeMethodTable(VM *vm, Table *methodTable, Callable *methods, 
							  InfallibleCallable *infallibleMethods) {
	if (methods != NULL) {
		if (!registerMethods(vm, methodTable, methods)) {
			return false;
		}
	}
	
	if (infallibleMethods != NULL) {
		if (!registerInfallibleMethods(vm, methodTable, infallibleMethods)) {
			return false;
		}
	}
	
	return true;
}

bool initializeStdLib(VM *vm) {
	if (!registerNativeFunctions(vm, &vm->globals, coreFunctionsArray)) {
		return false;
	}
	
	if (!registerNativeInfallibleFunctions(vm, &vm->globals, coreInfallibleFunctionsArray)) {
		return false;
	}
	

	if (!initTypeMethodTable(vm, &vm->stringType.methods, stringMethodsArray, NULL)) {
		return false;
	}
	
	if (!initTypeMethodTable(vm, &vm->arrayType.methods, arrayMethodsArray, arrayInfallibleMethodsArray)) {
		return false;
	}
	
	if (!initTypeMethodTable(vm, &vm->tableType.methods, tableMethodsArray, NULL)) {
		return false;
	}
	
	if (!initTypeMethodTable(vm, &vm->errorType.methods, errorMethodsArray, NULL)) {
		return false;
	}
	
	if (!initTypeMethodTable(vm, &vm->randomType.methods, randomMethodsArray, randomInfallibleMethodsArray)) {
		return false;
	}

	if (!initTypeMethodTable(vm, &vm->fileType.methods, fileMethodsArray, NULL)) {
		return false;
	}
	
	// Initialize standard library modules
	if (!initModule(vm, "math", mathFunctionsArray, mathInfallibleFunctionsArray)) {
		return false;
	}
	
	if (!initModule(vm, "io", ioFunctionsArray, NULL)) {
		return false;
	}
	
	if (!initModule(vm, "time", timeFunctionsArray, timeInfallibleFunctionsArray)) {
		return false;
	}
	
	if (!initModule(vm, "random", NULL, randomInfallibleFunctionsArray)) {
		return false;
	}
	
	return true;
}
