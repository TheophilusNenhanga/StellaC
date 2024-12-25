#ifndef COMPILER_H
#define COMPILER_H
#include "object.h"
#include "scanner.h"


ObjectFunction *compile(char *source);
void markCompilerRoots();
typedef struct {
	char *source;
	Token current;
	Token previous;
	bool hadError;
	bool panicMode;
} Parser;

// Precedence in order from lowest to highest
typedef enum {
	PREC_NONE,
	PREC_ASSIGNMENT, // =
	PREC_OR, // or
	PREC_AND, // and
	PREC_EQUALITY, // == !=
	PREC_COMPARISON, // < > <= >=
	PREC_SHIFT, // << >>
	PREC_TERM, // + -
	PREC_FACTOR, // * /
	PREC_UNARY, // ! -
	PREC_CALL, // . () []
	PREC_PRIMARY
} Precedence;

typedef enum { COMPOUND_OP_PLUS, COMPOUND_OP_MINUS, COMPOUND_OP_STAR, COMPOUND_OP_SLASH } CompoundOp;

typedef void (*ParseFn)(bool canAssign);


typedef struct {
	ParseFn prefix;
	ParseFn infix;
	Precedence precedence;
} ParseRule;

typedef struct {
	Token name;
	int depth;
	bool isCaptured;
} Local;

typedef struct {
	uint8_t index;
	bool isLocal;
} Upvalue;

typedef enum { TYPE_FUNCTION, TYPE_SCRIPT, TYPE_METHOD, TYPE_INITIALIZER, TYPE_ANONYMOUS } FunctionType;

typedef struct {
	struct Compiler *enclosing;
	ObjectFunction *function;
	ObjectModule *module;
	Local locals[UINT8_COUNT];
	Upvalue upvalues[UINT8_COUNT];
	FunctionType type;
	int localCount;
	int scopeDepth; // 0 is global scope
} Compiler;

typedef struct ClassCompiler {
	struct ClassCompiler *enclosing;
	bool hasSuperclass;
} ClassCompiler;

#endif // COMPILER_H
