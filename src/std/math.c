#include "math.h"
#include <math.h>

static bool numberArgs(Value *args, int argCount) {
	for (int i = 0; i < argCount; i++) {
		if (!IS_INT(args[i]) || IS_FLOAT(args[i])) {
			return false;
		}
	}
	return true;
}

ObjectResult *powFunction(VM *vm, int argCount, Value *args) {
	if (!numberArgs(args, argCount)) {
		return stellaErr(vm, newError(vm, copyString(vm, "Both arguments must be of type 'number'.", 40), TYPE, false));
	}

	double base = AS_FLOAT(args[0]);
	double exponent = AS_FLOAT(args[1]);

	return stellaOk(vm, FLOAT_VAL(pow(base, exponent)));
}

ObjectResult *sqrtFunction(VM *vm, int argCount, Value *args) {
	if (!numberArgs(args, argCount)) {
		return stellaErr(vm, newError(vm, copyString(vm, "Argument must be of type 'number'.", 34), TYPE, false));
	}
	double number = AS_FLOAT(args[0]);
	if (number < 0) {
		return stellaErr(vm, newError(vm,
		                              copyString(vm, "Cannot calculate square root of a negative number.", 50),
		                              VALUE,
		                              false));
	}

	return stellaOk(vm, FLOAT_VAL(sqrt(number)));
}

ObjectResult *absFunction(VM *vm, int argCount, Value *args) {
	if (!numberArgs(args, argCount)) {
		return stellaErr(vm, newError(vm, copyString(vm, "Argument must be of type 'number'.", 34), TYPE, false));
	}
	if (IS_INT(args[0])) {
		return stellaOk(vm, INT_VAL(abs(AS_INT(args[0]))));
	}
	return stellaOk(vm, FLOAT_VAL(fabs(AS_FLOAT(args[0]))));
}

ObjectResult *sinFunction(VM *vm, int argCount, Value *args) {
	if (!numberArgs(args, argCount)) {
		return stellaErr(vm, newError(vm, copyString(vm, "Argument must be of type 'number'.", 34), TYPE, false));
	}
	return stellaOk(vm, FLOAT_VAL(sin(FLOAT_VAL(args[0]))));
}

ObjectResult *cosFunction(VM *vm, int argCount, Value *args) {
	if (!numberArgs(args, argCount)) {
		return stellaErr(vm, newError(vm, copyString(vm, "Argument must be of type 'number'.", 34), TYPE, false));
	}
	return stellaOk(vm, FLOAT_VAL(cos(AS_FLOAT(args[0]))));
}

ObjectResult *tanFunction(VM *vm, int argCount, Value *args) {
	if (!numberArgs(args, argCount)) {
		return stellaErr(vm, newError(vm, copyString(vm, "Argument must be of type 'number'.", 34), TYPE, false));
	}
	return stellaOk(vm, FLOAT_VAL(tan(AS_FLOAT(args[0]))));
}

ObjectResult *asinFunction(VM *vm, int argCount, Value *args) {
	if (!numberArgs(args, argCount)) {
		return stellaErr(vm, newError(vm, copyString(vm, "Argument must be of type 'number'.", 34), TYPE, false));
	}

	double num = AS_FLOAT(args[0]);
	if (num < -1 || num > 1) {
		return stellaErr(vm, newError(vm,
		                              copyString(vm, "Argument must be between -1 and 1.", 34),
		                              VALUE,
		                              false));
	}

	return stellaOk(vm, FLOAT_VAL(asin(num)));
}

ObjectResult *acosFunction(VM *vm, int argCount, Value *args) {
	if (!numberArgs(args, argCount)) {
		return stellaErr(vm, newError(vm, copyString(vm, "Argument must be of type 'number'.", 34), TYPE, false));
	}
	double num = AS_FLOAT(args[0]);
	if (num < -1 || num > 1) {
		return stellaErr(vm, newError(vm,
		                              copyString(vm, "Argument must be between -1 and 1.", 34),
		                              VALUE,
		                              false));
	}
	return stellaOk(vm, FLOAT_VAL(acos(num)));
}

ObjectResult *atanFunction(VM *vm, int argCount, Value *args) {
	if (!numberArgs(args, argCount)) {
		return stellaErr(vm, newError(vm, copyString(vm, "Argument must be of type 'number'.", 34), TYPE, false));
	}
	return stellaOk(vm, FLOAT_VAL(atan(AS_FLOAT(args[0]))));
}

ObjectResult *expFunction(VM *vm, int argCount, Value *args) {
	if (!numberArgs(args, argCount)) {
		return stellaErr(vm, newError(vm, copyString(vm, "Argument must be of type 'number'.", 34), TYPE, false));
	}
	return stellaOk(vm, FLOAT_VAL(exp(AS_FLOAT(args[0]))));
}

ObjectResult *lnFunction(VM *vm, int argCount, Value *args) {
	if (!numberArgs(args, argCount)) {
		return stellaErr(vm, newError(vm, copyString(vm, "Argument must be of type 'number'.", 34), TYPE, false));
	}

	double number = AS_FLOAT(args[0]);
	if (number < 0) {
		return stellaErr(vm, newError(vm, copyString(vm, "Cannot calculate natural logarithm of non positive number.", 58),
		                              VALUE, false));
	}
	return stellaOk(vm, FLOAT_VAL(log(AS_FLOAT(args[0]))));
}

ObjectResult *log10Function(VM *vm, int argCount, Value *args) {
	if (!numberArgs(args, argCount)) {
		return stellaErr(vm, newError(vm, copyString(vm, "Argument must be of type 'number'.", 34), TYPE, false));
	}

	double number = AS_FLOAT(args[0]);
	if (number < 0) {
		return stellaErr(vm, newError(vm, copyString(vm, "Cannot calculate base 10 logarithm of non positive number.", 58),
		                              VALUE, false));
	}

	return stellaOk(vm, FLOAT_VAL(log10(AS_FLOAT(args[0]))));
}

ObjectResult *ceilFunction(VM *vm, int argCount, Value *args) {
	if (!numberArgs(args, argCount)) {
		return stellaErr(vm, newError(vm, copyString(vm, "Argument must be of type 'number'.", 34), TYPE, false));
	}
	return stellaOk(vm, FLOAT_VAL(ceil(AS_FLOAT(args[0]))));
}

ObjectResult *floorFunction(VM *vm, int argCount, Value *args) {
	if (!numberArgs(args, argCount)) {
		return stellaErr(vm, newError(vm, copyString(vm, "Argument must be of type 'number'.", 34), TYPE, false));
	}
	return stellaOk(vm, FLOAT_VAL(floor(AS_FLOAT(args[0]))));
}

ObjectResult *roundFunction(VM *vm, int argCount, Value *args) {
	if (!numberArgs(args, argCount)) {
		return stellaErr(vm, newError(vm, copyString(vm, "Argument must be of type 'number'.", 34), TYPE, false));
	}
	return stellaOk(vm, FLOAT_VAL(round(AS_FLOAT(args[0]))));
}

Value piFunction(VM *vm, int argCount, Value *args) {
	return FLOAT_VAL(3.14159265358979323846);
}

Value eFunction(VM *vm, int argCount, Value *args) {
	return FLOAT_VAL(2.71828182845904523536);
}
