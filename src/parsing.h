/*
 * Copyright (C) 2016, Isaac Woods. All rights reserved.
 */

#pragma once

typedef enum
{
  // Keywords
  TOKEN_TYPE,
  TOKEN_FN,
  TOKEN_TRUE,
  TOKEN_FALSE,

  // Punctuation n' shit
  TOKEN_DOT,
  TOKEN_COMMA,
  TOKEN_LEFT_PAREN,
  TOKEN_RIGHT_PAREN,
  TOKEN_LEFT_BRACE,
  TOKEN_RIGHT_BRACE,
  TOKEN_LEFT_BLOCK,
  TOKEN_RIGHT_BLOCK,
  TOKEN_SINGLE_QUOTE,
  TOKEN_DOUBLE_QUOTE,
  TOKEN_ASTERIX,
  TOKEN_AMPERSAND,
  TOKEN_PLUS,
  TOKEN_MINUS,
  TOKEN_SLASH,

  // Other stuff
  TOKEN_IDENTIFIER,
  TOKEN_NEW_LINE,
  TOKEN_INVALID
} token_type;

typedef struct
{
  token_type   type;
  unsigned int offset;
  const char*  textStart;   // NOTE(Isaac): this points into the parser's source
  unsigned int textLength;
} token;

typedef struct
{
  const char* source;
  const char* currentChar; // NOTE(Isaac): this points into `source`
  token       currentToken;
  token       nextToken;
} roo_parser;

void CreateParser(roo_parser* parser, const char* sourcePath);
void FreeParser(roo_parser* parser);
char NextChar(roo_parser* parser);
void NextToken(roo_parser* parser);

const char* GetTokenName(token_type type);
