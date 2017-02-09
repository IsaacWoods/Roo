/*
 * Copyright (C) 2016, Isaac Woods. All rights reserved.
 */

#pragma once

#include <error.hpp>

struct parse_result;

enum token_type
{
  // Keywords
  TOKEN_TYPE,
  TOKEN_FN,
  TOKEN_TRUE,
  TOKEN_FALSE,
  TOKEN_IMPORT,
  TOKEN_BREAK,
  TOKEN_RETURN,
  TOKEN_IF,
  TOKEN_ELSE,
  TOKEN_WHILE,
  TOKEN_MUT,
  TOKEN_OPERATOR,

  // Punctuation n' shit
  TOKEN_DOT,
  TOKEN_COMMA,
  TOKEN_COLON,
  TOKEN_LEFT_PAREN,
  TOKEN_RIGHT_PAREN,
  TOKEN_LEFT_BRACE,             // {
  TOKEN_RIGHT_BRACE,            // }
  TOKEN_LEFT_BLOCK,             // [
  TOKEN_RIGHT_BLOCK,            // ]
  TOKEN_ASTERIX,
  TOKEN_PLUS,
  TOKEN_MINUS,
  TOKEN_SLASH,
  TOKEN_EQUALS,
  TOKEN_BANG,                   // !
  TOKEN_TILDE,                  // ~
  TOKEN_PERCENT,
  TOKEN_QUESTION_MARK,
  TOKEN_POUND,                  // #

  TOKEN_YIELDS,                 // ->
  TOKEN_START_ATTRIBUTE,        // #[
  TOKEN_EQUALS_EQUALS,
  TOKEN_BANG_EQUALS,
  TOKEN_GREATER_THAN,
  TOKEN_GREATER_THAN_EQUAL_TO,
  TOKEN_LESS_THAN,
  TOKEN_LESS_THAN_EQUAL_TO,
  TOKEN_DOUBLE_PLUS,
  TOKEN_DOUBLE_MINUS,
  TOKEN_LEFT_SHIFT,
  TOKEN_RIGHT_SHIFT,
  TOKEN_AND,
  TOKEN_OR,
  TOKEN_XOR,
  TOKEN_DOUBLE_AND,
  TOKEN_DOUBLE_OR,

  // Other stuff
  TOKEN_IDENTIFIER,
  TOKEN_STRING,
  TOKEN_SIGNED_INT,
  TOKEN_UNSIGNED_INT,
  TOKEN_FLOAT,
  TOKEN_CHAR_CONSTANT,
  TOKEN_LINE,
  TOKEN_INVALID,

  NUM_TOKENS
};

struct token
{
  token_type   type;
  unsigned int offset;
  unsigned int line;
  unsigned int lineOffset;

  const char*  textStart;   // NOTE(Isaac): this points into the parser's source. It is not null-terminated!
  unsigned int textLength;

  union
  {
    int           asSignedInt;    // Valid if token type is TOKEN_SIGNED_INT
    unsigned int  asUnsignedInt;  // Valid if token type is TOKEN_UNSIGNED_INT
    float         asFloat;        // Valid if token type is TOKEN_FLOAT
  };
};

struct roo_parser
{
  const char*     path;               // NOTE(Isaac): the path of the thing we are compiling
  char*           source;
  const char*     currentChar;        // NOTE(Isaac): this points into `source`
  unsigned int    currentLine;
  unsigned int    currentLineOffset;

  token           currentToken;
  token           nextToken;

  parse_result*   result;
  error_state     errorState;
};

token PeekToken(roo_parser& parser, bool ignoreLines = true);
token NextToken(roo_parser& parser, bool ignoreLines = true);
const char* GetTokenName(token_type type);
void InitParseletMaps();
bool Parse(parse_result* result, const char* sourcePath);
