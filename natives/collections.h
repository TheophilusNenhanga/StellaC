#ifndef COLLECTIONS_H
#define COLLECTIONS_H

#include "../value.h"
#include "../common.h"

#define MAX_ARRAY_SIZE UINT16_MAX - 1

Value length(int argCount, Value *args);

Value arrayAdd(int argCount, Value *args);

Value arrayRemove(int argCount, Value *args);

#endif
