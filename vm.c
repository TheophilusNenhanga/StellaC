#include "vm.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "./natives/collections.h"
#include "./natives/io.h"
#include "./natives/time.h"
#include "common.h"
#include "compiler.h"
#include "debug.h"
#include "memory.h"
#include "object.h"
#include "value.h"

VM vm;

static void resetStack() {
	vm.stackTop = vm.stack;
	vm.frameCount = 0;
	vm.openUpvalues = NULL;
}

static void runtimeError(const char *format, ...) {
	va_list args;
	va_start(args, format);
	fprintf(stderr, "-------RUNTIME ERROR-------\n");
	vfprintf(stderr, format, args);
	va_end(args);
	fputs("\n", stderr);

	for (int i = vm.frameCount - 1; i >= 0; i--) {
		CallFrame *frame = &vm.frames[i];
		ObjectFunction *function = frame->closure->function;
		size_t instruction = frame->ip - function->chunk.code - 1;
		fprintf(stderr, "[line %d] in ", function->chunk.lines[instruction]);
		if (function->name == NULL) {
			fprintf(stderr, "script\n");
		} else {
			printf("%s()\n", function->name->chars);
		}
	}

	resetStack();
}

static void defineNative(const char *name, NativeFn function, int arity) {
	push(OBJECT_VAL(copyString(name, (int) strlen(name))));
	push(OBJECT_VAL(newNative(function, arity)));
	tableSet(&vm.globals, AS_STRING(vm.stack[0]), vm.stack[1]);
	pop();
	pop();
}

void push(Value value) {
	*vm.stackTop = value; // stores value in the array element at the top of the stack
	vm.stackTop++; // stack top points just past the last used element, at the next available one
}

Value pop() {
	vm.stackTop--; // Move the stack pointer back to get the most recent slot in the array
	return *vm.stackTop;
}

static Value peek(int distance) { return vm.stackTop[-1 - distance]; }

static bool call(ObjectClosure *closure, int argCount) {
	if (argCount != closure->function->arity) {
		runtimeError("Expected %d arguments, got %d", closure->function->arity, argCount);
		return false;
	}

	if (vm.frameCount == FRAMES_MAX) {
		runtimeError("Stack overflow");
		return false;
	}

	CallFrame *frame = &vm.frames[vm.frameCount++];
	frame->closure = closure;
	frame->ip = closure->function->chunk.code;
	frame->slots = vm.stackTop - argCount - 1;
	return true;
}

static bool callValue(Value callee, int argCount) {
	if (IS_OBJECT(callee)) {
		switch (OBJECT_TYPE(callee)) {
			case OBJECT_CLOSURE:
				return call(AS_CLOSURE(callee), argCount);
			case OBJECT_NATIVE: {
				ObjectNative *native = AS_NATIVE_FN(callee);
				if (argCount != native->arity) {
					runtimeError("Expected %d argument(s), got %d", native->arity, argCount);
					return false;
				}
				Value result = native->function(argCount, vm.stackTop - argCount);
				vm.stackTop -= argCount + 1;
				push(result);
				return true;
			}
			case OBJECT_CLASS: {
				ObjectClass *klass = AS_CLASS(callee);
				vm.stackTop[-argCount - 1] = OBJECT_VAL(newInstance(klass));
				Value initializer;

				if (tableGet(&klass->methods, vm.initString, &initializer)) {
					return call(AS_CLOSURE(initializer), argCount);
				}
				if (argCount != 0) {
					runtimeError("Expected 0 arguments but got %d arguments.", argCount);
					return false;
				}
				return true;
			}
			case OBJECT_BOUND_METHOD: {
				ObjectBoundMethod *bound = AS_BOUND_METHOD(callee);
				vm.stackTop[-argCount - 1] = bound->receiver;
				return call(bound->method, argCount);
			}
			default:
				break;
		}
	}
	runtimeError("Can only call functions and classes.");
	return false;
}

static bool invokeFromClass(ObjectClass *klass, ObjectString *name, int argCount) {
	Value method;
	if (tableGet(&klass->methods, name, &method)) {
		return call(AS_CLOSURE(method), argCount);
	}
	runtimeError("Undefined property '%s'.", name->chars);
	return false;
}

static bool invoke(ObjectString *name, int argCount) {
	Value receiver = peek(argCount);

	if (!IS_INSTANCE(receiver)) {
		runtimeError("Only instances have methods.");
		return false;
	}

	ObjectInstance *instance = AS_INSTANCE(receiver);

	Value value;
	if (tableGet(&instance->fields, name, &value)) {
		vm.stackTop[-argCount - 1] = value;
		return callValue(value, argCount);
	}

	return invokeFromClass(instance->klass, name, argCount);
}

static bool bindMethod(ObjectClass *klass, ObjectString *name) {
	Value method;
	if (!tableGet(&klass->methods, name, &method)) {
		runtimeError("Undefined property '%s'", name->chars);
		return false;
	}

	ObjectBoundMethod *bound = newBoundMethod(peek(0), AS_CLOSURE(method));
	pop();
	push(OBJECT_VAL(bound));
	return true;
}

static ObjectUpvalue *captureUpvalue(Value *local) {
	ObjectUpvalue *prevUpvalue = NULL;
	ObjectUpvalue *upvalue = vm.openUpvalues;

	while (upvalue != NULL && upvalue->location > local) {
		prevUpvalue = upvalue;
		upvalue = upvalue->next;
	}
	if (upvalue != NULL && upvalue->location == local) {
		return upvalue;
	}

	ObjectUpvalue *createdUpvalue = newUpvalue(local);

	createdUpvalue->next = upvalue;
	if (prevUpvalue == NULL) {
		vm.openUpvalues = createdUpvalue;
	} else {
		prevUpvalue->next = createdUpvalue;
	}

	return createdUpvalue;
}

static void closeUpvalues(Value *last) {
	while (vm.openUpvalues != NULL && vm.openUpvalues->location >= last) {
		ObjectUpvalue *upvalue = vm.openUpvalues;
		upvalue->closed = *upvalue->location;
		upvalue->location = &upvalue->closed;
		vm.openUpvalues = upvalue->next;
	}
}

static void defineMethod(ObjectString *name) {
	Value method = peek(0);
	ObjectClass *klass = AS_CLASS(peek(1));
	if (tableSet(&klass->methods, name, method)) {
		pop();
	}
}

static bool isFalsy(Value value) { return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value)); }

static void concatenate() {
	ObjectString *b = AS_STRING(peek(0));
	ObjectString *a = AS_STRING(peek(1));

	int length = a->length + b->length;
	char *chars = ALLOCATE(char, length + 1);
	memcpy(chars, a->chars, a->length);
	memcpy(chars + a->length, b->chars, b->length);
	chars[length] = '\0';

	ObjectString *result = takeString(chars, length);
	pop();
	pop();
	push(OBJECT_VAL(result));
}

void initVM() {
	resetStack();
	vm.objects = NULL;
	vm.bytesAllocated = 0;
	vm.nextGC = 1024 * 1024;

	vm.grayCount = 0;
	vm.grayCapacity = 0;
	vm.grayStack = NULL;

	initTable(&vm.strings);
	initTable(&vm.globals);

	vm.initString = NULL;
	vm.initString = copyString("init", 4);

	defineNative("time_s", currentTimeSeconds, 0);
	defineNative("time_ms", currentTimeMillis, 0);
	defineNative("print", printNative, 1);
	defineNative("len", length, 1);
	defineNative("array_add", arrayAdd, 2);
	defineNative("array_rem", arrayRemove, 1);
}

void freeVM() {
	freeTable(&vm.strings);
	freeTable(&vm.globals);
	vm.initString = NULL;
	freeObjects();
}

static bool binaryOperation(OpCode operation) {

	Value b = peek(0);
	Value a = peek(1);

	if (!(IS_NUMBER(a) || IS_NUMBER(b))) {
		runtimeError("{ Error: Binary Operation } Operands must be of type <number>.");
		return false;
	}

	pop();
	pop();

	double aNum = AS_NUMBER(a);
	double bNum = AS_NUMBER(b);

	switch (operation) {
		case OP_ADD: {
			push(NUMBER_VAL(aNum + bNum));
			break;
		}

		case OP_SUBTRACT: {
			push(NUMBER_VAL(aNum - bNum));
			break;
		}

		case OP_MULTIPLY: {
			push(NUMBER_VAL(aNum * bNum));
			break;
		}

		case OP_DIVIDE: {
			if (bNum == 0) {
				runtimeError("{ Error: Division by Zero }");
				return false;
			}
			push(NUMBER_VAL(aNum / bNum));
			break;
		}

		case OP_LESS_EQUAL: {
			push(BOOL_VAL(aNum <= bNum));
			break;
		}

		case OP_GREATER_EQUAL: {
			push(BOOL_VAL(aNum >= bNum));
			break;
		}

		case OP_LESS: {
			push(BOOL_VAL(aNum < bNum));
			break;
		}

		case OP_GREATER: {
			push(BOOL_VAL(aNum > bNum));
			break;
		}
		case OP_MODULUS: {
			push(NUMBER_VAL((int64_t) aNum % (int64_t) bNum));
			break;
		}
		case OP_LEFT_SHIFT: {
			push(NUMBER_VAL((int64_t) aNum << (int64_t) bNum));
			break;
		}
		case OP_RIGHT_SHIFT: {
			push(NUMBER_VAL((int64_t) aNum >> (int64_t) bNum));
			break;
		}
	}
	return true;
}

InterpretResult globalCompoundOperation(ObjectString *name, OpCode opcode, char *operation) {
	Value currentValue;
	if (!tableGet(&vm.globals, name, &currentValue)) {
		runtimeError("Undefined variable '%s'.", name->chars);
		return INTERPRET_RUNTIME_ERROR;
	}
	if (!(IS_NUMBER(currentValue) && IS_NUMBER(peek(0)))) {
		runtimeError("Operands must be numbers for '%s' operator.", operation);
		return INTERPRET_RUNTIME_ERROR;
	}
	switch (opcode) {
		case OP_SET_GLOBAL_SLASH: {
			if (AS_NUMBER(peek(0)) != 0.0) {
				double result = AS_NUMBER(currentValue) / AS_NUMBER(peek(0));
				tableSet(&vm.globals, name, NUMBER_VAL(result));
				break;
			}
			runtimeError("Division by zero error: '%s'.", name->chars);
			return INTERPRET_RUNTIME_ERROR;
		}
		case OP_SET_GLOBAL_STAR: {
			double result = AS_NUMBER(currentValue) * AS_NUMBER(peek(0));
			tableSet(&vm.globals, name, NUMBER_VAL(result));
			break;
		}
		case OP_SET_GLOBAL_PLUS: {
			double result = AS_NUMBER(currentValue) + AS_NUMBER(peek(0));
			tableSet(&vm.globals, name, NUMBER_VAL(result));
			break;
		}
		case OP_SET_GLOBAL_MINUS: {
			double result = AS_NUMBER(currentValue) - AS_NUMBER(peek(0));
			tableSet(&vm.globals, name, NUMBER_VAL(result));
			break;
		}
		default:;
	}
	return INTERPRET_OK;
}

static InterpretResult run() {
	CallFrame *frame = &vm.frames[vm.frameCount - 1];

#define READ_BYTE() (*frame->ip++)
#define READ_CONSTANT() (frame->closure->function->chunk.constants.values[READ_BYTE()])
#define READ_STRING() AS_STRING(READ_CONSTANT())
#define READ_SHORT() (frame->ip += 2, (uint16_t) ((frame->ip[-2] << 8) | frame->ip[-1]))

	for (;;) {
#ifdef DEBUG_TRACE_EXECUTION
		printf("        ");
		for (Value *slot = vm.stack; slot < vm.stackTop; slot++) {
			printf("[");
			printValue(*slot);
			printf("]");
		}
		printf("\n");

		disassembleInstruction(&frame->closure->function->chunk, (int) (frame->ip - frame->closure->function->chunk.code));
#endif

		uint8_t instruction;
		// decoding and dispatching
		switch (instruction = READ_BYTE()) {
			case OP_CONSTANT: {
				Value constant = READ_CONSTANT();
				push(constant);
				break;
			}

			case OP_RETURN: {
				Value result = pop();
				closeUpvalues(frame->slots);
				vm.frameCount--;
				if (vm.frameCount == 0) {
					pop();
					return INTERPRET_OK;
				}
				vm.stackTop = frame->slots;
				push(result);
				frame = &vm.frames[vm.frameCount - 1];
				break;
			}

			case OP_CLOSURE: {
				ObjectFunction *function = AS_FUNCTION(READ_CONSTANT());
				ObjectClosure *closure = newClosure(function);
				push(OBJECT_VAL(closure));

				for (int i = 0; i < closure->upvalueCount; i++) {
					uint8_t isLocal = READ_BYTE();
					uint8_t index = READ_BYTE();

					if (isLocal) {
						closure->upvalues[i] = captureUpvalue(frame->slots + index);
					} else {
						closure->upvalues[i] = frame->closure->upvalues[index];
					}
				}

				break;
			}

			case OP_NIL: {
				push(NIL_VAL);
				break;
			}

			case OP_TRUE: {
				push(BOOL_VAL(true));
				break;
			}

			case OP_FALSE: {
				push(BOOL_VAL(false));
				break;
			}

			case OP_EQUAL: {
				Value b = pop();
				Value a = pop();
				push(BOOL_VAL(valuesEqual(a, b)));
				break;
			}

			case OP_NOT_EQUAL: {
				Value b = pop();
				Value a = pop();
				push(BOOL_VAL(!valuesEqual(a, b)));
				break;
			}

			case OP_GREATER: {
				if (!binaryOperation(OP_GREATER)) {
					return INTERPRET_RUNTIME_ERROR;
				}
				break;
			}

			case OP_LESS: {
				if (!binaryOperation(OP_LESS)) {
					return INTERPRET_RUNTIME_ERROR;
				}
				break;
			}

			case OP_LESS_EQUAL: {
				if (!binaryOperation(OP_LESS_EQUAL)) {
					return INTERPRET_RUNTIME_ERROR;
				}
				break;
			}

			case OP_GREATER_EQUAL: {
				if (!binaryOperation(OP_GREATER_EQUAL)) {
					return INTERPRET_RUNTIME_ERROR;
				}
				break;
			}

			case OP_NEGATE: {
				if (IS_NUMBER(peek(0))) {
					push(NUMBER_VAL(-AS_NUMBER(pop())));
					break;
				}
				runtimeError("Operand must be of type <number>.");
				return INTERPRET_RUNTIME_ERROR;
			}

			case OP_ADD: {
				if (IS_STRING(peek(0)) && IS_STRING(peek(1))) {
					concatenate();
					break;
				}
				if (!binaryOperation(OP_ADD)) {
					return INTERPRET_RUNTIME_ERROR;
				}
				break;
			}

			case OP_SUBTRACT: {
				if (!binaryOperation(OP_SUBTRACT)) {
					return INTERPRET_RUNTIME_ERROR;
				}
				break;
			}

			case OP_MULTIPLY: {
				if (!binaryOperation(OP_MULTIPLY)) {
					return INTERPRET_RUNTIME_ERROR;
				}
				break;
			}

			case OP_DIVIDE: {
				if (!binaryOperation(OP_DIVIDE)) {
					return INTERPRET_RUNTIME_ERROR;
				}
				break;
			}

			case OP_MODULUS: {
				if (!binaryOperation(OP_MODULUS)) {
					return INTERPRET_RUNTIME_ERROR;
				}
				break;
			}

			case OP_LEFT_SHIFT: {
				if (!binaryOperation(OP_LEFT_SHIFT)) {
					return INTERPRET_RUNTIME_ERROR;
				}
				break;
			}

			case OP_RIGHT_SHIFT: {
				if (!binaryOperation(OP_RIGHT_SHIFT)) {
					return INTERPRET_RUNTIME_ERROR;
				}
				break;
			}

			case OP_NOT: {
				push(BOOL_VAL(isFalsy(pop())));
				break;
			}

			case OP_PRINT: {
				printValue(pop());
				printf("\n");
				break;
			}

			case OP_POP: {
				pop();
				break;
			}

			case OP_DEFINE_GLOBAL: {
				// TODO: Check what happens when you try define a value that already exists
				ObjectString *name = READ_STRING();
				if (tableSet(&vm.globals, name, peek(0))) {
					pop();
					break;
				}
				runtimeError("Cannot define '%s' because it is already defined.", name->chars);
				return INTERPRET_RUNTIME_ERROR;
			}

			case OP_GET_GLOBAL: {
				ObjectString *name = READ_STRING();
				Value value;
				if (tableGet(&vm.globals, name, &value)) {
					push(value);
					break;
				}
				runtimeError("Undefined variable '%s'.", name->chars);
				return INTERPRET_RUNTIME_ERROR;
			}

			case OP_SET_GLOBAL: {
				ObjectString *name = READ_STRING();
				if (!tableSet(&vm.globals, name, peek(0))) {
					runtimeError("Cannot give variable '%s' a value because it has not been defined", name->chars);
					return INTERPRET_RUNTIME_ERROR;
				}
				break;
			}

			case OP_GET_LOCAL: {
				uint8_t slot = READ_BYTE();
				push(frame->slots[slot]);
				break;
			}

			case OP_SET_LOCAL: {
				uint8_t slot = READ_BYTE();
				frame->slots[slot] = peek(0);
				break;
			}

			case OP_JUMP_IF_FALSE: {
				uint16_t offset = READ_SHORT();
				if (isFalsy(peek(0))) {
					frame->ip += offset;
					break;
				}
				break;
			}

			case OP_JUMP: {
				uint16_t offset = READ_SHORT();
				frame->ip += offset;
				break;
			}

			case OP_LOOP: {
				uint16_t offset = READ_SHORT();
				frame->ip -= offset;
				break;
			}

			case OP_CALL: {
				int argCount = READ_BYTE();
				if (!callValue(peek(argCount), argCount)) {
					return INTERPRET_RUNTIME_ERROR;
				}
				frame = &vm.frames[vm.frameCount - 1];
				break;
			}

			case OP_GET_UPVALUE: {
				uint8_t slot = READ_BYTE();
				push(*frame->closure->upvalues[slot]->location);
				break;
			}

			case OP_SET_UPVALUE: {
				uint8_t slot = READ_BYTE();
				*frame->closure->upvalues[slot]->location = peek(0);
				break;
			}

			case OP_CLOSE_UPVALUE: {
				closeUpvalues(vm.stackTop - 1);
				pop();
				break;
			}

			case OP_CLASS: {
				push(OBJECT_VAL(newClass(READ_STRING())));
				break;
			}

			case OP_GET_PROPERTY: {
				if (!IS_INSTANCE(peek(0))) {
					runtimeError("Only instances have properties.");
					return INTERPRET_RUNTIME_ERROR;
				}
				ObjectInstance *instance = AS_INSTANCE(peek(0));
				ObjectString *name = READ_STRING();

				Value value;
				bool fieldFound = false;
				if (tableGet(&instance->fields, name, &value)) {
					pop();
					push(value);
					fieldFound = true;
					break;
				}

				if (!fieldFound) {
					if (!bindMethod(instance->klass, name)) {
						return INTERPRET_RUNTIME_ERROR;
					}
				}
				break;
			}

			case OP_SET_PROPERTY: {
				if (!IS_INSTANCE(peek(1))) {
					runtimeError("Only instances have fields.");
					return INTERPRET_RUNTIME_ERROR;
				}

				ObjectInstance *instance = AS_INSTANCE(peek(1));
				if (tableSet(&instance->fields, READ_STRING(), peek(0))) {
					Value value = pop();
					pop();
					push(value);
					break;
				}
				runtimeError("Undefined property '%s'.", READ_STRING());
				return INTERPRET_RUNTIME_ERROR;
			}
			case OP_METHOD: {
				defineMethod(READ_STRING());
				break;
			}

			case OP_INVOKE: {
				ObjectString *method = READ_STRING();
				int argCount = READ_BYTE();
				if (!invoke(method, argCount)) {
					return INTERPRET_RUNTIME_ERROR;
				}
				frame = &vm.frames[vm.frameCount - 1];
				break;
			}

			case OP_INHERIT: {
				Value superClass = peek(1);

				if (!IS_CLASS(superClass)) {
					runtimeError("Cannot inherit from non class object.");
					return INTERPRET_RUNTIME_ERROR;
				}

				ObjectClass *subClass = AS_CLASS(peek(0));
				tableAddAll(&AS_CLASS(superClass)->methods, &subClass->methods);
				pop();
				break;
			}

			case OP_GET_SUPER: {
				ObjectString *name = READ_STRING();
				ObjectClass *superClass = AS_CLASS(pop());

				if (!bindMethod(superClass, name)) {
					return INTERPRET_RUNTIME_ERROR;
				}
				break;
			}

			case OP_SUPER_INVOKE: {
				ObjectString *method = READ_STRING();
				int argCount = READ_BYTE();
				ObjectClass *superClass = AS_CLASS(pop());
				if (!invokeFromClass(superClass, method, argCount)) {
					return INTERPRET_RUNTIME_ERROR;
				}
				frame = &vm.frames[vm.frameCount - 1];
				break;
			}

			case OP_TABLE: {
				uint16_t elementCount = READ_SHORT();
				ObjectTable *table = newTable(elementCount);
				table->size = elementCount;
				for (int i = elementCount-1; i >= 0; i--) {
					Value value = pop();
					Value key = pop();
				}
				push(OBJECT_VAL(table));
				break;
			}


			case OP_ARRAY: {
				uint16_t elementCount = READ_SHORT();
				ObjectArray *array = newArray(elementCount);
				array->size = elementCount;
				for (int i = elementCount - 1; i >= 0; i--) {
					array->array[i] = pop();
				}
				push(OBJECT_VAL(array));
				break;
			}

			case OP_GET_ARRAY: {
				int index = AS_NUMBER(pop());
				ObjectArray *array = AS_ARRAY(pop());
				if (index >= 0 && index < array->size) {
					push(array->array[index]);
					break;
				} else {
					runtimeError("{ Error: OP_GET_ARRAY } Array index out of bounds.");
					return INTERPRET_RUNTIME_ERROR;
				}
			}

			case OP_SET_ARRAY: {
				Value value = pop();
				int index = AS_NUMBER(pop());
				ObjectArray *array = AS_ARRAY(pop());
				if (index >= 0 && index < array->size) {
					array->array[index] = value;
				} else {
					runtimeError("{ Error: OP_SET_ARRAY } Array index out of bounds.");
					return INTERPRET_RUNTIME_ERROR;
				}
				break;
			}
			case OP_SET_LOCAL_SLASH: {
				uint8_t slot = READ_BYTE();
				Value currentValue = frame->slots[slot];

				if (!(IS_NUMBER(currentValue) && IS_NUMBER(peek(0)))) {
					runtimeError("Both operands must be numbers for the '/=' operator");
					return INTERPRET_RUNTIME_ERROR;
				}

				double divisor = AS_NUMBER(peek(0));

				if (divisor == 0.0) {
					runtimeError("{ Error: OP_SET_LOCAL_SLASH } Divisor must be non-zero.");
					return INTERPRET_RUNTIME_ERROR;
				}

				frame->slots[slot] = NUMBER_VAL(AS_NUMBER(currentValue) / divisor);
				break;
			}
			case OP_SET_LOCAL_STAR: {
				uint8_t slot = READ_BYTE();
				Value currentValue = frame->slots[slot];

				if (!(IS_NUMBER(currentValue) && IS_NUMBER(peek(0)))) {
					runtimeError("Both operands must be numbers for the '*=' operator");
					return INTERPRET_RUNTIME_ERROR;
				}

				double divisor = AS_NUMBER(peek(0));

				if (divisor == 0.0) {
					runtimeError("{ Error: OP_SET_LOCAL_SLASH } Divisor must be non-zero.");
					return INTERPRET_RUNTIME_ERROR;
				}

				frame->slots[slot] = NUMBER_VAL(AS_NUMBER(currentValue) * divisor);
				break;
			}
			case OP_SET_LOCAL_PLUS: {
				uint8_t slot = READ_BYTE();
				Value currentValue = frame->slots[slot];

				if (!(IS_NUMBER(currentValue) && IS_NUMBER(peek(0)))) {
					runtimeError("Both operands must be numbers for the '+=' operator");
					return INTERPRET_RUNTIME_ERROR;
				}

				double divisor = AS_NUMBER(peek(0));

				if (divisor == 0.0) {
					runtimeError("{ Error: OP_SET_LOCAL_SLASH } Divisor must be non-zero.");
					return INTERPRET_RUNTIME_ERROR;
				}

				frame->slots[slot] = NUMBER_VAL(AS_NUMBER(currentValue) + divisor);
				break;
			}
			case OP_SET_LOCAL_MINUS: {
				uint8_t slot = READ_BYTE();
				Value currentValue = frame->slots[slot];

				if (!(IS_NUMBER(currentValue) && IS_NUMBER(peek(0)))) {
					runtimeError("Both operands must be numbers for the '-=' operator");
					return INTERPRET_RUNTIME_ERROR;
				}

				double divisor = AS_NUMBER(peek(0));

				if (divisor == 0.0) {
					runtimeError("{ Error: OP_SET_LOCAL_SLASH } Divisor must be non-zero.");
					return INTERPRET_RUNTIME_ERROR;
				}

				frame->slots[slot] = NUMBER_VAL(AS_NUMBER(currentValue) - divisor);
				break;
			}
			case OP_SET_UPVALUE_SLASH: {
				uint8_t slot = READ_BYTE();
				Value currentValue = *frame->closure->upvalues[slot]->location;

				if (!(IS_NUMBER(currentValue) && IS_NUMBER(peek(0)))) {
					runtimeError("Both operands must be numbers for the '/=' operator");
					return INTERPRET_RUNTIME_ERROR;
				}
				double divisor = AS_NUMBER(peek(0));
				if (divisor == 0.0) {
					runtimeError("{ Error: OP_SET_UPVALUE_SLASH } Divisor must be non-zero.");
					return INTERPRET_RUNTIME_ERROR;
				}
				*frame->closure->upvalues[slot]->location = NUMBER_VAL(AS_NUMBER(currentValue) / divisor);

				break;
			}
			case OP_SET_UPVALUE_STAR: {
				uint8_t slot = READ_BYTE();
				Value currentValue = *frame->closure->upvalues[slot]->location;

				if (!(IS_NUMBER(currentValue) && IS_NUMBER(peek(0)))) {
					runtimeError("Both operands must be numbers for the '*=' operator");
					return INTERPRET_RUNTIME_ERROR;
				}

				*frame->closure->upvalues[slot]->location = NUMBER_VAL(AS_NUMBER(currentValue) * AS_NUMBER(peek(0)));

				break;
			}
			case OP_SET_UPVALUE_PLUS: {
				uint8_t slot = READ_BYTE();
				Value currentValue = *frame->closure->upvalues[slot]->location;

				if (!(IS_NUMBER(currentValue) && IS_NUMBER(peek(0)))) {
					runtimeError("Both operands must be numbers for the '+=' operator");
					return INTERPRET_RUNTIME_ERROR;
				}
				*frame->closure->upvalues[slot]->location = NUMBER_VAL(AS_NUMBER(currentValue) + AS_NUMBER(peek(0)));

				break;
			}
			case OP_SET_UPVALUE_MINUS: {
				uint8_t slot = READ_BYTE();
				Value currentValue = *frame->closure->upvalues[slot]->location;

				if (!(IS_NUMBER(currentValue) && IS_NUMBER(peek(0)))) {
					runtimeError("Both operands must be numbers for the '-=' operator");
					return INTERPRET_RUNTIME_ERROR;
				}

				*frame->closure->upvalues[slot]->location = NUMBER_VAL(AS_NUMBER(currentValue) / AS_NUMBER(peek(0)));
				break;
			}
			case OP_SET_GLOBAL_SLASH: {
				ObjectString *name = READ_STRING();
				if (globalCompoundOperation(name, OP_SET_GLOBAL_SLASH, "/=") == INTERPRET_RUNTIME_ERROR) {
					return INTERPRET_RUNTIME_ERROR;
				}
				break;
			}
			case OP_SET_GLOBAL_STAR: {
				ObjectString *name = READ_STRING();
				if (globalCompoundOperation(name, OP_SET_GLOBAL_STAR, "*=") == INTERPRET_RUNTIME_ERROR) {
					return INTERPRET_RUNTIME_ERROR;
				}
				break;
			}
			case OP_SET_GLOBAL_PLUS: {
				ObjectString *name = READ_STRING();
				if (globalCompoundOperation(name, OP_SET_GLOBAL_PLUS, "+=") == INTERPRET_RUNTIME_ERROR) {
					return INTERPRET_RUNTIME_ERROR;
				}
				break;
			}
			case OP_SET_GLOBAL_MINUS: {
				ObjectString *name = READ_STRING();
				if (globalCompoundOperation(name, OP_SET_GLOBAL_MINUS, "-=") == INTERPRET_RUNTIME_ERROR) {
					return INTERPRET_RUNTIME_ERROR;
				}
				break;
			}

			default: {
			}
		}
	}
#undef READ_BYTE
#undef READ_CONSTANT
#undef BINARY_OP
#undef BOOL_BINARY_OP
#undef READ_STRING
#undef READ_SHORT
}

InterpretResult interpret(const char *source) {
	ObjectFunction *function = compile(source);
	if (function == NULL)
		return INTERPRET_COMPILE_ERROR;

	push(OBJECT_VAL(function));
	ObjectClosure *closure = newClosure(function);
	pop();
	push(OBJECT_VAL(closure));
	call(closure, 0);

	return run();
}

// Store ip in a local variable to improve performance
