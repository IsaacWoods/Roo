/*
 * Copyright (C) 2016, Isaac Woods. All rights reserved.
 */

#pragma once

#include <nstring.hpp>
#include <ir.hpp>

enum token_type
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
  TOKEN_LINE,
  TOKEN_INVALID
};

struct token
{
  token_type   type;
  unsigned int offset;
  unsigned int line;
  unsigned int lineOffset;

  const char*  textStart;   // NOTE(Isaac): this points into the parser's source. It is not null-terminated!
  unsigned int textLength;
};

struct roo_parser
{
  const char*   source;
  const char*   currentChar; // NOTE(Isaac): this points into `source`
  unsigned int  currentLine;
  unsigned int  currentLineOffset;

  token         currentToken;
  token         nextToken;
  bool          careAboutLines;

  function_def* firstFunctionDef;
};

void CreateParser(roo_parser& parser, const char* sourcePath);
void Parse(roo_parser& parser);
void FreeParser(roo_parser& parser);

const char* GetTokenName(token_type type);
