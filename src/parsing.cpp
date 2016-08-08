/*
 * Copyright (C) 2016, Isaac Woods. All rights reserved.
 */

#include <parsing.hpp>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cstddef>
#include <cstdint>
#include <cstdarg>
#include <cassert>

#define Match(expectedType) (parser.currentToken.type == expectedType)

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

static char NextChar(roo_parser& parser)
{
  // Don't dereference memory past the end of the string
  if (*(parser.currentChar) == '\0')
    return '\0';

  if (*(parser.currentChar) == '\n')
  {
    parser.currentLine++;
    parser.currentLineOffset = 0;
  }
  else
  {
    parser.currentLineOffset++;
  }

  return *(parser.currentChar++);
}

static bool IsName(char c)
{
  return ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'));
}

static bool IsDigit(char c)
{
  return (c >= '0' && c <= '9');
}

static token MakeToken(roo_parser& parser, token_type type, unsigned int offset, const char* startChar,
    unsigned int length)
{
  return token{type, offset, parser.currentLine, parser.currentLineOffset, startChar, length};
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
      parser.currentToken = MakeToken(parser, tokenType, tokenOffset, startChar, (unsigned int)length); \
      return; \
    }

  // See if it's a keyword
  KEYWORD("type", TOKEN_TYPE)
  KEYWORD("fn", TOKEN_FN)
  KEYWORD("true", TOKEN_TRUE)
  KEYWORD("false", TOKEN_FALSE)

  // It's not a keyword, so create an identifier token
  parser.currentToken = MakeToken(parser, TOKEN_IDENTIFIER, tokenOffset, startChar, (unsigned int)length);
}

static token NextToken(roo_parser& parser)
{
  token_type type = TOKEN_INVALID;

  while (NextChar(parser) != '\0')
  {
    switch (*parser.currentChar)
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
      return parser.currentToken;
    }
  }

EmitSimpleToken:
  parser.currentToken = MakeToken(parser, type, (unsigned int)(parser.currentChar - parser.source), nullptr, 0u);
  return parser.currentToken;
}

void CreateParser(roo_parser& parser, const char* sourcePath)
{
  parser.source = ReadFile(sourcePath);
  parser.currentChar = parser.source;
  parser.currentLine = 0u;
  parser.currentLineOffset = 0u;
  parser.careAboutLines = false;
  NextToken(parser);
}

__attribute__((noreturn))
static void SyntaxError(roo_parser& parser, const char* messageFmt, ...)
{
#define ERROR_MESSAGE_LENGTH 1024

  va_list args;
  va_start(args, messageFmt);

  char message[ERROR_MESSAGE_LENGTH];
  int length = vsprintf(message, messageFmt, args);
  assert(length < ERROR_MESSAGE_LENGTH);

  fprintf(stderr, "SYNTAX ERROR(%u:%u): %s\n", parser.currentLine, parser.currentLineOffset, message);

  va_end(args);
  exit(1);
#undef ERROR_MESSAGE_LENGTH
}

static inline void Consume(roo_parser& parser, token_type expectedType)
{
  if (parser.currentToken.type != expectedType)
  {
    SyntaxError(parser, "Expected %s, but got %s!", GetTokenName(expectedType), GetTokenName(parser.currentToken.type));
  }

  NextToken(parser);
}

static inline void ConsumeNext(roo_parser& parser, token_type expectedType)
{
  NextToken(parser);
  Consume(parser, expectedType);
}

static void Function(roo_parser& parser)
{
  printf("--> Function\n");
  function_def* definition = static_cast<function_def*>(malloc(sizeof(function_def)));

  token nameToken = NextToken(parser);
  char* name = ToCStr(nstring{nameToken.textStart, nameToken.textLength});

  printf("Function name: %s\n", name);

  Consume(parser, TOKEN_LEFT_PAREN);
  // TODO: parse parameter list
  Consume(parser, TOKEN_RIGHT_PAREN);

  // TODO: parse a block
  Consume(parser, TOKEN_LEFT_BRACE);
  Consume(parser, TOKEN_RIGHT_BRACE);

  printf("<-- Function\n");
}

void Parse(roo_parser& parser)
{
  printf("--- Starting parse ---\n");

  if (Match(TOKEN_FN))
  {
    Function(parser);
  }

  printf("--- Finished parse ---\n");
}

void FreeParser(roo_parser& parser)
{
  /*
   * NOTE(Isaac): the cast here is fine; the C standard is actually wrong.
   * The signature of free should be `free(const void*)`.
   */
  free((char*)parser.source);

  parser.source = nullptr;
  parser.currentChar = nullptr;
}

/*
 * Get a string representation of a given token type.
 * This is for debug purposes and it might be worth stripping it from releases.
 */
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
