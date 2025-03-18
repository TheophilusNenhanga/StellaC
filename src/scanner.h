#ifndef SCANNER_H
#define SCANNER_H

typedef enum {
	// Single-character tokens.
	TOKEN_LEFT_PAREN,
	TOKEN_RIGHT_PAREN,
	TOKEN_LEFT_BRACE,
	TOKEN_RIGHT_BRACE,
	TOKEN_LEFT_SQUARE,
	TOKEN_RIGHT_SQUARE,
	TOKEN_COMMA,
	TOKEN_DOT,
	TOKEN_MINUS,
	TOKEN_PLUS,
	TOKEN_SEMICOLON,
	TOKEN_SLASH,
	TOKEN_STAR,
	TOKEN_PERCENT,
	TOKEN_COLON,
	TOKEN_SINGLE_QUOTE,
	TOKEN_DOUBLE_QUOTE,
	// One or two character tokens.
	TOKEN_BANG_EQUAL,
	TOKEN_EQUAL,
	TOKEN_EQUAL_EQUAL,
	TOKEN_GREATER,
	TOKEN_GREATER_EQUAL,
	TOKEN_LESS,
	TOKEN_LESS_EQUAL,
	TOKEN_LEFT_SHIFT,
	TOKEN_RIGHT_SHIFT,
	TOKEN_PLUS_EQUAL,
	TOKEN_MINUS_EQUAL,
	TOKEN_STAR_EQUAL,
	TOKEN_SLASH_EQUAL,
	// Literals.
	TOKEN_IDENTIFIER,
	TOKEN_STRING,
	TOKEN_INT,
	TOKEN_FLOAT,
	// Keywords.
	TOKEN_AND,
	TOKEN_NOT,
	TOKEN_CLASS,
	TOKEN_ELSE,
	TOKEN_FALSE,
	TOKEN_FOR,
	TOKEN_FN,
	TOKEN_IF,
	TOKEN_NIL,
	TOKEN_OR,
	TOKEN_RETURN,
	TOKEN_SUPER,
	TOKEN_SELF,
	TOKEN_TRUE,
	TOKEN_LET,
	TOKEN_WHILE,
	TOKEN_ERROR,
	TOKEN_BREAK,
	TOKEN_CONTINUE,
	TOKEN_USE,
	TOKEN_FROM,
	TOKEN_PUB,
	TOKEN_AS, 
	TOKEN_EOF,
	TOKEN_MATCH,
	TOKEN_EQUAL_ARROW,
	TOKEN_OK,
	TOKEN_ERR,
	TOKEN_DEFAULT,
	TOKEN_GIVE,
} TokenType;

typedef struct {
	TokenType type;
	int length;
	const char *start;
	int line;
} Token;

/**
 * Initializes the scanner with the given source code.
 * @param source Pointer to the source code string
 */
void initScanner(const char *source);

/**
 * Scans the next token from the source code.
 * @return The scanned token
 */
Token scanToken();

#endif // SCANNER_H
