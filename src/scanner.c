#include "scanner.h"

#include <math.h>
#include <stdbool.h>
#include <string.h>

typedef struct {
	const char *start;
	const char *current;
	int line;
} Scanner;

Scanner scanner;

/**
 * Advances the scanner to the next character.
 * @return The character that was just consumed
 */
static char advance() {
	scanner.current++;
	return scanner.current[-1];
}

/**
 * Returns the current character without consuming it.
 * @return The current character
 */
static char peek() { return *scanner.current; }

/**
 * Checks if the scanner has reached the end of the source.
 * @return true if at the end of source, false otherwise
 */
static bool isAtEnd() { return *scanner.current == '\0'; }

/**
 * Returns the character after the current one without consuming any.
 * @return The next character, or '\0' if at the end of source
 */
static char peekNext() {
	if (isAtEnd())
		return '\0';
	return scanner.current[1];
}

/**
 * Determines if a character can start an identifier.
 * @param c The character to check
 * @return true if the character can start an identifier, false otherwise
 */
static bool isIdentifierStarter(char c) {
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c == '$';
}

/**
 * Checks if a character is alphabetic or underscore.
 * @param c The character to check
 * @return true if the character is alphabetic or underscore, false otherwise
 */
static bool isAlpha(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }

/**
 * Skips whitespace and comments in the source code.
 */
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

/**
 * Determines the token type of identifier.
 * @return The token type (keyword or identifier)
 */
static TokenType identifierType() {
	switch (scanner.start[0]) {
		case 'a':
			if (scanner.current - scanner.start > 1) {
				switch (scanner.start[1]) {
					case 's': {
						return checkKeyword(2, 0, "", TOKEN_AS);
					}
					case 'n': {
						return checkKeyword(2, 1, "d", TOKEN_AND);
					}
					default:;
				}
			}
		case 'b':
			return checkKeyword(1, 4, "reak", TOKEN_BREAK);
		case 'c': {
			if (scanner.current - scanner.start > 1) {
				switch (scanner.start[1]) {
					case 'l':
						return checkKeyword(2, 3, "ass", TOKEN_CLASS);
					case 'o':
						return checkKeyword(2, 6, "ntinue", TOKEN_CONTINUE);
					default:;
				}
			}
			return checkKeyword(1, 4, "lass", TOKEN_CLASS);
		}
		case 'd': {
			return checkKeyword(1, 6, "efault", TOKEN_DEFAULT);
		}
		case 'e':
			return checkKeyword(1, 3, "lse", TOKEN_ELSE);
		case 'g': {
			return checkKeyword(1, 3, "ive", TOKEN_GIVE);
		}
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
		case 'm': {
			return checkKeyword(1, 4, "atch", TOKEN_MATCH);
		}
		case 't':
			return checkKeyword(1, 3, "rue", TOKEN_TRUE);
		case 'u':
			return checkKeyword(1, 2, "se", TOKEN_USE);
		case 'p':
			return checkKeyword(1, 2, "ub", TOKEN_PUB);
		case 'E':
			return checkKeyword(1, 2, "rr", TOKEN_ERR);
		case 'O':
			return checkKeyword(1, 1, "k", TOKEN_OK);
		default:;
	}

	return TOKEN_IDENTIFIER;
}


void initScanner(const char *source) {
	scanner.start = source;
	scanner.current = source;
	scanner.line += 1;
}

/**
 * Creates a token of the given type from the current lexeme.
 * @param type The token type
 * @return The created token
 */
static Token makeToken(TokenType type) {
	Token token;
	token.type = type;
	token.start = scanner.start;
	token.length = (int) (scanner.current - scanner.start);
	token.line = scanner.line;
	return token;
}

/**
 * Creates an error token with the given message.
 * @param message The error message
 * @return The error token
 */
static Token errorToken(const char *message) {
	Token token;
	token.type = TOKEN_ERROR;
	token.start = message;
	token.length = (int) strlen(message);
	token.line = scanner.line;
	return token;
}

/**
 * Checks if the current character matches the expected one and advances if it does.
 * @param expected The character to match
 * @return true if the character matches, false otherwise
 */
static bool match(char expected) {
	if (isAtEnd())
		return false;
	if (*scanner.current != expected)
		return false;
	scanner.current++;
	return true;
}

/**
 * Scans a single-quoted string literal.
 * @return The string token or an error token
 */
static Token singleString() {
	while (!isAtEnd()) {
		if (peek() == '\'') break;
		if (peek() == '\\') {
			advance();
			if (!isAtEnd()) {
				advance();
			}
			continue;
		}
		if (peek() == '\n') {
			scanner.line++;
		}
		advance();
	}

	if (isAtEnd())
		return errorToken("Unterminated String");
	// The closing quote
	advance();
	return makeToken(TOKEN_STRING);
}

/**
 * Scans a double-quoted string literal.
 * @return The string token or an error token
 */
static Token doubleString() {
	while (!isAtEnd()) {
		if (peek() == '"') break;
		if (peek() == '\\') {
			advance();
			if (!isAtEnd()) {
				advance();
			}
			continue;
		}
		if (peek() == '\n') {
			scanner.line++;
		}
		advance();
	}

	if (isAtEnd())
		return errorToken("Unterminated String");
	// The closing quote
	advance();
	return makeToken(TOKEN_STRING);
}


/**
 * Checks if a character is a digit (0-9).
 * @param c The character to check
 * @return true if the character is a digit, false otherwise
 */
static bool isDigit(char c) { return c >= '0' && c <= '9'; }

/**
 * Scans a numeric literal (integer or float).
 * @return The numeric token (TOKEN_INT or TOKEN_FLOAT)
 */
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

/**
 * Scans an identifier or keyword.
 * @return The identifier or keyword token
 */
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
		case '\\':
			if (match('=')) {
				return makeToken(TOKEN_BACK_SLASH_EQUAL);
			}
			return makeToken(TOKEN_BACKSLASH);
		case '*':
			if (match('*')) {
				return makeToken(TOKEN_STAR_STAR);
			}
			if (match('=')) {
				return makeToken(TOKEN_STAR_EQUAL);
			}
			return makeToken(TOKEN_STAR);
		case '%':
			if (match('=')) {
				return makeToken(TOKEN_PERCENT_EQUAL);
			}
			return makeToken(TOKEN_PERCENT);
		case '!':
			if (match('=')) {
				return makeToken(TOKEN_BANG_EQUAL);
			}
		case '=':
			// we can have == or = or => as tokens
			if (match('=')) {
				return makeToken(TOKEN_EQUAL_EQUAL);
			}
			if (match('>')) {
				return makeToken(TOKEN_EQUAL_ARROW);
			}
			return makeToken(TOKEN_EQUAL);
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
			return doubleString();
		case '\'':
			return singleString();
		default:;
	}
	return errorToken("Unexpected character.");
}
