#ifndef MEMORY_H
#define MEMORY_H

#include "common.h"
#include "object.h"

#define TABLE_MAX_LOAD 0.6

void *reallocate(void *pointer, size_t oldSize, size_t newSize);

#define ALLOCATE(type, count) (type *) reallocate(NULL, 0, sizeof(type) * count)

#define FREE(type, pointer) reallocate(pointer, sizeof(type), 0)

#define GROW_CAPACITY(capacity) ((capacity) < 16 ? 16 : (capacity) * 2)

#define GROW_ARRAY(type, pointer, oldCount, newCount)                                                                  \
	(type *) reallocate(pointer, sizeof(type) * (oldCount), sizeof(type) * (newCount))

#define FREE_ARRAY(type, pointer, oldCount) reallocate(pointer, sizeof(type) * (oldCount), 0)

void markObject(Object *object);

void markValue(Value value);

void collectGarbage();

void freeObjects();

#endif // MEMORY_H
