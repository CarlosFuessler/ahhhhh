#ifndef TOKEN_H
#define TOKEN_H

// token kinds produced by the lexer
typedef enum {
  TOKEN_STRING_LITERAL,
  TOKEN_NUMBER_LITERAL,
  TOKEN_IDENTIFIER,
  TOKEN_KEYWORD,
  TOKEN_OPEN_PAREN,
  TOKEN_CLOSE_PAREN,
  TOKEN_OPEN_BRACE,
  TOKEN_CLOSE_BRACE,
  TOKEN_OPEN_BRACKET,
  TOKEN_CLOSE_BRACKET,
  TOKEN_COMMA,
  TOKEN_COLON,
  TOKEN_EQUALS,
  TOKEN_DOUBLE_EQUALS,
  TOKEN_NOT_EQUALS,
  TOKEN_LESS,
  TOKEN_LESS_EQUALS,
  TOKEN_GREATER,
  TOKEN_GREATER_EQUALS,
  TOKEN_PLUS,
  TOKEN_MINUS,
  TOKEN_ASTERISK,
  TOKEN_SLASH,
  TOKEN_PERCENT,
  TOKEN_AMPERSAND,
  TOKEN_AT,
  TOKEN_EXCLAMATION_MARK,
  TOKEN_DOT,
  TOKEN_DOUBLE_DOT,
  TOKEN_AND,
  TOKEN_OR,
  TOKEN_NEWLINE,
  TOKEN_EOF,
  TOKEN_ERROR,
  TOKEN_COUNT
} TokenType;

// printable names for token kinds
extern const char *token_type_names[TOKEN_COUNT];

// one token with source location and payload
typedef struct {
  TokenType type;
  int line;
  int col;
  union {
    double number;
    char *string;
  } value;
} Token;

// free token-owned memory if needed
void token_free(Token *token);
// map token kind to display name
const char *token_type_name(TokenType type);

#endif // TOKEN_H
