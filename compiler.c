#include "compiler.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "chunk.h"
#include "memory.h"
#include "object.h"
#include "scanner.h"

#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif

typedef struct {
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

typedef enum {
	TYPE_FUNCTION,
	TYPE_SCRIPT,
	TYPE_METHOD,
	TYPE_INITIALIZER,
} FunctionType;

typedef struct {
	struct Compiler *enclosing;
	ObjectFunction *function;
	FunctionType type;
	Local locals[UINT8_COUNT];
	int localCount;
	int scopeDepth; // 0 is global scope
	int loopDepth;
	Upvalue upvalues[UINT8_COUNT];
} Compiler;

typedef struct ClassCompiler {
	struct ClassCompiler *enclosing;
	bool hasSuperclass;
} ClassCompiler;

typedef struct {
	int loopStart;
	int exitJump;
} LoopContext;

LoopContext loopStack[UINT16_MAX];

// Make these two non globals
Parser parser;
Compiler *current = NULL;
ClassCompiler *currentClass = NULL;
Chunk *compilingChunk;


static void expression();

static void parsePrecedence(Precedence precedence);

static ParseRule *getRule(TokenType type);

static void binary(bool canAssign);

static void unary(bool canAssign);

static void grouping(bool canAssign);

static void number(bool canAssign);

static void statement();

static void declaration();

static Chunk *currentChunk() { return &current->function->chunk; }

static void errorAt(Token *token, const char *message) {
	if (parser.panicMode)
		return;
	parser.panicMode = true;
	fprintf(stderr, "-------COMPILER ERROR-------\n");
	fprintf(stderr, "[line %d] Error", token->line);
	if (token->type == TOKEN_EOF) {
		fprintf(stderr, " at end");
	} else if (token->type == TOKEN_ERROR) {
	} else {
		fprintf(stderr, " at '%.*s'", token->length, token->start);
	}
	fprintf(stderr, ": %s\n", message);
	parser.hadError = true;
}

static void error(const char *message) { errorAt(&parser.previous, message); }

static void errorAtCurrent(const char *message) { errorAt(&parser.current, message); }

static void advance() {
	parser.previous = parser.current;
	for (;;) {
		parser.current = scanToken();
		if (parser.current.type != TOKEN_ERROR)
			break;
		errorAtCurrent(parser.current.start);
	}
}

/**
 * Reads the next token. Validates that the token has the expected type. If not
 * reports an error.
 */
static void consume(TokenType type, const char *message) {
	if (parser.current.type == type) {
		advance();
		return;
	}
	errorAtCurrent(message);
}

static bool check(TokenType type) { return parser.current.type == type; }

static bool match(TokenType type) {
	if (!check(type))
		return false;
	advance();
	return true;
}

static void emitByte(uint8_t byte) { writeChunk(currentChunk(), byte, parser.previous.line); }

static void emitBytes(uint8_t byte1, uint8_t byte2) {
	emitByte(byte1);
	emitByte(byte2);
}

static void emitLoop(int loopStart) {
	emitByte(OP_LOOP);
	int offset = currentChunk()->count - loopStart + 2; // +2 takes into account the size of the OP_LOOP
	if (offset > UINT16_MAX) {
		error("Loop body too large.");
	}
	emitBytes(((offset >> 8) & 0xff), (offset & 0xff));
}

static int emitJump(uint8_t instruction) {
	emitByte(instruction);
	emitBytes(0xff, 0xff);
	return currentChunk()->count - 2;
}

static void emitReturn() {
	if (current->type == TYPE_INITIALIZER) {
		emitBytes(OP_GET_LOCAL, 0);
	} else {
		emitByte(OP_NIL);
	}
	emitByte(OP_RETURN);
}

static uint8_t makeConstant(Value value) {
	int constant = addConstant(currentChunk(), value);
	// Add constant adds the given value to the end of the constant table and
	// returns the index.
	if (constant > UINT8_MAX) {
		// Only 256 constants can be stored and loaded in a chunk.
		// TODO: Add new instruction 'OP_CONSTANT_16' to store two byte operands
		// that can handle more constants when needed
		error("Too many constants in one chunk.");
		return 0;
	}
	return (uint8_t) constant;
}

static void emitConstant(Value value) { emitBytes(OP_CONSTANT, makeConstant(value)); }

static void patchJump(int offset) {
	// -2 to adjust for the bytecode for the jump offset itself
	int jump = currentChunk()->count - offset - 2;
	if (jump > UINT16_MAX) {
		error("Too much code to jump over.");
	}
	currentChunk()->code[offset] = (jump >> 8) & 0xff;
	currentChunk()->code[offset + 1] = jump & 0xff;
}

static void initCompiler(Compiler *compiler, FunctionType type) {
	compiler->enclosing = current;
	compiler->function = NULL;
	compiler->type = type;
	compiler->localCount = 0;
	compiler->scopeDepth = 0;
	compiler->loopDepth = 0;
	compiler->function = newFunction();
	current = compiler;

	if (type != TYPE_SCRIPT) {
		current->function->name = copyString(parser.previous.start, parser.previous.length);
	}

	Local *local = &current->locals[current->localCount++];
	local->depth = 0;
	local->name.start = "";
	local->name.length = 0;
	local->isCaptured = false;

	if (type != TYPE_FUNCTION) {
		local->name.start = "self";
		local->name.length = 4;
	} else {
		local->name.start = "";
		local->name.length = 0;
	}
}

static uint8_t identifierConstant(Token *name) {
	return makeConstant(OBJECT_VAL(copyString(name->start, name->length)));
}

static void beginScope() { current->scopeDepth++; }

static void endScope() {
	current->scopeDepth--;

	while (current->localCount > 0 && current->locals[current->localCount - 1].depth > current->scopeDepth) {
		if (current->locals[current->localCount - 1].isCaptured) {
			emitByte(OP_CLOSE_UPVALUE);
		} else {
			emitByte(OP_POP);
		}
		current->localCount--;
	}
}

static bool identifiersEqual(Token *a, Token *b) {
	if (a->length != b->length)
		return false;
	return memcmp(a->start, b->start, a->length) == 0;
}

static int resolveLocal(Compiler *compiler, Token *name) {
	for (int i = compiler->localCount - 1; i >= 0; i--) {
		Local *local = &compiler->locals[i];
		if (identifiersEqual(name, &local->name)) {
			if (local->depth == -1) {
				error("Cannot read local variable in its own initializer");
			}
			return i;
		}
	}
	return -1;
}

static int addUpvalue(Compiler *compiler, uint8_t index, bool isLocal) {
	int upvalueCount = compiler->function->upvalueCount;

	for (int i = 0; i < upvalueCount; i++) {
		Upvalue *upvalue = &compiler->upvalues[i];
		if (upvalue->index == index && upvalue->isLocal == isLocal) {
			return i;
		}
	}

	if (upvalueCount >= UINT8_COUNT) {
		error("Too many closure variables in function.");
		return 0;
	}

	compiler->upvalues[upvalueCount].isLocal = isLocal;
	compiler->upvalues[upvalueCount].index = index;
	return compiler->function->upvalueCount++;
}

static int resolveUpvalue(Compiler *compiler, Token *name) {
	if (compiler->enclosing == NULL)
		return -1;

	int local = resolveLocal(compiler->enclosing, name);
	if (local != -1) {
		((Compiler *) compiler->enclosing)->locals[local].isCaptured = true; //
		return addUpvalue(compiler, (uint8_t) local, true);
	}

	int upValue = resolveUpvalue(compiler->enclosing, name);
	if (upValue != -1) {
		return addUpvalue(compiler, (uint8_t) upValue, false);
	}

	return -1;
}

static void addLocal(Token name) {
	if (current->localCount == UINT8_COUNT) {
		error("Too many local variables in function.");
		return;
	}

	Local *local = &current->locals[current->localCount++];
	local->name = name;
	local->depth = -1;
	local->isCaptured = false;
}

static void declareVariable() {
	if (current->scopeDepth == 0)
		return;

	Token *name = &parser.previous;

	for (int i = current->localCount - 1; i >= 0; i--) {
		Local *local = &current->locals[i];
		if (local->depth != -1 && local->depth < current->scopeDepth) {
			break;
		}
		if (identifiersEqual(name, &local->name)) {
			error("Cannot redefine variable in the same scope");
		}
	}

	addLocal(*name);
}

static void markInitialized() {
	if (current->scopeDepth == 0)
		return;
	current->locals[current->localCount - 1].depth = current->scopeDepth;
}

static uint8_t parseVariable(const char *errorMessage) {
	consume(TOKEN_IDENTIFIER, errorMessage);
	declareVariable();
	if (current->scopeDepth > 0)
		return 0;
	return identifierConstant(&parser.previous);
}

static void defineVariable(uint8_t global) {
	if (current->scopeDepth > 0) {
		markInitialized();
		return;
	}
	emitBytes(OP_DEFINE_GLOBAL, global);
}

static uint8_t argumentList() {
	uint8_t argCount = 0;
	if (!check(TOKEN_RIGHT_PAREN)) {
		do {
			expression();
			if (argCount == 255) {
				error("Cannot have more than 255 arguments.");
			}
			argCount++;
		} while (match(TOKEN_COMMA));
	}
	consume(TOKEN_RIGHT_PAREN, "Expected ')' after argument list");
	return argCount;
}

static void and_(bool canAssign) {
	int endJump = emitJump(OP_JUMP_IF_FALSE);
	emitByte(OP_POP);
	parsePrecedence(PREC_AND);

	patchJump(endJump);
}

static void or_(bool canAssign) {
	int elseJump = emitJump(OP_JUMP_IF_FALSE);
	int endJump = emitJump(OP_JUMP);

	patchJump(elseJump);
	emitByte(OP_POP);
	parsePrecedence(PREC_OR);
	patchJump(endJump);
}

static ObjectFunction *endCompiler() {
	emitReturn();
	ObjectFunction *function = current->function;
#ifdef DEBUG_PRINT_CODE
	if (!parser.hadError) {
		disassembleChunk(currentChunk(), function->name != NULL ? function->name->chars : "<script>");
	}
#endif
	current = current->enclosing;
	return function;
}

static void binary(bool canAssign) {
	TokenType operatorType = parser.previous.type;
	ParseRule *rule = getRule(operatorType);
	parsePrecedence(rule->precedence + 1);

	switch (operatorType) {
		case TOKEN_BANG_EQUAL:
			emitByte(OP_NOT_EQUAL);
			break;
		case TOKEN_EQUAL_EQUAL:
			emitByte(OP_EQUAL);
			break;
		case TOKEN_GREATER:
			emitByte(OP_GREATER);
			break;
		case TOKEN_GREATER_EQUAL:
			emitByte(OP_GREATER_EQUAL);
			break;
		case TOKEN_LESS:
			emitByte(OP_LESS);
			break;
		case TOKEN_LESS_EQUAL:
			emitByte(OP_LESS_EQUAL);
			break;
		case TOKEN_PLUS:
			emitByte(OP_ADD);
			break;
		case TOKEN_MINUS:
			emitByte(OP_SUBTRACT);
			break;
		case TOKEN_STAR:
			emitByte(OP_MULTIPLY);
			break;
		case TOKEN_SLASH:
			emitByte(OP_DIVIDE);
			break;
		case TOKEN_PERCENT:
			emitByte(OP_MODULUS);
			break;
		case TOKEN_RIGHT_SHIFT:
			emitByte(OP_RIGHT_SHIFT);
			break;
		case TOKEN_LEFT_SHIFT:
			emitByte(OP_LEFT_SHIFT);
			break;
		default:
			return; // unreachable
	}
}

static void call(bool canAssign) {
	uint8_t argCount = argumentList();
	emitBytes(OP_CALL, argCount);
}

static void literal(bool canAssign) {
	switch (parser.previous.type) {
		case TOKEN_FALSE:
			emitByte(OP_FALSE);
			break;
		case TOKEN_NIL:
			emitByte(OP_NIL);
			break;
		case TOKEN_TRUE:
			emitByte(OP_TRUE);
		default:
			return; // unreachable
	}
}

static void dot(bool canAssign) {
	consume(TOKEN_IDENTIFIER, "Expected property name after '.'.");
	uint8_t name = identifierConstant(&parser.previous);

	if (canAssign && match(TOKEN_EQUAL)) {
		expression();
		emitBytes(OP_SET_PROPERTY, name);
	} else if (match(TOKEN_LEFT_PAREN)) {
		uint8_t argCount = argumentList();
		emitBytes(OP_INVOKE, name);
		emitByte(argCount);
	} else {
		emitBytes(OP_GET_PROPERTY, name);
	}
}

static void expression() { parsePrecedence(PREC_ASSIGNMENT); }


static OpCode getCompoundOpcode(OpCode setOp, CompoundOp op) {
	switch (setOp) {
		case OP_SET_LOCAL:
			switch (op) {
				case COMPOUND_OP_PLUS:
					return OP_SET_LOCAL_PLUS;
				case COMPOUND_OP_MINUS:
					return OP_SET_LOCAL_MINUS;
				case COMPOUND_OP_STAR:
					return OP_SET_LOCAL_STAR;
				case COMPOUND_OP_SLASH:
					return OP_SET_LOCAL_SLASH;
			}
		case OP_SET_UPVALUE:
			switch (op) {
				case COMPOUND_OP_PLUS:
					return OP_SET_UPVALUE_PLUS;
				case COMPOUND_OP_MINUS:
					return OP_SET_UPVALUE_MINUS;
				case COMPOUND_OP_STAR:
					return OP_SET_UPVALUE_STAR;
				case COMPOUND_OP_SLASH:
					return OP_SET_UPVALUE_SLASH;
			}
		case OP_SET_GLOBAL:
			switch (op) {
				case COMPOUND_OP_PLUS:
					return OP_SET_GLOBAL_PLUS;
				case COMPOUND_OP_MINUS:
					return OP_SET_GLOBAL_MINUS;
				case COMPOUND_OP_STAR:
					return OP_SET_GLOBAL_STAR;
				case COMPOUND_OP_SLASH:
					return OP_SET_GLOBAL_SLASH;
			}
		default:
			return setOp; // Should never happen
	}
}

static void namedVariable(Token name, bool canAssign) {
	uint8_t getOp, setOp;
	int arg = resolveLocal(current, &name);

	if (arg != -1) {
		getOp = OP_GET_LOCAL;
		setOp = OP_SET_LOCAL;
	} else if ((arg = resolveUpvalue(current, &name)) != -1) {
		getOp = OP_GET_UPVALUE;
		setOp = OP_SET_UPVALUE;
	} else {
		arg = identifierConstant(&name);
		getOp = OP_GET_GLOBAL;
		setOp = OP_SET_GLOBAL;
	}

	if (canAssign) {
		if (match(TOKEN_EQUAL)) {
			expression();
			emitBytes(setOp, arg);
			return;
		}
		CompoundOp op;
		bool isCompoundAssignment = true;

		if (match(TOKEN_PLUS_EQUAL)) {
			op = COMPOUND_OP_PLUS;
		} else if (match(TOKEN_MINUS_EQUAL)) {
			op = COMPOUND_OP_MINUS;
		} else if (match(TOKEN_STAR_EQUAL)) {
			op = COMPOUND_OP_STAR;
		} else if (match(TOKEN_SLASH_EQUAL)) {
			op = COMPOUND_OP_SLASH;
		} else {
			isCompoundAssignment = false;
		}

		if (isCompoundAssignment) {
			expression();
			emitBytes(getCompoundOpcode(setOp, op), arg);
			return;
		}
	}
	emitBytes(getOp, arg);
}

static void variable(bool canAssign) { namedVariable(parser.previous, canAssign); }

static Token syntheticToken(const char *text) {
	Token token;
	token.start = text;
	token.length = strlen(text);
	return token;
}

static void super_(bool canAssign) {
	if (currentClass == NULL) {
		error("Cannot use 'super' outside of a class");
	} else if (!currentClass->hasSuperclass) {
		error("Cannot use 'super' in a class that does not have a superclass");
	}

	consume(TOKEN_DOT, "Expected '.' after 'super'.");
	consume(TOKEN_IDENTIFIER, "Expected superclass method name.");
	uint8_t name = identifierConstant(&parser.previous);
	namedVariable(syntheticToken("self"), false);
	namedVariable(syntheticToken("super"), false);

	if (match(TOKEN_LEFT_PAREN)) {
		uint8_t argCount = argumentList();
		namedVariable(syntheticToken("super"), false);
		emitBytes(OP_SUPER_INVOKE, name);
		emitByte(argCount);
	} else {
		namedVariable(syntheticToken("super"), false);
		emitBytes(OP_GET_SUPER, name);
	}
}

static void block() {
	while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
		declaration();
	}

	consume(TOKEN_RIGHT_BRACE, "Expected '}' after block");
}

static void function(FunctionType type) {
	Compiler compiler;
	initCompiler(&compiler, type);
	beginScope();

	consume(TOKEN_LEFT_PAREN, "Expect '(' after function name.");

	if (!check(TOKEN_RIGHT_PAREN)) {
		do {
			current->function->arity++;
			if (current->function->arity > 255) {
				errorAtCurrent("Functions cannot have more than 255 arguments");
			}
			uint8_t constant = parseVariable("Expected parameter name");
			defineVariable(constant);
		} while (match(TOKEN_COMMA));
	}

	consume(TOKEN_RIGHT_PAREN, "Expect ')' after parameters.");
	consume(TOKEN_LEFT_BRACE, "Expect '{' before function body.");
	block();

	ObjectFunction *function = endCompiler();
	emitBytes(OP_CLOSURE, makeConstant(OBJECT_VAL(function)));

	for (int i = 0; i < function->upvalueCount; i++) {
		emitByte(compiler.upvalues[i].isLocal ? 1 : 0);
		emitByte(compiler.upvalues[i].index);
	}
}

static void method() {
	consume(TOKEN_FN, "Expected 'fn' to start a method declaration.");
	consume(TOKEN_IDENTIFIER, "Expected method name.");
	uint8_t constant = identifierConstant(&parser.previous);

	FunctionType type = TYPE_METHOD;

	if (parser.previous.length == 4 && memcmp(parser.previous.start, "init", 4) == 0) {
		type = TYPE_INITIALIZER;
	}

	function(type);

	emitBytes(OP_METHOD, constant);
}

static void classDeclaration() {
	consume(TOKEN_IDENTIFIER, "Expected class name");
	Token className = parser.previous;
	uint8_t nameConstant = identifierConstant(&parser.previous);
	declareVariable();

	emitBytes(OP_CLASS, nameConstant);
	defineVariable(nameConstant);

	ClassCompiler classCompiler;
	classCompiler.enclosing = currentClass;
	classCompiler.hasSuperclass = false;
	currentClass = &classCompiler;

	if (match(TOKEN_LESS)) {
		consume(TOKEN_IDENTIFIER, "Expected super class name after '<'.");
		variable(false);

		if (identifiersEqual(&className, &parser.previous)) {
			error("A class cannot inherit from itself");
		}

		beginScope();
		addLocal(syntheticToken("super"));
		defineVariable(0);

		namedVariable(className, false);
		emitByte(OP_INHERIT);
		classCompiler.hasSuperclass = true;
	}

	namedVariable(className, false);

	consume(TOKEN_LEFT_BRACE, "Expected '{' before class body");

	while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
		method();
	}

	consume(TOKEN_RIGHT_BRACE, "Expected '}' after class body");
	emitByte(OP_POP);

	if (classCompiler.hasSuperclass) {
		endScope();
	}

	currentClass = classCompiler.enclosing;
}

static void fnDeclaration() {
	uint8_t global = parseVariable("Expected function name");
	markInitialized();
	function(TYPE_FUNCTION);
	defineVariable(global);
}

static void tableLiteral(bool canAssign) {
	uint16_t elementCount = 0;
	if (!match(TOKEN_RIGHT_BRACE)) {
		do {
			expression();
			if (elementCount >= UINT16_MAX) {
				error("Too many elements in table declaration.");
			}
			consume(TOKEN_COLON, "Expected ':' after table key.");
			expression();
			elementCount++;
		}while (match(TOKEN_COMMA));
		consume(TOKEN_RIGHT_BRACE, "Expected '}' after table elements.");
	}
	emitByte(OP_TABLE);
	emitBytes(elementCount >> 8 & 0xff, elementCount & 0xff);
}

static void arrayLiteral(bool canAssign) {
	uint16_t elementCount = 0;

	if (!match(TOKEN_RIGHT_SQUARE)) {
		do {
			expression();
			if (elementCount >= UINT16_MAX) {
				error("Too many elements in array declaration");
			}
			elementCount++;
		} while (match(TOKEN_COMMA));
		consume(TOKEN_RIGHT_SQUARE, "Expected ']' after array elements");
	}
	emitByte(OP_ARRAY);
	emitBytes(elementCount >> 8 & 0xff, elementCount & 0xff);
}

static void arrayIndex(bool canAssign) {
	expression(); // array index
	consume(TOKEN_RIGHT_SQUARE, "Expected ']' after array index");

	if (match(TOKEN_EQUAL)) {
		expression();
		emitByte(OP_SET_ARRAY);
	} else {
		emitByte(OP_GET_ARRAY);
	}
}

static void varDeclaration() {
	uint8_t global = parseVariable("Expected Variable Name.");

	if (match(TOKEN_EQUAL)) {
		expression();
	} else {
		emitByte(OP_NIL);
	}
	consume(TOKEN_SEMICOLON, "Expected ';' after variable declaration.");
	defineVariable(global);
}

static void expressionStatement() {
	expression();
	consume(TOKEN_SEMICOLON, "Expected ';' after expression");
	emitByte(OP_POP);
}

static void breakStatement() {
	if (current->scopeDepth == 0) {
		error("'break' cannot be used outside of a loop.");
		return;
	}
	consume(TOKEN_SEMICOLON, "Expected ';' after break statement.");
	loopStack[current->loopDepth - 1].exitJump = emitJump(OP_JUMP);
}

static void continueStatement() {
	if (current->loopDepth == 0) {
		error("'continue' cannot be used outside of a loop.");
		return;
	}
	consume(TOKEN_SEMICOLON, "Expected ';' after continue statement.");
	emitLoop(loopStack[current->loopDepth - 1].loopStart);
}

static void whileStatement() {
	int loopStart = currentChunk()->count;
	expression();
	int exitJump = emitJump(OP_JUMP_IF_FALSE);
	emitByte(OP_POP);

	loopStack[current->loopDepth].loopStart = loopStart;
	loopStack[current->loopDepth].exitJump = -1;
	current->loopDepth++;

	statement();

	emitLoop(loopStart);

	patchJump(exitJump);
	emitByte(OP_POP);

	// patching breaks
	if (loopStack[current->loopDepth - 1].exitJump != -1) {
		patchJump(loopStack[current->loopDepth - 1].exitJump);
	}

	current->loopDepth--;
}

static void forStatement() {
	beginScope();

	if (match(TOKEN_SEMICOLON)) {
		// no initializer
	} else if (match(TOKEN_LET)) {
		varDeclaration();
	} else {
		expressionStatement();
	}
	int loopStart = currentChunk()->count;
	int exitJump = -1;
	if (!match(TOKEN_SEMICOLON)) {
		expression();
		consume(TOKEN_SEMICOLON, "Expected ';' after loop condition");

		// Jump out of the loop if the condition is false
		exitJump = emitJump(OP_JUMP_IF_FALSE);
		emitByte(OP_POP); // condition
	}

	// check for right paren here
	if (!match(TOKEN_RIGHT_PAREN)) {
		int bodyJump = emitJump(OP_JUMP);
		int incrementStart = currentChunk()->count;
		expression();
		emitByte(OP_POP);

		emitLoop(loopStart); // main loop that takes us bak to the top of the for loop
		loopStart = incrementStart;
		patchJump(bodyJump);
	}

	loopStack[current->loopDepth].loopStart = loopStart;
	loopStack[current->loopDepth].exitJump = exitJump;
	current->loopDepth++;

	statement();
	emitLoop(loopStart);

	if (exitJump != -1) {
		patchJump(exitJump);
		emitByte(OP_POP);
	}

	// patching breaks
	if (loopStack[current->loopDepth - 1].exitJump != -1) {
		patchJump(loopStack[current->loopDepth - 1].exitJump);
	}

	current->loopDepth--;
	endScope();
}

static void ifStatement() {
	expression();
	int thenJump = emitJump(OP_JUMP_IF_FALSE);
	emitByte(OP_POP);
	statement();

	int elseJump = emitJump(OP_JUMP);
	patchJump(thenJump);
	emitByte(OP_POP);

	if (match(TOKEN_ELSE))
		statement();
	patchJump(elseJump);
}

static void returnStatement() {
	if (current->type == TYPE_SCRIPT) {
		error("Cannot use <return> outside of a function");
	}

	if (match(TOKEN_SEMICOLON)) {
		emitReturn();
	} else {
		if (current->type == TYPE_INITIALIZER) {
			error("Cannot return a value from an 'init' function");
		}
	}
	expression();
	consume(TOKEN_SEMICOLON, "Expected ';' after return value");
	emitByte(OP_RETURN);
}

static void synchronize() {
	parser.panicMode = false;

	while (parser.current.type != TOKEN_EOF) {
		if (parser.previous.type == TOKEN_SEMICOLON)
			return;
		switch (parser.current.type) {
			case TOKEN_CLASS:
			case TOKEN_FN:
			case TOKEN_LET:
			case TOKEN_FOR:
			case TOKEN_IF:
			case TOKEN_WHILE:
			case TOKEN_RETURN:
				return;
			default:;
		}
		advance();
	}
}

static void declaration() {
	if (match(TOKEN_LET)) {
		varDeclaration();
	} else if (match(TOKEN_CLASS)) {
		classDeclaration();
	} else if (match(TOKEN_FN)) {
		fnDeclaration();
	} else {
		statement();
	}

	if (parser.panicMode)
		synchronize();
}

static void statement() {
	if (match(TOKEN_IF)) {
		ifStatement();
	} else if (match(TOKEN_LEFT_BRACE)) {
		beginScope();
		block();
		endScope();
	} else if (match(TOKEN_WHILE)) {
		whileStatement();
	} else if (match(TOKEN_FOR)) {
		forStatement();
	} else if (match(TOKEN_BREAK)) {
		breakStatement();
	} else if (match(TOKEN_CONTINUE)) {
		continueStatement();
	} else if (match(TOKEN_RETURN)) {
		returnStatement();
	} else {
		expressionStatement();
	}
}

static void grouping(bool canAssign) {
	expression();
	consume(TOKEN_RIGHT_PAREN, "Expected ')' after expression.");
}

static void number(bool canAssign) {
	double value = strtod(parser.previous.start, NULL);
	emitConstant(NUMBER_VAL(value));
}

static void string(bool canAssign) {
	// +1 -2 trims the leading and trailing quotation marks
	emitConstant(OBJECT_VAL(copyString(parser.previous.start + 1, parser.previous.length - 2)));
	// TODO: translate string escape sequences
}


static void self() {
	if (currentClass == NULL) {
		error("'self' cannot be used outside of a class.");
		return;
	}

	variable(false);
}

static void unary(bool canAssign) {
	TokenType operatorType = parser.previous.type;

	// compile the operand
	parsePrecedence(PREC_UNARY);

	switch (operatorType) {
		case TOKEN_NOT:
			emitByte(OP_NOT);
			break;
		case TOKEN_MINUS:
			emitByte(OP_NEGATE);
			break;
		default:
			return; // unreachable
	}
}

// Prefix Infix Precedence
ParseRule rules[] = {
		[TOKEN_LEFT_PAREN] = {grouping, call, PREC_CALL},
		[TOKEN_RIGHT_PAREN] = {NULL, NULL, PREC_NONE},
		[TOKEN_LEFT_BRACE] = {tableLiteral, NULL, PREC_NONE},
		[TOKEN_RIGHT_BRACE] = {NULL, NULL, PREC_NONE},
		[TOKEN_LEFT_SQUARE] = {arrayLiteral, arrayIndex, PREC_CALL},
		[TOKEN_RIGHT_SQUARE] = {NULL, NULL, PREC_NONE},
		[TOKEN_COMMA] = {NULL, NULL, PREC_NONE},
		[TOKEN_DOT] = {NULL, dot, PREC_CALL},
		[TOKEN_MINUS] = {unary, binary, PREC_TERM},
		[TOKEN_PLUS] = {NULL, binary, PREC_TERM},
		[TOKEN_SEMICOLON] = {NULL, NULL, PREC_NONE},
		[TOKEN_SLASH] = {NULL, binary, PREC_FACTOR},
		[TOKEN_STAR] = {NULL, binary, PREC_FACTOR},
		[TOKEN_PERCENT] = {NULL, binary, PREC_FACTOR},
		[TOKEN_LEFT_SHIFT] = {NULL, binary, PREC_SHIFT},
		[TOKEN_RIGHT_SHIFT] = {NULL, binary, PREC_SHIFT},
		[TOKEN_NOT] = {unary, NULL, PREC_NONE},
		[TOKEN_BANG_EQUAL] = {NULL, binary, PREC_EQUALITY},
		[TOKEN_EQUAL] = {NULL, NULL, PREC_NONE},
		[TOKEN_EQUAL_EQUAL] = {NULL, binary, PREC_EQUALITY},
		[TOKEN_GREATER] = {NULL, binary, PREC_COMPARISON},
		[TOKEN_GREATER_EQUAL] = {NULL, binary, PREC_COMPARISON},
		[TOKEN_LESS] = {NULL, binary, PREC_COMPARISON},
		[TOKEN_LESS_EQUAL] = {NULL, binary, PREC_COMPARISON},
		[TOKEN_IDENTIFIER] = {variable, NULL, PREC_NONE},
		[TOKEN_STRING] = {string, NULL, PREC_NONE},
		[TOKEN_INT] = {number, NULL, PREC_NONE},
		[TOKEN_FLOAT] = {number, NULL, PREC_NONE},
		[TOKEN_CONTINUE] = {NULL, NULL, PREC_NONE},
		[TOKEN_BREAK] = {NULL, NULL, PREC_NONE},
		[TOKEN_AND] = {NULL, and_, PREC_AND},
		[TOKEN_CLASS] = {NULL, NULL, PREC_NONE},
		[TOKEN_ELSE] = {NULL, NULL, PREC_NONE},
		[TOKEN_FALSE] = {literal, NULL, PREC_NONE},
		[TOKEN_FOR] = {NULL, NULL, PREC_NONE},
		[TOKEN_FN] = {NULL, NULL, PREC_NONE},
		[TOKEN_IF] = {NULL, NULL, PREC_NONE},
		[TOKEN_NIL] = {literal, NULL, PREC_NONE},
		[TOKEN_OR] = {NULL, or_, PREC_OR},
		[TOKEN_RETURN] = {NULL, NULL, PREC_NONE},
		[TOKEN_SUPER] = {super_, NULL, PREC_NONE},
		[TOKEN_SELF] = {self, NULL, PREC_NONE},
		[TOKEN_TRUE] = {literal, NULL, PREC_NONE},
		[TOKEN_LET] = {NULL, NULL, PREC_NONE},
		[TOKEN_WHILE] = {NULL, NULL, PREC_NONE},
		[TOKEN_ERROR] = {NULL, NULL, PREC_NONE},
		[TOKEN_EOF] = {NULL, NULL, PREC_NONE},
};

/**
starts at the current token and parses any expression at the given precedence or
higher
*/
static void parsePrecedence(Precedence precedence) {
	advance();
	ParseFn prefixRule = getRule(parser.previous.type)->prefix;
	if (prefixRule == NULL) {
		error("Expected expression.");
		return;
	}

	bool canAssign = precedence <= PREC_ASSIGNMENT;
	prefixRule(canAssign);

	while (precedence <= getRule(parser.current.type)->precedence) {
		advance();
		ParseFn infixRule = getRule(parser.previous.type)->infix;
		infixRule(canAssign);
	}

	if (canAssign && match(TOKEN_EQUAL)) {
		error("Invalid Assignment Target");
	}
}

static ParseRule *getRule(TokenType type) {
	return &rules[type]; // Returns the rule at the given index
}

ObjectFunction *compile(const char *source) {
	initScanner(source);
	Compiler compiler;
	initCompiler(&compiler, TYPE_SCRIPT);

	parser.hadError = false;
	parser.panicMode = false;

	advance();

	while (!match(TOKEN_EOF)) {
		declaration();
	}

	ObjectFunction *function = endCompiler();
	return parser.hadError ? NULL : function;
}

void markCompilerRoots() {
	Compiler *compiler = current;
	while (compiler != NULL) {
		markObject((Object *) compiler->function);
		compiler = (Compiler *) compiler->enclosing;
	}
}
