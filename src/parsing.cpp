/*
 * Copyright (C) 2016, Isaac Woods. All rights reserved.
 */

#include <parsing.hpp>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cstddef>
#include <cstdint>

/*
 * Reads a file as a string.
 * The string is allocated on the heap and it is the responsibility of the caller to free it.
 */
static const char* ReadFile(const char* path)
{
  FILE* file = fopen(path, "rb");

  if (!file)
  {
    fprintf(stderr, "Failed to read source file: %s\n", path);
    exit(1);
  }

  fseek(file, 0, SEEK_END);
  unsigned long length = ftell(file);
  fseek(file, 0, SEEK_SET);
  char* contents = static_cast<char*>(malloc(length + 1));

  if (!contents)
  {
    fprintf(stderr, "Failed to allocate space for source file!\n");
    exit(1);
  }

  if (fread(contents, 1, length, file) != length)
  {
    fprintf(stderr, "Failed to read source file: %s\n", path);
    exit(1);
  }

  contents[length] = '\0';
  fclose(file);

  return contents;
}

void CreateParser(roo_parser& parser, const char* sourcePath)
{
  parser.source = ReadFile(sourcePath);
  parser.currentChar = parser.source;
  parser.careAboutLines = false;
  NextToken(parser);
}

void FreeParser(roo_parser& parser)
{
  /*
   * NOTE(Isaac): the cast here is fine; the C standard is actually wrong
   * The signature of free should be `free(const void*)`.
   */
  free((char*)parser.source);

  parser.source = nullptr;
  parser.currentChar = nullptr;
}

char NextChar(roo_parser& parser)
{
  // Don't dereference memory past the end of the string
  if (parser.currentChar == '\0')
    return '\0';

  parser.currentChar++;
  return *(parser.currentChar);
}

static bool IsName(char c)
{
  return ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'));
}

static bool IsDigit(char c)
{
  return (c >= '0' && c <= '9');
}

/*
 * Lex identifiers and keywords
 */
static void LexName(roo_parser& parser)
{
  // Get the current char as well
  const char* startChar = parser.currentChar - 1u;

  // Concat the string until the next character that isn't part of the name
  while (IsName(*(parser.currentChar)))
    NextChar(parser);

  ptrdiff_t length = (uintptr_t)parser.currentChar - (uintptr_t)startChar;
  unsigned int tokenOffset = (unsigned int)(parser.currentChar - parser.source);

  #define KEYWORD(keyword, tokenType) \
    if (memcmp(startChar, keyword, length) == 0) \
    { \
      parser.currentToken = token{tokenType, tokenOffset, startChar, (unsigned int)length}; \
      return; \
    }

  KEYWORD("type", TOKEN_TYPE)
  KEYWORD("fn", TOKEN_FN)
  KEYWORD("true", TOKEN_TRUE)
  KEYWORD("false", TOKEN_FALSE)

  // It's not a keyword, so create an identifier token
  parser.currentToken = token{TOKEN_IDENTIFIER, tokenOffset, startChar, (unsigned int)length};
}

void NextToken(roo_parser& parser)
{
  token_type type = TOKEN_INVALID;

  while (*parser.currentChar != '\0')
  {
    switch (*(parser.currentChar++))
    {
      case '.':
        type = TOKEN_DOT;
        goto EmitSimpleToken;
  
      case ',':
        type = TOKEN_COMMA;
        goto EmitSimpleToken;
  
      case '(':
        type = TOKEN_LEFT_PAREN;
        goto EmitSimpleToken;
  
      case ')':
        type = TOKEN_RIGHT_PAREN;
        goto EmitSimpleToken;
  
      case '{':
        type = TOKEN_LEFT_BRACE;
        goto EmitSimpleToken;
  
      case '}':
        type = TOKEN_RIGHT_BRACE;
        goto EmitSimpleToken;
  
      case '[':
        type = TOKEN_LEFT_BLOCK;
        goto EmitSimpleToken;
  
      case ']':
        type = TOKEN_RIGHT_BLOCK;
        goto EmitSimpleToken;
  
      case '\'':
        type = TOKEN_SINGLE_QUOTE;
        goto EmitSimpleToken;
  
      case '"':
        type = TOKEN_DOUBLE_QUOTE;
        goto EmitSimpleToken;
  
      case '*':
        type = TOKEN_ASTERIX;
        goto EmitSimpleToken;
  
      case '&':
        type = TOKEN_AMPERSAND;
        goto EmitSimpleToken;
  
      case '+':
        type = TOKEN_PLUS;
        goto EmitSimpleToken;
  
      case '-':
        type = TOKEN_MINUS;
        goto EmitSimpleToken;
  
      case '/':
        type = TOKEN_SLASH;
        goto EmitSimpleToken;
  
      case ' ':
      case '\t':
        break;

      case '\n':
        if (parser.careAboutLines)
        {
          type = TOKEN_LINE;
          goto EmitSimpleToken;
        }
        else
        {
          break;
        }
    }
  
    // Lex identifiers and keywords
    if (IsName(*parser.currentChar))
    {
      LexName(parser);
      return;
    }
  }

  parser.currentToken = token{TOKEN_INVALID, (unsigned int) (parser.currentChar - parser.source), nullptr, 0u};

EmitSimpleToken:
  parser.currentToken = token{type, (unsigned int) (parser.currentChar - parser.source), nullptr, 0u};
}

const char* GetTokenName(token_type type)
{
  switch (type)
  {
    case TOKEN_TYPE:
      return "TOKEN_TYPE";
    case TOKEN_FN:
      return "TOKEN_FN";
    case TOKEN_TRUE:
      return "TOKEN_TRUE";
    case TOKEN_FALSE:
      return "TOKEN_FALSE";

    case TOKEN_DOT:
      return "TOKEN_DOT";
    case TOKEN_COMMA:
      return "TOKEN_COMMA";
    case TOKEN_LEFT_PAREN:
      return "TOKEN_LEFT_PAREN";
    case TOKEN_RIGHT_PAREN:
      return "TOKEN_RIGHT_PAREN";
    case TOKEN_LEFT_BRACE:
      return "TOKEN_LEFT_BRACE";
    case TOKEN_RIGHT_BRACE:
      return "TOKEN_RIGHT_BRACE";
    case TOKEN_LEFT_BLOCK:
      return "TOKEN_LEFT_BLOCK";
    case TOKEN_RIGHT_BLOCK:
      return "TOKEN_RIGHT_BLOCK";
    case TOKEN_SINGLE_QUOTE:
      return "TOKEN_SINGLE_QUOTE";
    case TOKEN_DOUBLE_QUOTE:
      return "TOKEN_DOUBLE_QUOTE";
    case TOKEN_ASTERIX:
      return "TOKEN_ASTERIX";
    case TOKEN_AMPERSAND:
      return "TOKEN_AMPERSAND";
    case TOKEN_PLUS:
      return "TOKEN_PLUS";
    case TOKEN_MINUS:
      return "TOKEN_MINUS";
    case TOKEN_SLASH:
      return "TOKEN_SLASH";

    case TOKEN_IDENTIFIER:
      return "TOKEN_IDENTIFIER";
    case TOKEN_LINE:
      return "TOKEN_LINE";
    case TOKEN_INVALID:
      return "TOKEN_INVALID";

    default:
      fprintf(stderr, "Unhandled token type in GetTokenName!\n");
      exit(1);
  }
}
