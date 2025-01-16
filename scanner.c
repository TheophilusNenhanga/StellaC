#include "scanner.h"

#include <stdbool.h>
#include <string.h>

typedef struct {
	const char *start;
	const char *current;
	int line;
} Scanner;

Scanner scanner;

static char advance() {
	// Consumes the current character and returns it
	scanner.current++;
	return scanner.current[-1];
}

static char peek() { return *scanner.current; }

static bool isAtEnd() { return *scanner.current == '\0'; }

static char peekNext() {
	if (isAtEnd())
		return '\0';
	return scanner.current[1];
}

static bool isIdentifierStarter(char c) {
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c == '$';
}

static bool isAlpha(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }

static void skipWhitespace() {
	for (;;) {
		char c = peek();
		switch (c) {
			case ' ':
			case '\r':
			case '\t':
				advance();
				break;
			case '\n': {
				scanner.line++;
				advance();
				break;
			}
			case '/': {
				if (peekNext() == '/') {
					// Comments are only on one line
					while (peek() != '\n' && !isAtEnd())
						advance();
				} else {
					return;
				}
				break;
			}
			default:
				return;
		}
	}
}

static TokenType checkKeyword(int start, int length, char *rest, TokenType type) {
	if (scanner.current - scanner.start == start + length && memcmp(scanner.start + start, rest, length) == 0) {
		return type;
	}
	return TOKEN_IDENTIFIER;
}

static TokenType identifierType() {
	switch (scanner.start[0]) {
		case 'a':
			return checkKeyword(1, 2, "nd", TOKEN_AND);
		case 'b':
			return checkKeyword(1, 4, "reak", TOKEN_BREAK);
		case 'c':
			if (scanner.current - scanner.start > 1) {
				switch (scanner.start[1]) {
					case 'l':
						return checkKeyword(2, 3, "ass", TOKEN_CLASS);
					case 'o':
						if (scanner.start[2] == 'n') {
							return checkKeyword(2, 6, "ntinue", TOKEN_CONTINUE);
						}
						return TOKEN_CO;
					default:;
				}
			}
			return checkKeyword(1, 4, "lass", TOKEN_CLASS);
		case 'e':
			return checkKeyword(1, 3, "lse", TOKEN_ELSE);
		case 'i':
			return checkKeyword(1, 1, "f", TOKEN_IF);
		case 'l':
			return checkKeyword(1, 2, "et", TOKEN_LET);
		case 'n':
			if (scanner.current - scanner.start > 1) {
				switch (scanner.start[1]) {
					case 'o': {
						return checkKeyword(2, 1, "t", TOKEN_NOT);
					}
					case 'i': {
						return checkKeyword(2, 1, "l", TOKEN_NIL);
					}
					default:;
				}
			}

		case 'o':
			return checkKeyword(1, 1, "r", TOKEN_OR);
		case 'r':
			return checkKeyword(1, 5, "eturn", TOKEN_RETURN);
		case 's':
			if (scanner.current - scanner.start > 1) {
				switch (scanner.start[1]) {
					case 'e':
						if (scanner.current - scanner.start > 2) {
							return checkKeyword(2, 2, "lf", TOKEN_SELF);
						}
					case 'u':
						return checkKeyword(2, 3, "per", TOKEN_SUPER);
					default:;
				}
			}
		case 'w':
			return checkKeyword(1, 4, "hile", TOKEN_WHILE);
		case 'f':
			if (scanner.current - scanner.start > 1) {
				switch (scanner.start[1]) {
					case 'a':
						return checkKeyword(2, 3, "lse", TOKEN_FALSE);
					case 'o':
						return checkKeyword(2, 1, "r", TOKEN_FOR);
					case 'n':
						return TOKEN_FN;
					case 'r':
						return checkKeyword(2, 2, "om", TOKEN_FROM);
					default:
						return TOKEN_IDENTIFIER;
				}
			}
		case 't':
			return checkKeyword(1, 3, "rue", TOKEN_TRUE);
		case 'u':
			return checkKeyword(1, 2, "se", TOKEN_USE);
		case 'p':
			return checkKeyword(1, 2, "ub", TOKEN_PUB);
		case 'y':
			return checkKeyword(1, 4, "ield", TOKEN_YIELD);
		default:;
	}

	return TOKEN_IDENTIFIER;
}

void initScanner(const char *source) {
	scanner.start = source;
	scanner.current = source;
	scanner.line += 1;
}

static Token makeToken(TokenType type) {
	Token token;
	token.type = type;
	token.start = scanner.start;
	token.length = (int) (scanner.current - scanner.start);
	token.line = scanner.line;
	return token;
}

static Token errorToken(const char *message) {
	Token token;
	token.type = TOKEN_ERROR;
	token.start = message;
	token.length = (int) strlen(message);
	token.line = scanner.line;
	return token;
}

static bool match(char expected) {
	if (isAtEnd())
		return false;
	if (*scanner.current != expected)
		return false;
	scanner.current++;
	return true;
}

static Token string() {
	while (peek() != '"' && !isAtEnd()) {
		if (peek() == '\n')
			scanner.line++;
		advance();
	}

	if (isAtEnd())
		return errorToken("Unterminated String");
	// The closing quote
	advance();
	return makeToken(TOKEN_STRING);
}

static bool isDigit(char c) { return c >= '0' && c <= '9'; }

static Token number() {
	while (isDigit(peek()))
		advance();
	bool fpFound = false;
	if (peek() == '.' && isDigit(peekNext())) {
		fpFound = true;
		advance();
		while (isDigit(peek()))
			advance();
	}
	if (fpFound) {
		return makeToken(TOKEN_FLOAT);
	}
	return makeToken(TOKEN_INT);
}

static Token identifier() {
	while (isAlpha(peek()) || isDigit(peek()))
		advance();
	return makeToken(identifierType());
}

Token scanToken() {
	skipWhitespace();
	scanner.start = scanner.current;
	if (isAtEnd())
		return makeToken(TOKEN_EOF);

	char c = advance();

	if (isDigit(c))
		return number();
	if (isIdentifierStarter(c))
		return identifier();

	switch (c) {
		case ':':
			return makeToken(TOKEN_COLON);
		case '(':
			return makeToken(TOKEN_LEFT_PAREN);
		case ')':
			return makeToken(TOKEN_RIGHT_PAREN);
		case '{':
			return makeToken(TOKEN_LEFT_BRACE);
		case '}':
			return makeToken(TOKEN_RIGHT_BRACE);
		case '[':
			return makeToken(TOKEN_LEFT_SQUARE);
		case ']':
			return makeToken(TOKEN_RIGHT_SQUARE);
		case ';':
			return makeToken(TOKEN_SEMICOLON);
		case ',':
			return makeToken(TOKEN_COMMA);
		case '.':
			return makeToken(TOKEN_DOT);
		case '-':
			return makeToken(match('=') ? TOKEN_MINUS_EQUAL : TOKEN_MINUS);
		case '+':
			return makeToken(match('=') ? TOKEN_PLUS_EQUAL : TOKEN_PLUS);
		case '/':
			return makeToken(match('=') ? TOKEN_SLASH_EQUAL : TOKEN_SLASH);
		case '*':
			return makeToken(match('=') ? TOKEN_STAR_EQUAL : TOKEN_STAR);
		case '%':
			return makeToken(TOKEN_PERCENT);
		case '!':
			if (match('=')) {
				return makeToken(TOKEN_BANG_EQUAL);
			}
		case '=':
			return makeToken(match('=') ? TOKEN_EQUAL_EQUAL : TOKEN_EQUAL);
		case '<':
			if (match('<')) {
				return makeToken(TOKEN_LEFT_SHIFT);
			}
			return makeToken(match('=') ? TOKEN_LESS_EQUAL : TOKEN_LESS);
		case '>':
			if (match('>')) {
				return makeToken(TOKEN_RIGHT_SHIFT);
			}
			return makeToken(match('=') ? TOKEN_GREATER_EQUAL : TOKEN_GREATER);
		case '"':
			return string();
		default:;
	}
	return errorToken("Unexpected character.");
}
