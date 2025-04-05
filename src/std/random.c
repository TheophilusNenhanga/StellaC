#include "random.h"

#define A 25214903917
#define C 11

// Linear Congruential Algorithm
// Adapted from https://learn.microsoft.com/en-us/archive/msdn-magazine/2016/august/test-run-lightweight-random-number-generation

ObjectResult* randomSeedMethod(VM* vm, int argCount, Value *args) {
    Value seed = args[1];
    if (!IS_INT(seed)) {
        return stellaErr(vm, newError(vm, copyString(vm, "Seed must be a number.", 22), RUNTIME, false));
    }

    uint64_t seedInt = (uint64_t) AS_INT(seed);

    ObjectRandom *random = AS_CRUX_RANDOM(args[0]);
    random->seed = seedInt;
    return stellaOk(vm, NIL_VAL);
}

int next(uint64_t* seed, int bits) {
    *seed = (*seed * A + C) & ((1ULL << 48) - 1);
    return (int)(*seed >> (48 - bits));
}

double getNext(ObjectRandom *random) {
    int bits26 = next(&random->seed, 26);
    int bits27 = next(&random->seed, 27);
    
    double result = (((uint64_t)bits26 << 27) + bits27) / (double)(1ULL << 53);
    return result;
}

Value randomNextMethod(VM* vm, int argCount, Value *args) {
    ObjectRandom *random = AS_CRUX_RANDOM(args[0]);
    return FLOAT_VAL(getNext(random));
}   

Value randomInitFunction(VM* vm, int argCount, Value *args) {
    return OBJECT_VAL(newRandom(vm));
}

// Returns a random integer in the range [min, max] inclusive
ObjectResult* randomIntMethod(VM* vm, int argCount, Value *args) {
    Value min = args[1];
    Value max = args[2];
    
    if (!IS_INT(min) || !IS_INT(max)) {
        return stellaErr(vm, newError(vm, copyString(vm, "Arguments must be of type 'int'.", 32), TYPE, false));
    }

    int32_t minInt = AS_INT(min);
    int32_t maxInt = AS_INT(max);

    if (minInt > maxInt) {
        return stellaErr(vm, newError(vm, copyString(vm, "Min must be less than or equal to max", 37), RUNTIME, false));
    }

    ObjectRandom *random = AS_CRUX_RANDOM(args[0]);
    double r = getNext(random);
    uint64_t range = (uint64_t)maxInt - (uint64_t)minInt + 1;
    int32_t result = minInt + (int32_t)(r * range);
    
    return stellaOk(vm, INT_VAL(result));
}

// Returns a random double in the range [min, max]
ObjectResult* randomDoubleMethod(VM* vm, int argCount, Value *args) {
    Value min = args[1];
    Value max = args[2];

    if (!IS_FLOAT(min) && !IS_INT(max)) {
        return stellaErr(vm, newError(vm, copyString(vm, "Parameter <min> must be a number.", 33), RUNTIME, false));
    }

    if (!IS_FLOAT(max) && !IS_INT(max)) {
        return stellaErr(vm, newError(vm, copyString(vm, "Parameter <max> must be a number.", 33), RUNTIME, false));
    }
    

    double minDouble = IS_FLOAT(min) ? AS_FLOAT(min) : (double) AS_INT(min);
    double maxDouble = IS_FLOAT(max) ? AS_FLOAT(max) : (double) AS_INT(max);

    if (minDouble > maxDouble) {
        return stellaErr(vm, newError(vm, copyString(vm, "Parameter <min> must be less than or equal to parameter <max>.", 62), RUNTIME, false));
    }

    ObjectRandom *random = AS_CRUX_RANDOM(args[0]);
    double r = getNext(random);
    double result = minDouble + r * (maxDouble - minDouble);
    
    return stellaOk(vm, FLOAT_VAL(result));
}

// Returns true with probability p (0 <= p <= 1)
ObjectResult* randomBoolMethod(VM* vm, int argCount, Value *args) {
    Value p = args[1];
    if (!IS_FLOAT(p) && !IS_INT(p)) {
        return stellaErr(vm, newError(vm, copyString(vm, "Argument must be of type 'int' | 'float'.", 41), RUNTIME, false));
    }

    double prob = IS_FLOAT(p) ? AS_FLOAT(p) : (double) AS_INT(p);

    if (prob < 0 || prob > 1) {
        return stellaErr(vm, newError(vm, copyString(vm, "Probability must be between 0 and 1", 37), RUNTIME, false));
    }

    ObjectRandom *random = AS_CRUX_RANDOM(args[0]);
    double r = getNext(random);

    return stellaOk(vm, BOOL_VAL(r < prob));
}

// Returns a random element from the array
ObjectResult* randomChoiceMethod(VM* vm, int argCount, Value *args) {
    Value array = args[1];
    if (!IS_CRUX_ARRAY(array)) {
        return stellaErr(vm, newError(vm, copyString(vm, "Argument must be an array", 25), RUNTIME, false));
    }

    ObjectArray *arr = AS_CRUX_ARRAY(array);
    if (arr->size == 0) {
        return stellaErr(vm, newError(vm, copyString(vm, "Array cannot be empty", 20), RUNTIME, false));
    }

    ObjectRandom *random = AS_CRUX_RANDOM(args[0]);
    double r = getNext(random);
    uint32_t index = (uint32_t)(r * arr->size);
    
    return stellaOk(vm, arr->array[index]);
}


