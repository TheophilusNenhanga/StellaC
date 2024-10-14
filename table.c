#include <stdlib.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "table.h"

#include <stdio.h>

#include "value.h"

#define TABLE_MAX_LOAD 0.6

void initTable(Table *table) {
	table->count = 0;
	table->capacity = 0;
	table->entries = NULL;
}

void freeTable(Table *table) {
	FREE_ARRAY(Entry, table->entries, table->capacity);
	initTable(table);
}

static Entry *findEntry(Entry *entries, int capacity, ObjectString *key) {
	// uint32_t index = key->hash % capacity;
	uint32_t index = key->hash & (capacity - 1);
	Entry *tombstone = NULL;
	for (;;) {
		Entry *entry = &entries[index];

		if (entry->key == NULL) {
			if (IS_NIL(entry->value)) {
				return tombstone != NULL ? tombstone : entry;
			}
			if (tombstone == NULL)
				tombstone = entry;
		} else if (entry->key == key) {
			return entry;
		}

		// We have collided, start probing
		// index = (index + 1) % capacity;
		index = (index + 1) & (capacity - 1);
	}
}

static void adjustCapacity(Table *table, int capacity) {
	Entry *entries = ALLOCATE(Entry, capacity);
	for (int i = 0; i < capacity; i++) {
		entries[i].key = NULL;
		entries[i].value = NIL_VAL;
	}
	table->count = 0;
	for (int i = 0; i < table->capacity; i++) {
		Entry *entry = &table->entries[i];
		if (entry->key == NULL)
			continue;

		Entry *dest = findEntry(entries, capacity, entry->key);
		dest->key = entry->key;
		dest->value = entry->value;
		table->count++;
	}
	FREE_ARRAY(Entry, table->entries, table->capacity);
	table->entries = entries;
	table->capacity = capacity;
}

TableResponse tableSet(Table *table, ObjectString *key, Value value) {
	if (table->count + 1 > table->capacity * TABLE_MAX_LOAD) {
		int capacity = GROW_CAPACITY(table->capacity);
		adjustCapacity(table, capacity);
	}

	Entry *entry = findEntry(table->entries, table->capacity, key);
	bool isNewKey = entry->key == NULL;
	bool isNilValue = IS_NIL(entry->value);

	if (isNewKey && isNilValue) {
		table->count++;
	}

	if (!isNewKey && !entry->value.isMutable) {
		return IMMUTABLE_OVERWRITE;
	}

	entry->key = key;
	entry->value = value;
	return isNewKey ? NEW_KEY_SUCCESS : SET_SUCCESS;
}

bool tableDelete(Table *table, ObjectString *key) {
	if (table->count == 0)
		return false;

	// Find the entry
	Entry *entry = findEntry(table->entries, table->capacity, key);
	if (entry->key == NULL)
		return false;

	// place a tombstone in the entry
	entry->key = NULL;
	entry->value = BOOL_VAL(true);
	return true;
}

bool tableCheck(Table *table, ObjectString *key) {
	if (table->count == 0)
		return false;
	Entry *entry = findEntry(table->entries, table->capacity, key);
	if (entry->key == NULL)
		return false;
	return true;
}

bool isTableValueMutable(Table *table, ObjectString *key) {
	if (table->count == 0)
		false;
	Entry *entry = findEntry(table->entries, table->capacity, key);
	return entry->value.isMutable;
}

TableResponse tableGet(Table *table, ObjectString *key, Value *value) {
	if (table->count == 0)
		return TABLE_EMPTY;

	Entry *entry = findEntry(table->entries, table->capacity, key);
	if (entry->key == NULL)
		return VAR_NOT_FOUND;
	*value = entry->value;
	return GET_SUCCESS;
}

void tableAddAll(Table *from, Table *to) {
	for (int i = 0; i < from->capacity; i++) {
		Entry *entry = &from->entries[i];
		if (entry->key != NULL) {
			tableSet(to, entry->key, entry->value);
		}
	}
}

ObjectString *tableFindString(Table *table, const char *chars, int length, uint32_t hash) {
	if (table->count == 0)
		return NULL;

	uint32_t index = hash % table->capacity;
	for (;;) {
		Entry *entry = &table->entries[index];
		if (entry->key == NULL) {
			// Stop if we find an empty non tombstone entry
			if (IS_NIL(entry->value))
				return NULL;
		} else if (entry->key->length == length && entry->key->hash == hash &&
							 memcmp(entry->key->chars, chars, length) == 0) {
			// we found it
			return entry->key;
		}
		index = (index + 1) % table->capacity;
	}
}
