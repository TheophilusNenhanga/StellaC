#include "compiler.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "chunk.h"
#include "memory.h"
#include "object.h"
#include "panic.h"

#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif

#ifdef DUMP_BYTECODE
#include "debug.h"
#endif


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

static void advance() {
	parser.previous = parser.current;
	for (;;) {
		parser.current = scanToken();
		if (parser.current.type != TOKEN_ERROR)
			break;
		compilerPanic(&parser, parser.current.start, SYNTAX);
	}
}

static void consume(TokenType type, const char *message) {
	if (parser.current.type == type) {
		advance();
		return;
	}
	compilerPanic(&parser, message, SYNTAX);
}

static bool check(TokenType type) { return parser.current.type == type; }

static bool match(TokenType type) {
	if (!check(type))
		return false;
	advance();
	return true;
}

static void emitByte(uint8_t byte) { writeChunk(current->owner, currentChunk(), byte, parser.previous.line); }

static void emitBytes(uint8_t byte1, uint8_t byte2) {
	emitByte(byte1);
	emitByte(byte2);
}

/**
 * emits an OP_LOOP instruction
 * @param loopStart The starting point of the loop
 */
static void emitLoop(int loopStart) {
	emitByte(OP_LOOP);
	int offset = currentChunk()->count - loopStart + 2; // +2 takes into account the size of the OP_LOOP
	if (offset > UINT16_MAX) {
		compilerPanic(&parser, "Loop body too large.", LOOP_EXTENT);
	}
	emitBytes(((offset >> 8) & 0xff), (offset & 0xff));
}

/**
 * Emits a jump instruction and placeholders for the jump offset.
 *
 * @param instruction The opcode for the jump instruction (e.g., OP_JUMP, OP_JUMP_IF_FALSE).
 * @return The index of the jump instruction in the bytecode, used for patching later.
 */
static int emitJump(uint8_t instruction) {
	emitByte(instruction);
	emitBytes(0xff, 0xff);
	return currentChunk()->count - 2;
}

/**
 * Patches a jump instruction with the calculated offset.
 *
 * @param offset The index of the jump instruction in the bytecode to patch.
 */
static void patchJump(int offset) {
	// -2 to adjust for the bytecode for the jump offset itself
	int jump = currentChunk()->count - offset - 2;
	if (jump > UINT16_MAX) {
		compilerPanic(&parser, "Too much code to jump over.", BRANCH_EXTENT);
	}
	currentChunk()->code[offset] = (jump >> 8) & 0xff;
	currentChunk()->code[offset + 1] = jump & 0xff;
}

/**
 * Emits an instruction that signals the end of a scope.
 * Depending on the scope one of these three OP CODES can be emitted:
 * OP_GET_LOCAL 0,
 * OP_NIL,
 * OP_RETURN 0
 */
static void emitReturn() {
	if (current->type == TYPE_INITIALIZER) {
		emitBytes(OP_GET_LOCAL, 0);
	} else {
		emitByte(OP_NIL);
	}
	emitByte(OP_RETURN);
}

/**
 * Creates a constant value and adds it to the current chunk's constant pool.
 *
 * @param value The value to add as a constant.
 * @return The index of the constant in the constant pool.
 */
static uint8_t makeConstant(Value value) {
	int constant = addConstant(current->owner, currentChunk(), value);
	// Add constant adds the given value to the end of the constant table and
	// returns the index.
	if (constant > UINT8_MAX) {
		// Only 256 constants can be stored and loaded in a chunk.
		// TODO: Add new instruction 'OP_CONSTANT_16' to store two byte operands
		// that can handle more constants when needed
		compilerPanic(&parser, "Too many constants in one chunk.", LIMIT);
		return 0;
	}
	return (uint8_t) constant;
}

/**
 * Emits an OP_CONSTANT instruction and its operand (the constant index).
 *
 * @param value The constant value to emit.
 */
static void emitConstant(Value value) { emitBytes(OP_CONSTANT, makeConstant(value)); }


/**
 * Initializes a compiler.
 *
 * Sets up the compiler state for compiling a new function (or script).
 *
 * @param compiler The compiler to initialize.
 * @param type The type of function being compiled (e.g., TYPE_FUNCTION, TYPE_SCRIPT).
 * @param vm The virtual machine instance.
 */
static void initCompiler(Compiler *compiler, FunctionType type, VM *vm) {
	compiler->enclosing = current;
	compiler->function = NULL;
	compiler->type = type;
	compiler->localCount = 0;
	compiler->scopeDepth = 0;
	compiler->matchDepth = 0;
	compiler->owner = vm;

	compiler->function = newFunction(compiler->owner);
	current = compiler;

	if (type == TYPE_ANONYMOUS) {
		current->function->name = copyString(current->owner, "anonymous", 9);
	} else if (type != TYPE_SCRIPT) {
		current->function->name = copyString(current->owner, parser.previous.start, parser.previous.length);
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

/**
 * Creates a constant for an identifier token.
 *
 * @param name The token representing the identifier.
 * @return The index of the identifier constant in the constant pool.
 */
static uint8_t identifierConstant(Token *name) {
	return makeConstant(OBJECT_VAL(copyString(current->owner, name->start, name->length)));
}

/**
 * Begins a new scope.
 *
 * Increases the scope depth, indicating that variables declared subsequently are in a new, inner scope.
 */
static void beginScope() { current->scopeDepth++; }

/**
 * Ends the current scope.
 *
 * Decreases the scope depth and emits OP_POP instructions to remove local variables that go out of scope.
 */
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

/**
 * Checks if two identifier tokens are equal.
 *
 * @param a The first token.
 * @param b The second token.
 * @return true if the tokens represent the same identifier, false otherwise.
 */
static bool identifiersEqual(Token *a, Token *b) {
	if (a->length != b->length)
		return false;
	return memcmp(a->start, b->start, a->length) == 0;
}

/**
 * Resolves a local variable in the current compiler's scope.
 *
 * Searches the current compiler's local variables for a variable with the given name.
 *
 * @param compiler The compiler whose scope to search.
 * @param name The token representing the variable name.
 * @return The index of the local variable if found, -1 otherwise.
 */
static int resolveLocal(Compiler *compiler, Token *name) {
	for (int i = compiler->localCount - 1; i >= 0; i--) {
		Local *local = &compiler->locals[i];
		if (identifiersEqual(name, &local->name)) {
			if (local->depth == -1) {
				compilerPanic(&parser, "Cannot read local variable in its own initializer", NAME);
			}
			return i;
		}
	}
	return -1;
}

/**
 * Adds an upvalue to the current function.
 *
 * An upvalue is a variable captured from an enclosing function's scope.
 *
 * @param compiler The current compiler.
 * @param index The index of the local variable in the enclosing function, or the index of the upvalue.
 * @param isLocal true if the upvalue is a local variable in the enclosing function, false if it's an upvalue itself.
 * @return The index of the added upvalue in the current function's upvalue array.
 */
static int addUpvalue(Compiler *compiler, uint8_t index, bool isLocal) {
	int upvalueCount = compiler->function->upvalueCount;

	for (int i = 0; i < upvalueCount; i++) {
		Upvalue *upvalue = &compiler->upvalues[i];
		if (upvalue->index == index && upvalue->isLocal == isLocal) {
			return i;
		}
	}

	if (upvalueCount >= UINT8_COUNT) {
		compilerPanic(&parser, "Too many closure variables in function.", CLOSURE_EXTENT);
		return 0;
	}

	compiler->upvalues[upvalueCount].isLocal = isLocal;
	compiler->upvalues[upvalueCount].index = index;
	return compiler->function->upvalueCount++;
}

/**
 * Resolves an upvalue in enclosing scopes.
 *
 * Searches enclosing compiler scopes for a local variable or upvalue with the given name.
 *
 * @param compiler The current compiler.
 * @param name The token representing the variable name.
 * @return The index of the resolved upvalue if found, -1 otherwise.
 */
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

/**
 * Adds a local variable to the current scope.
 *
 * @param name The token representing the name of the local variable.
 */
static void addLocal(Token name) {
	if (current->localCount == UINT8_COUNT) {
		compilerPanic(&parser, "Too many local variables in function.", LOCAL_EXTENT);
		return;
	}

	Local *local = &current->locals[current->localCount++];
	local->name = name;
	local->depth = -1;
	local->isCaptured = false;
}

/**
 * Declares a variable in the current scope.
 *
 * Adds the variable name to the list of local variables, but does not mark it as initialized yet.
 */
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
			compilerPanic(&parser, "Cannot redefine variable in the same scope", NAME);
		}
	}

	addLocal(*name);
}

/**
 * Marks the most recently declared local variable as initialized.
 *
 * This prevents reading a local variable before it has been assigned a value.
 */
static void markInitialized() {
	if (current->scopeDepth == 0)
		return;
	current->locals[current->localCount - 1].depth = current->scopeDepth;
}

/**
 * Parses a variable name and returns its constant pool index.
 *
 * @param errorMessage The error message to display if an identifier is not found.
 * @return The index of the variable name constant in the constant pool.
 */
static uint8_t parseVariable(const char *errorMessage) {
	consume(TOKEN_IDENTIFIER, errorMessage);
	declareVariable();
	if (current->scopeDepth > 0)
		return 0;
	return identifierConstant(&parser.previous);
}

/**
 * Defines a variable, emitting the bytecode to store its value.
 *
 * If the variable is global, emits OP_DEFINE_GLOBAL. Otherwise, it's a local variable and no bytecode is emitted at definition time
 * (local variables are implicitly defined when they are declared and initialized).
 *
 * @param global The index of the variable name constant in the constant pool (for global variables).
 */
static void defineVariable(uint8_t global) {
	if (current->scopeDepth > 0) {
		markInitialized();
		return;
	}
	emitBytes(OP_DEFINE_GLOBAL, global);
}

/**
 * Parses an argument list for a function call.
 *
 * @return The number of arguments parsed.
 */
static uint8_t argumentList() {
	uint8_t argCount = 0;
	if (!check(TOKEN_RIGHT_PAREN)) {
		do {
			expression();
			if (argCount == 255) {
				compilerPanic(&parser, "Cannot have more than 255 arguments.", ARGUMENT_EXTENT);
			}
			argCount++;
		} while (match(TOKEN_COMMA));
	}
	consume(TOKEN_RIGHT_PAREN, "Expected ')' after argument list");
	return argCount;
}

/**
 * Parses the 'and' operator. Implemented using short-circuiting.
 *
 * @param canAssign Whether the 'and' expression can be the target of an assignment.
 */
static void and_(bool canAssign) {
	int endJump = emitJump(OP_JUMP_IF_FALSE);
	emitByte(OP_POP);
	parsePrecedence(PREC_AND);

	patchJump(endJump);
}

/**
 * Parses the 'or' operator. Implemented using short-circuiting.
 *
 * @param canAssign Whether the 'or' expression can be the target of an assignment.
 */
static void or_(bool canAssign) {
	int elseJump = emitJump(OP_JUMP_IF_FALSE);
	int endJump = emitJump(OP_JUMP);

	patchJump(elseJump);
	emitByte(OP_POP);
	parsePrecedence(PREC_OR);
	patchJump(endJump);
}

/**
 * Finishes compilation and returns the compiled function.
 *
 * Emits the final OP_RETURN instruction, frees the compiler state, and returns the compiled function object.
 *
 * @return The compiled function object.
 */
static ObjectFunction *endCompiler() {
	emitReturn();
	ObjectFunction *function = current->function;
#ifdef DEBUG_PRINT_CODE
	if (!parser.hadError) {
		disassembleChunk(currentChunk(), function->name != NULL ? function->name->chars : "<script>");
	}
#endif

#ifdef DUMP_BYTECODE
	if (!parser.hadError) {
		char filename[256];
		const char* name = function->name != NULL ? function->name->chars : "script";
		snprintf(filename, sizeof(filename), "%s.cruxbc", name);
		FILE* file = fopen(filename, "w");
		if (file != NULL) {
			dumpBytecodeToFile(currentChunk(), name, file);
			fclose(file);
			printf("Bytecode dumped to %s\n", filename);
		} else {
			fprintf(stderr, "Could not open file %s for bytecode dump\n", filename);
		}
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
		case TOKEN_BACKSLASH:
			emitByte(OP_INT_DIVIDE);
			break;
		case TOKEN_STAR_STAR:
			emitByte(OP_POWER);
			break;

		default:
			return; // unreachable
	}
}

static void call(bool canAssign) {
	uint8_t argCount = argumentList();
	emitBytes(OP_CALL, argCount);
}

/**
 * Parses a literal boolean or nil value.
 *
 * @param canAssign Whether the literal can be the target of an assignment (always false).
 */
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
			break;
		default:
			return; // unreachable
	}
}

/**
 * Parses a dot (.) property access expression.
 *
 * @param canAssign Whether the dot expression can be the target of an assignment.
 */
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

/**
 * Parses an expression. Entry point for expression parsing.
 */
static void expression() { parsePrecedence(PREC_ASSIGNMENT); }

/**
 * Gets the compound opcode based on the set opcode and compound operation.
 *
 * This helper function is used for compound assignments (e.g., +=, -=, *=, /=).
 *
 * @param setOp The base set opcode (e.g., OP_SET_LOCAL, OP_SET_GLOBAL).
 * @param op The compound operation (e.g., COMPOUND_OP_PLUS, COMPOUND_OP_MINUS).
 * @return The corresponding compound opcode.
 */
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
				case COMPOUND_OP_BACK_SLASH:
					return OP_SET_LOCAL_INT_DIVIDE;
				case COMPOUND_OP_PERCENT:
					return OP_SET_LOCAL_MODULUS;
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
				case COMPOUND_OP_BACK_SLASH:
					return OP_SET_UPVALUE_INT_DIVIDE;
				case COMPOUND_OP_PERCENT:
					return OP_SET_UPVALUE_MODULUS;
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
				case COMPOUND_OP_BACK_SLASH:
					return OP_SET_GLOBAL_INT_DIVIDE;
				case COMPOUND_OP_PERCENT:
					return OP_SET_GLOBAL_MODULUS;
			}
		default:
			return setOp; // Should never happen
	}
}

/**
 * Parses a named variable (local, upvalue, or global).
 *
 * @param name The token representing the variable name.
 * @param canAssign Whether the variable expression can be the target of an assignment.
 */
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
		}else if (match(TOKEN_BACK_SLASH_EQUAL)) {
			op = COMPOUND_OP_BACK_SLASH;
		}else if (match(TOKEN_PERCENT_EQUAL)) {
			op = COMPOUND_OP_PERCENT;
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

/**
 * Creates a synthetic token for internal compiler use.
 *
 * @param text The text of the synthetic token.
 * @return The created synthetic token.
 */
static Token syntheticToken(const char *text) {
	Token token;
	token.start = text;
	token.length = (int) strlen(text);
	return token;
}

static void super_(bool canAssign) {
	if (currentClass == NULL) {
		compilerPanic(&parser, "Cannot use 'super' outside of a class", NAME);
	} else if (!currentClass->hasSuperclass) {
		compilerPanic(&parser, "Cannot use 'super' in a class that does not have a superclass", NAME);
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
	initCompiler(&compiler, type, current->owner);
	beginScope();

	consume(TOKEN_LEFT_PAREN, "Expect '(' after function name.");

	if (!check(TOKEN_RIGHT_PAREN)) {
		do {
			current->function->arity++;
			if (current->function->arity > 255) {
				compilerPanic(&parser, "Functions cannot have more than 255 arguments", ARGUMENT_EXTENT);
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

	if (match(TOKEN_COLON)) {
		consume(TOKEN_IDENTIFIER, "Expected super class name after ':'.");
		variable(false);

		if (identifiersEqual(&className, &parser.previous)) {
			compilerPanic(&parser, "A class cannot inherit from itself", NAME);
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


static void anonymousFunction(bool canAssign) {
	Compiler compiler;
	initCompiler(&compiler, TYPE_ANONYMOUS, current->owner);
	beginScope();
	consume(TOKEN_LEFT_PAREN, "Expected '(' to start argument list");
	if (!check(TOKEN_RIGHT_PAREN)) {
		do {
			current->function->arity++;
			if (current->function->arity > 255) {
				compilerPanic(&parser, "Functions cannot have more than 255 arguments", ARGUMENT_EXTENT);
			}
			uint8_t constant = parseVariable("Expected parameter name");
			defineVariable(constant);
		} while (match(TOKEN_COMMA));
	}
	consume(TOKEN_RIGHT_PAREN, "Expected ')' after argument list");
	consume(TOKEN_LEFT_BRACE, "Expected '{' before function body");
	block();
	ObjectFunction *function = endCompiler();
	emitBytes(OP_ANON_FUNCTION, makeConstant(OBJECT_VAL(function)));

	for (int i = 0; i < function->upvalueCount; i++) {
		emitByte(compiler.upvalues[i].isLocal ? 1 : 0);
		emitByte(compiler.upvalues[i].index);
	}
}

static void arrayLiteral(bool canAssign) {
	uint16_t elementCount = 0;

	if (!match(TOKEN_RIGHT_SQUARE)) {
		do {
			expression();
			if (elementCount >= UINT16_MAX) {
				compilerPanic(&parser, "Too many elements in array literal", COLLECTION_EXTENT);
			}
			elementCount++;
		} while (match(TOKEN_COMMA));
		consume(TOKEN_RIGHT_SQUARE, "Expected ']' after array elements");
	}
	emitByte(OP_ARRAY);
	emitBytes(((elementCount >> 8) & 0xff), (elementCount & 0xff));
}

static void tableLiteral(bool canAssign) {
	uint16_t elementCount = 0;

	if (!match(TOKEN_RIGHT_BRACE)) {
		do {
			expression();
			consume(TOKEN_COLON, "Expected ':' after <table> key");
			expression();
			if (elementCount >= UINT16_MAX) {
				compilerPanic(&parser, "Too many elements in table literal", COLLECTION_EXTENT);
			}
			elementCount++;
		} while (match(TOKEN_COMMA));
		consume(TOKEN_RIGHT_BRACE, "Expected '}' after table elements");
	}
	emitByte(OP_TABLE);
	emitBytes(((elementCount >> 8) & 0xff), (elementCount & 0xff));
}

/**
 * Parses a collection index access expression (e.g., array[index]).
 *
 * @param canAssign Whether the collection index expression can be the target of an assignment.
 */
static void collectionIndex(bool canAssign) {
	expression();
	consume(TOKEN_RIGHT_SQUARE, "Expected ']' after array index");

	if (canAssign && match(TOKEN_EQUAL)) {
		expression();
		emitByte(OP_SET_COLLECTION);
	} else {
		emitByte(OP_GET_COLLECTION);
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

static void whileStatement() {
	beginScope();
	int loopStart = currentChunk()->count;
	expression();
	int exitJump = emitJump(OP_JUMP_IF_FALSE);
	emitByte(OP_POP);

	statement();

	emitLoop(loopStart);

	patchJump(exitJump);
	emitByte(OP_POP);

	endScope();
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

	int bodyJump = emitJump(OP_JUMP);
	int incrementStart = currentChunk()->count;
	expression();
	emitByte(OP_POP);

	emitLoop(loopStart); // main loop that takes us back to the top of the for loop
	loopStart = incrementStart;
	patchJump(bodyJump);

	statement();
	emitLoop(loopStart);

	if (exitJump != -1) {
		patchJump(exitJump);
		emitByte(OP_POP);
	}

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
		compilerPanic(&parser, "Cannot use <return> outside of a function", SYNTAX);
	}

	if (match(TOKEN_SEMICOLON)) {
		emitReturn();
	} else {
		if (current->type == TYPE_INITIALIZER) {
			compilerPanic(&parser, "Cannot return a value from an 'init' function", SYNTAX);
		}
		expression();
		consume(TOKEN_SEMICOLON, "Expected ';' after return value");
		emitByte(OP_RETURN);
	}
}

static void useStatement() {
	bool hasParen = false;
	if (parser.current.type == TOKEN_LEFT_PAREN) {
		consume(TOKEN_LEFT_PAREN, "Expected '(' after use statement");
		hasParen = true;
	}

	uint8_t nameCount = 0;
	uint8_t names[UINT8_MAX];
	uint8_t aliases[UINT8_MAX];
	bool aliasPresence[UINT8_MAX];
	do {
		if (nameCount >= UINT8_MAX) {
			compilerPanic(&parser, "Cannot import more than 255 names from another module.", IMPORT_EXTENT);
		}
		consume(TOKEN_IDENTIFIER, "Expected name to import from external module");

		uint8_t name;
		if (parser.current.type == TOKEN_AS) {
			name = identifierConstant(&parser.previous);
			consume(TOKEN_AS, "Expected 'as' keyword.");
			consume(TOKEN_IDENTIFIER, "Expected name to alias import from external module.");
			uint8_t alias = identifierConstant(&parser.previous);
			aliases[nameCount] = alias;
			aliasPresence[nameCount] = true;
		} else {
			name = identifierConstant(&parser.previous);
		}

		names[nameCount] = name;
		nameCount++;
	} while (match(TOKEN_COMMA));
	if (hasParen) {
		consume(TOKEN_RIGHT_PAREN, "Expected ')' after last imported name.");
	}

	consume(TOKEN_FROM, "Expected 'from' after 'use' statement.");
	consume(TOKEN_STRING, "Expected string literal for module name");
	uint8_t module =
			makeConstant(OBJECT_VAL(copyString(current->owner, parser.previous.start + 1, parser.previous.length - 2)));
	emitBytes(OP_USE, nameCount);
	for (uint8_t i = 0; i < nameCount; i++) {
		emitByte(names[i]);
	}
	for (uint8_t i = 0; i < nameCount; i++) {
		if (aliasPresence[i]) {
			emitByte(aliases[i]);
		} else {
			emitByte(names[i]);
		}
	}
	emitByte(module);
	consume(TOKEN_SEMICOLON, "Expected semicolon after import statement.");
}

/**
 * Synchronizes the parser after encountering a syntax error.
 *
 * Discards tokens until a statement boundary is found to minimize cascading errors.
 */
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
			default: ;
		}
		advance();
	}
}


static void publicDeclaration() {
	if (current->scopeDepth > 0) {
		compilerPanic(&parser, "Cannot declare public members in a local scope.", SYNTAX);
	}
	emitByte(OP_PUB);
	if (match(TOKEN_FN)) {
		fnDeclaration();
	} else if (match(TOKEN_LET)) {
		varDeclaration();
	} else if (match(TOKEN_CLASS)) {
		classDeclaration();
	} else {
		compilerPanic(&parser, "Expected 'fn', 'let', or 'class' after 'pub'.", SYNTAX);
	}
}

static void beginMatchScope() {
	if (current->matchDepth > 0) {
		compilerPanic(&parser, "Nesting match statements is not allowed.", SYNTAX);
	}
	current->matchDepth++;
}

static void endMatchScope() { current->matchDepth--; }


static void giveStatement() {
	if (current->matchDepth == 0) {
		compilerPanic(&parser, "'give' can only be used inside a match expression.", SYNTAX);
	}

	if (match(TOKEN_SEMICOLON)) {
		emitByte(OP_NIL);
	} else {
		expression();
		consume(TOKEN_SEMICOLON, "Expected ';' after give statement.");
	}

	emitByte(OP_GIVE);
}

/**
 * Parses a match expression.
 */
static void matchExpression(bool canAssign) {
	beginMatchScope();
	expression(); // compile match target
	consume(TOKEN_LEFT_BRACE, "Expected '{' after match target.");

	int *endJumps = ALLOCATE(current->owner, int, 8);
	int jumpCount = 0;
	int jumpCapacity = 8;

	emitByte(OP_MATCH);
	bool hasDefault = false;
	bool hasOkPattern = false;
	bool hasErrPattern = false;
	uint8_t bindingSlot = UINT8_MAX;
	bool hasBinding = false;

	while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
		int jumpIfNotMatch = -1;
		bindingSlot = UINT8_MAX;
		hasBinding = false;

		if (match(TOKEN_DEFAULT)) {
			if (hasDefault) {
				compilerPanic(&parser, "Cannot have multiple default patterns.", SYNTAX);
			}
			hasDefault = true;
		} else if (match(TOKEN_OK)) {
			if (hasOkPattern) {
				compilerPanic(&parser, "Cannot have multiple 'Ok' patterns.", SYNTAX);
			}
			hasOkPattern = true;
			jumpIfNotMatch = emitJump(OP_RESULT_MATCH_OK);

			if (match(TOKEN_LEFT_PAREN)) {
				beginScope();
				hasBinding = true;
				consume(TOKEN_IDENTIFIER, "Expected identifier after 'Ok' pattern.");
				declareVariable();
				bindingSlot = current->localCount - 1;
				markInitialized();
				consume(TOKEN_RIGHT_PAREN, "Expected ')' after identifier.");
			}
		} else if (match(TOKEN_ERR)) {
			if (hasErrPattern) {
				compilerPanic(&parser, "Cannot have multiple 'Err' patterns.", SYNTAX);
			}
			hasErrPattern = true;
			jumpIfNotMatch = emitJump(OP_RESULT_MATCH_ERR);

			if (match(TOKEN_LEFT_PAREN)) {
				beginScope();
				hasBinding = true;
				consume(TOKEN_IDENTIFIER, "Expected identifier after 'Err' pattern.");
				declareVariable();
				bindingSlot = current->localCount - 1;
				markInitialized();
				consume(TOKEN_RIGHT_PAREN, "Expected ')' after identifier.");
			}
		} else {
			expression();
			jumpIfNotMatch = emitJump(OP_MATCH_JUMP);
		}

		consume(TOKEN_EQUAL_ARROW, "Expected '=>' after pattern.");

		if (bindingSlot != UINT8_MAX) {
			emitBytes(OP_RESULT_BIND, bindingSlot);
		}

		// Compile match arm body

		if (match(TOKEN_LEFT_BRACE)) {
			block();
		} else if (match(TOKEN_GIVE)) {
			if (match(TOKEN_SEMICOLON)) {
				emitByte(OP_NIL);
			} else {
				expression();
				consume(TOKEN_SEMICOLON, "Expected ';' after give expression.");
			}
			emitByte(OP_GIVE);
		} else {
			expression();
			consume(TOKEN_SEMICOLON, "Expected ';' after expression.");
		}

		if (hasBinding) {
			endScope();
		}

		if (jumpCount + 1 > jumpCapacity) {
			int oldCapacity = jumpCapacity;
			jumpCapacity = GROW_CAPACITY(oldCapacity);
			endJumps = GROW_ARRAY(current->owner, int, endJumps, oldCapacity, jumpCapacity);
		}

		endJumps[jumpCount++] = emitJump(OP_JUMP);

		if (jumpIfNotMatch != -1) {
			patchJump(jumpIfNotMatch);
		}
	}

	if (jumpCount == 0) {
		compilerPanic(&parser, "'match' expression must have at least one arm.", SYNTAX);
	}

	if (hasOkPattern || hasErrPattern) {
		if (!hasDefault && !(hasOkPattern && hasErrPattern)) {
			compilerPanic(&parser, "Result 'match' must have both 'Ok' and 'Err' patterns, or include a default case.",
			              SYNTAX);
		}
	} else if (!hasDefault) {
		compilerPanic(&parser, "'match' expression must have default case 'default'.", SYNTAX);
	}

	for (int i = 0; i < jumpCount; i++) {
		patchJump(endJumps[i]);
	}

	emitByte(OP_MATCH_END);

	FREE_ARRAY(current->owner, int, endJumps, jumpCapacity);
	consume(TOKEN_RIGHT_BRACE, "Expected '}' after match expression.");
	endMatchScope();
}

/**
 * Parses a declaration, which can be a variable, function, or class declaration.
 */
static void declaration() {
	if (match(TOKEN_LET)) {
		varDeclaration();
	} else if (match(TOKEN_CLASS)) {
		classDeclaration();
	} else if (match(TOKEN_FN)) {
		fnDeclaration();
	} else if (match(TOKEN_PUB)) {
		publicDeclaration();
	} else {
		statement();
	}

	if (parser.panicMode)
		synchronize();
}

/**
 * Parses a statement.
 */
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
	} else if (match(TOKEN_RETURN)) {
		returnStatement();
	} else if (match(TOKEN_USE)) {
		useStatement();
	} else if (match(TOKEN_GIVE)) {
		giveStatement();
	} else {
		expressionStatement();
	}
}

/**
 * Parses a grouping expression.
 *
 * @param canAssign Whether the grouping expression can be the target of an assignment.
 */
static void grouping(bool canAssign) {
	expression();
	consume(TOKEN_RIGHT_PAREN, "Expected ')' after expression.");
}

static void number(bool canAssign) {
	char* end;
	errno = 0;

	const char* numberStart = parser.previous.start;
	double number = strtod(numberStart, &end);

	if (end == numberStart) {
		compilerPanic(&parser, "Failed to form number", SYNTAX);
		return;
	}
	if (errno == ERANGE) {
		emitConstant(FLOAT_VAL(number));
		return;
	}
	if (!isfinite(number)) {
		emitConstant(FLOAT_VAL(number));
	}
	int32_t integer = (int32_t)number;
	if ((double)integer == number) {
		emitConstant(INT_VAL(integer));
	}else {
		emitConstant(FLOAT_VAL(number));
	}
}

/**
 * Processes an escape sequence within a string literal.
 *
 * @param escape The escape character (e.g., 'n', 't', '\\').
 * @param hasError A pointer to a boolean to indicate if an error occurred during processing.
 * @return The processed escaped character, or '\0' if an error occurred.
 */
static char processEscapeSequence(char escape, bool *hasError) {
	*hasError = false;
	switch (escape) {
		case 'n':
			return '\n';
		case 't':
			return '\t';
		case 'r':
			return '\r';
		case '\\':
			return '\\';
		case '"':
			return '"';
		case '\'':
			return '\'';
		case '0':
			return ' ';
		case 'a':
			return '\a';
		case 'b':
			return '\b';
		case 'f':
			return '\f';
		case 'v':
			return '\v';
		default: {
			*hasError = true;
			return '\0';
		}
	}
}

/**
 * Parses a string literal.
 *
 * @param canAssign Whether the string literal can be the target of an assignment.
 */
static void string(bool canAssign) {
	char *processed = ALLOCATE(current->owner, char, parser.previous.length);

	if (processed == NULL) {
		compilerPanic(&parser, "Cannot allocate memory for string expression.", MEMORY);
		return;
	}

	int processedLength = 0;
	char *src = (char *) parser.previous.start + 1;
	int srcLength = parser.previous.length - 2;

	if (srcLength == 0) {
		ObjectString *string = copyString(current->owner, "", 0);
		emitConstant(OBJECT_VAL(string));
		return;
	}

	for (int i = 0; i < srcLength; i++) {
		if (src[i] == '\\') {
			if (i + 1 >= srcLength) {
				compilerPanic(&parser, "Unterminated escape sequence at end of string", SYNTAX);
				FREE_ARRAY(current->owner, char, processed, parser.previous.length);
				return;
			}

			bool error;
			char escaped = processEscapeSequence(src[i + 1], &error);
			if (error) {
				char errorMessage[64];
				snprintf(errorMessage, 64, "Unexpected escape sequence '\\%c'", src[i + 1]);
				compilerPanic(&parser, errorMessage, SYNTAX);
				FREE_ARRAY(current->owner, char, processed, parser.previous.length);
				return;
			}

			processed[processedLength++] = escaped;
			i++;
		} else {
			processed[processedLength++] = src[i];
		}
	}

	char *temp = realloc(processed, processedLength * sizeof(char));
	if (temp == NULL) {
		compilerPanic(&parser, "Cannot allocate memory for string expression.", MEMORY);
		FREE_ARRAY(current->owner, char, processed, parser.previous.length);
		return;
	}
	processed = temp;
	processed[processedLength] = '\0';
	ObjectString *string = takeString(current->owner, processed, processedLength);
	emitConstant(OBJECT_VAL(string));
}

/**
 * Parses a 'self' expression (referring to the current object instance within a class method).
 *
 * @param canAssign Whether the 'self' expression can be the target of an assignment.
 */
static void self(bool canAssign) {
	if (currentClass == NULL) {
		compilerPanic(&parser, "'self' cannot be used outside of a class.", NAME);
		return;
	}

	variable(false);
}

/**
 * Parses a unary operator expression.
 *
 * @param canAssign Whether the unary expression can be the target of an assignment.
 */
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

/**
 * @brief Parse rules for each token type.
 *
 * Defines prefix and infix parsing functions and precedence for each token type.
 */
ParseRule rules[] = {
	[TOKEN_LEFT_PAREN] = {grouping, call, PREC_CALL},
	[TOKEN_RIGHT_PAREN] = {NULL, NULL, PREC_NONE},
	[TOKEN_LEFT_BRACE] = {tableLiteral, NULL, PREC_NONE},
	[TOKEN_RIGHT_BRACE] = {NULL, NULL, PREC_NONE},
	[TOKEN_LEFT_SQUARE] = {arrayLiteral, collectionIndex, PREC_CALL},
	[TOKEN_RIGHT_SQUARE] = {NULL, NULL, PREC_NONE},
	[TOKEN_COMMA] = {NULL, NULL, PREC_NONE},
	[TOKEN_DOT] = {NULL, dot, PREC_CALL},
	[TOKEN_MINUS] = {unary, binary, PREC_TERM},
	[TOKEN_PLUS] = {NULL, binary, PREC_TERM},
	[TOKEN_SEMICOLON] = {NULL, NULL, PREC_NONE},
	[TOKEN_SLASH] = {NULL, binary, PREC_FACTOR},
	[TOKEN_BACKSLASH] = {NULL, binary, PREC_FACTOR},
	[TOKEN_STAR] = {NULL, binary, PREC_FACTOR},
	[TOKEN_STAR_STAR] = {NULL, binary, PREC_FACTOR},
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
	[TOKEN_FN] = {anonymousFunction, NULL, PREC_NONE},
	[TOKEN_IF] = {NULL, NULL, PREC_NONE},
	[TOKEN_NIL] = {literal, NULL, PREC_NONE},
	[TOKEN_OR] = {NULL, or_, PREC_OR},
	[TOKEN_RETURN] = {NULL, NULL, PREC_NONE},
	[TOKEN_SUPER] = {super_, NULL, PREC_NONE},
	[TOKEN_SELF] = {self, NULL, PREC_NONE},
	[TOKEN_TRUE] = {literal, NULL, PREC_NONE},
	[TOKEN_LET] = {NULL, NULL, PREC_NONE},
	[TOKEN_USE] = {NULL, NULL, PREC_NONE},
	[TOKEN_FROM] = {NULL, NULL, PREC_NONE},
	[TOKEN_PUB] = {NULL, NULL, PREC_NONE},
	[TOKEN_WHILE] = {NULL, NULL, PREC_NONE},
	[TOKEN_ERROR] = {NULL, NULL, PREC_NONE},
	[TOKEN_DEFAULT] = {NULL, NULL, PREC_NONE},
	[TOKEN_EQUAL_ARROW] = {NULL, NULL, PREC_NONE},
	[TOKEN_MATCH] = {matchExpression, NULL, PREC_PRIMARY},
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
		compilerPanic(&parser, "Expected expression.", SYNTAX);
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
		compilerPanic(&parser, "Invalid Assignment Target", SYNTAX);
	}
}

/**
 * Retrieves the parse rule for a given token type.
 *
 * @param type The token type to get the rule for.
 * @return A pointer to the ParseRule struct for the given token type.
 */
static ParseRule *getRule(TokenType type) {
	return &rules[type]; // Returns the rule at the given index
}

/**
 * Compiles source code into bytecode.
 *
 * @param vm The virtual machine instance.
 * @param source The source code string to compile.
 * @return A pointer to the compiled function object if compilation succeeds, NULL otherwise.
 */
ObjectFunction *compile(VM *vm, char *source) {
	initScanner(source);
	Compiler compiler;
	initCompiler(&compiler, TYPE_SCRIPT, vm);

	parser.hadError = false;
	parser.panicMode = false;
	parser.source = source;

	advance();

	while (!match(TOKEN_EOF)) {
		declaration();
	}

	ObjectFunction *function = endCompiler();
	return parser.hadError ? NULL : function;
}

/**
 * Marks compiler-related objects as reachable for garbage collection.
 *
 * Traverses the compiler chain and marks the function objects associated with each compiler.
 *
 * @param vm The virtual machine instance.
 */
void markCompilerRoots(VM *vm) {
	Compiler *compiler = current;
	while (compiler != NULL) {
		markObject(vm, (Object *) compiler->function);
		compiler = (Compiler *) compiler->enclosing;
	}
}
