#ifndef TABLE_H
#define TABLE_H
#define TABLE_MAX_LOAD 0.6
#define FNV_PRIME 16777619u

#include "common.h"
#include "value.h"

typedef struct {
	ObjectString *key;
	Value value;
} Entry;

typedef struct {
	int count;
	int capacity;
	Entry *entries;
} Table;

void initTable(Table *table);

void freeTable(Table *table);

bool tableSet(Table *table, ObjectString *key, Value value);

bool tableGet(Table *table, ObjectString *key, Value *value);

bool tableDelete(Table *table, ObjectString *key);

void tableAddAll(Table *from, Table *to);

ObjectString *tableFindString(Table *table, const char *chars, int length, uint32_t hash);

void tableRemoveWhite(Table *table);

void markTable(Table *table);

#endif
