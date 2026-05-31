#include "lexer.h"
#include "../buffer/buffer.h"

#include "stdbool.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"

static const char *keywords[] = {
    "enum",  "struct", "fn",     "export", "var",     "const", "return",
    "if",    "else",   "switch", "case",   "default", "for",   "in",
    "while", "br",     "fw",     "mk",     "rm",      "self",  "true",
    "false", "null",   "and",    "or",     NULL};

int is_keyword(const char *str) {
  for (int i = 0; keywords[i] != NULL; i++) {
    if (strcmp(str, keywords[i]) == 0) {
      return 1;
    }
  }
  return 0;
}

static Token make_token(TokenType type, int line, int col) {
  Token token = {.type = type, .line = line, .col = col, .value = {0}};
  return token;
}

static Token make_error_token(int line, int col, const char *message) {
  Token token = {.type = TOKEN_ERROR, .line = line, .col = col, .value = {0}};
  token.value.string = (char *)message;
  return token;
}

Lexer lexer_init(const char *input) {
  Lexer lexer = {
      .input = input == NULL ? "" : input,
      .pos = 0,
      .line = 1,
      .col = 0,
  };
  return lexer;
}

int lexer_peek(Lexer *lexer) {
  unsigned char ch = (unsigned char)lexer->input[lexer->pos];
  if (ch == '\0') {
    return EOF;
  }
  return ch;
}

int lexer_next(Lexer *lexer) {
  int ch = lexer_peek(lexer);
  if (ch == EOF) {
    return EOF;
  }

  lexer->pos += 1;
  if (ch == '\n') {
    lexer->line += 1;
    lexer->col = 0;
  } else {
    lexer->col += 1;
  }
  return ch;
}

static int is_whitespace(int ch) {
  return ch == ' ' || ch == '\n' || ch == '\t' || ch == '\r';
}

static int is_digit(int ch) { return ch >= '0' && ch <= '9'; }

static int is_identifier(int ch) {
  return !is_whitespace(ch) && ch != EOF && ch != '(' && ch != ')' &&
         ch != '{' && ch != '}' && ch != '[' && ch != ']' && ch != ':' &&
         ch != '!' && ch != '.' && ch != '/' && ch != '=' && ch != '<' &&
         ch != '>' && ch != '+' && ch != '-' && ch != '*' && ch != '&' &&
         ch != '@' && ch != ',';
}

static bool skip_whitespace_and_comments(Lexer *lexer) {
  bool skipped_newline = false;
  int peek;

  // comments and things like spaces aor tabs are whitespace
  // and have no use to us, so we skip those
  while (true) {
    while (true) {
      peek = lexer_peek(lexer);
      if (!is_whitespace(peek)) {
        break;
      }
      if (peek == '\n') {
        skipped_newline = true;
      }
      lexer_next(lexer);
    }

    peek = lexer_peek(lexer);
    if (peek == '/') {
      lexer_next(lexer);
      int next = lexer_peek(lexer);
      if (next == '/') {
        // found a comment, so now skipping until EOL
        lexer_next(lexer);
        while (true) {
          peek = lexer_peek(lexer);
          if (peek == '\n') {
            skipped_newline = true;
            lexer_next(lexer);
            break;
          }
          if (peek == EOF) {
            return skipped_newline;
          }
          lexer_next(lexer);
        }
      } else {
        // we've got no comment, go back to the slash
        lexer->pos -= 1;
        if (lexer->col > 0) {
          lexer->col -= 1;
        }
        break;
      }
    } else {
      break;
    }
  }

  return skipped_newline;
}

static bool parse_number(const char *str, double *num) {
  char *end = NULL;
  *num = strtod(str, &end);
  if (end != NULL && *end == '\0') {
    return true;
  }
  return false;
}

Token tokenize_next(Lexer *lexer) {
  if (skip_whitespace_and_comments(lexer)) {
    return make_token(TOKEN_NEWLINE, lexer->line, lexer->col);
  }

  int ch = lexer_next(lexer);
  int line = lexer->line;
  int col = lexer->col;

  switch (ch) {
  case EOF:
    return make_token(TOKEN_EOF, line, col);
  case '(':
    return make_token(TOKEN_OPEN_PAREN, line, col);
  case ')':
    return make_token(TOKEN_CLOSE_PAREN, line, col);
  case '{':
    return make_token(TOKEN_OPEN_BRACE, line, col);
  case '}':
    return make_token(TOKEN_CLOSE_BRACE, line, col);
  case '[':
    return make_token(TOKEN_OPEN_BRACKET, line, col);
  case ']':
    return make_token(TOKEN_CLOSE_BRACKET, line, col);
  case ',':
    return make_token(TOKEN_COMMA, line, col);
  case ':':
    return make_token(TOKEN_COLON, line, col);
  case '+':
    return make_token(TOKEN_PLUS, line, col);
  case '-':
    return make_token(TOKEN_MINUS, line, col);
  case '*':
    return make_token(TOKEN_ASTERISK, line, col);
  case '/':
    return make_token(TOKEN_SLASH, line, col);
  case '%':
    return make_token(TOKEN_PERCENT, line, col);
  case '&':
    return make_token(TOKEN_AMPERSAND, line, col);
  case '@':
    return make_token(TOKEN_AT, line, col);
  case '!':
    if (lexer_peek(lexer) == '=') {
      lexer_next(lexer);
      return make_token(TOKEN_NOT_EQUALS, line, col);
    }
    return make_token(TOKEN_EXCLAMATION_MARK, line, col);
  case '=':
    if (lexer_peek(lexer) == '=') {
      lexer_next(lexer);
      return make_token(TOKEN_DOUBLE_EQUALS, line, col);
    }
    return make_token(TOKEN_EQUALS, line, col);
  case '<':
    if (lexer_peek(lexer) == '=') {
      lexer_next(lexer);
      return make_token(TOKEN_LESS_EQUALS, line, col);
    }
    return make_token(TOKEN_LESS, line, col);
  case '>':
    if (lexer_peek(lexer) == '=') {
      lexer_next(lexer);
      return make_token(TOKEN_GREATER_EQUALS, line, col);
    }
    return make_token(TOKEN_GREATER, line, col);
  case '.':
    if (lexer_peek(lexer) == '.') {
      lexer_next(lexer);
      return make_token(TOKEN_DOUBLE_DOT, line, col);
    }
    return make_token(TOKEN_DOT, line, col);
  default:
    break;
  }

  if (is_digit(ch)) {
    CharBuffer buffer;
    buffer_init(&buffer);

    if (buffer_push(&buffer, (char)ch) < 0) {
      buffer_free(&buffer);
      return make_error_token(line, col, "Out of memory");
    }

    while (is_digit(lexer_peek(lexer))) {
      ch = lexer_next(lexer);
      if (buffer_push(&buffer, (char)ch) < 0) {
        buffer_free(&buffer);
        return make_error_token(line, col, "Out of memory");
      }
    }

    if (lexer_peek(lexer) == '.') {
      int next = (unsigned char)lexer->input[lexer->pos + 1];
      if (is_digit(next)) {
        lexer_next(lexer);
        if (buffer_push(&buffer, '.') < 0) {
          buffer_free(&buffer);
          return make_error_token(line, col, "Out of memory");
        }
        while (is_digit(lexer_peek(lexer))) {
          ch = lexer_next(lexer);
          if (buffer_push(&buffer, (char)ch) < 0) {
            buffer_free(&buffer);
            return make_error_token(line, col, "Out of memory");
          }
        }
      }
    }

    if (buffer_push(&buffer, '\0') < 0) {
      buffer_free(&buffer);
      return make_error_token(line, col, "Out of memory");
    }

    char *str = buffer_release(&buffer);
    double num = 0.0;
    if (!parse_number(str, &num)) {
      free(str);
      return make_error_token(line, col, "invalid number literal");
    }

    free(str);
    Token token = make_token(TOKEN_NUMBER_LITERAL, line, col);
    token.value.number = num;
    return token;
  }

  if (ch == '"') {
    CharBuffer buffer;
    buffer_init(&buffer);

    while (lexer_peek(lexer) != '"') {
      ch = lexer_next(lexer);
      if (ch == EOF) {
        buffer_free(&buffer);
        return make_error_token(line, col,
                                "unexpected EOF file while parsing string");
      }

      if (ch == '\\') {
        ch = lexer_next(lexer);
        if (ch == EOF) {
          buffer_free(&buffer);
          return make_error_token(line, col,
                                  "uexpected EOF while parsing string");
        }

        char actual = (char)ch;
        if (ch == 'n') {
          actual = '\n';
        } else if (ch == 'r') {
          actual = '\r';
        } else if (ch == 't') {
          actual = '\t';
        }

        if (buffer_push(&buffer, actual) < 0) {
          buffer_free(&buffer);
          return make_error_token(line, col, "ut of memory");
        }
      } else {
        if (buffer_push(&buffer, (char)ch) < 0) {
          buffer_free(&buffer);
          return make_error_token(line, col, "out of memory");
        }
      }
    }

    lexer_next(lexer);
    if (buffer_push(&buffer, '\0') < 0) {
      buffer_free(&buffer);
      return make_error_token(line, col, "out of memory");
    }

    Token token = make_token(TOKEN_STRING_LITERAL, line, col);
    token.value.string = buffer_release(&buffer);
    return token;
  }

  CharBuffer buffer;
  buffer_init(&buffer);
  if (buffer_push(&buffer, (char)ch) < 0) {
    buffer_free(&buffer);
    return make_error_token(line, col, "Out of memory");
  }

  while (is_identifier(lexer_peek(lexer))) {
    ch = lexer_next(lexer);
    if (buffer_push(&buffer, (char)ch) < 0) {
      buffer_free(&buffer);
      return make_error_token(line, col, "Out of memory");
    }
  }

  if (buffer_push(&buffer, '\0') < 0) {
    buffer_free(&buffer);
    return make_error_token(line, col, "Out of memory");
  }

  char *str = buffer_release(&buffer);
  double num = 0.0;
  if (parse_number(str, &num)) {
    free(str);
    Token token = make_token(TOKEN_NUMBER_LITERAL, line, col);
    token.value.number = num;
    return token;
  }

  if (is_keyword(str)) {
    if (strcmp(str, "and") == 0) {
      Token token = make_token(TOKEN_AND, line, col);
      token.value.string = str;
      return token;
    }
    if (strcmp(str, "or") == 0) {
      Token token = make_token(TOKEN_OR, line, col);
      token.value.string = str;
      return token;
    }
    Token token = make_token(TOKEN_KEYWORD, line, col);
    token.value.string = str;
    return token;
  }

  Token token = make_token(TOKEN_IDENTIFIER, line, col);
  token.value.string = str;
  return token;
}
