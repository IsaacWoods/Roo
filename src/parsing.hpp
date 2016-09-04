/*
 * Copyright (C) 2016, Isaac Woods. All rights reserved.
 */

#pragma once

#include <cstring>
#include <cstdlib>
#include <token_type.hpp>
#include <ir.hpp>

struct token
{
  token_type   type;
  unsigned int offset;
  unsigned int line;
  unsigned int lineOffset;

  const char*  textStart;   // NOTE(Isaac): this points into the parser's source. It is not null-terminated!
  unsigned int textLength;
};

inline char* GetTextFromToken(const token& tkn)
{
  char* text = static_cast<char*>(malloc(sizeof(char) * tkn.textLength + 1u));
  memcpy(text, tkn.textStart, tkn.textLength);
  text[tkn.textLength] = '\0';

  return text;
}

struct parse_result
{
  function_def* firstFunction;
};

void FreeParseResult(parse_result& result);

struct roo_parser;
typedef node* (*prefix_parselet)(roo_parser&);
typedef node* (*infix_parselet)(roo_parser&, node* left);

struct roo_parser
{
  char*           source;
  const char*     currentChar; // NOTE(Isaac): this points into `source`
  unsigned int    currentLine;
  unsigned int    currentLineOffset;

  token           currentToken;
  token           nextToken;

  prefix_parselet prefixMap[NUM_TOKENS];
  infix_parselet  infixMap[NUM_TOKENS];
  unsigned int    precedenceTable[NUM_TOKENS];

  parse_result*   result;
};

void CreateParser(roo_parser& parser, parse_result* result, const char* sourcePath);
void Parse(roo_parser& parser);
void FreeParser(roo_parser& parser);
