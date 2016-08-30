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
#define MatchNext(expectedType) (parser.nextToken.type == expectedType)

/*
 * Reads a file as a string.
 * The string is allocated on the heap and it is the responsibility of the caller to free it.
 */
static char* ReadFile(const char* path)
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
static token LexName(roo_parser& parser)
{
  // NOTE(Isaac): Get the current char as well
  const char* startChar = parser.currentChar - 1u;

  // NOTE(Isaac): Concat the string until the next character that isn't part of the name
  while (IsName(*(parser.currentChar)))
    NextChar(parser);

  ptrdiff_t length = (ptrdiff_t)((uintptr_t)parser.currentChar - (uintptr_t)startChar);
  unsigned int tokenOffset = (unsigned int)(parser.currentChar - parser.source);

  // Parse a keyword
  #define KEYWORD(keyword, tokenType) \
    if (memcmp(startChar, keyword, length) == 0) \
    { \
      return MakeToken(parser, tokenType, tokenOffset, startChar, (unsigned int)length); \
    }

  KEYWORD("type", TOKEN_TYPE)
  KEYWORD("fn", TOKEN_FN)
  KEYWORD("true", TOKEN_TRUE)
  KEYWORD("false", TOKEN_FALSE)
  KEYWORD("import", TOKEN_IMPORT)

  // It's not a keyword, so create an identifier token
  return MakeToken(parser, TOKEN_IDENTIFIER, tokenOffset, startChar, (unsigned int)length);
}

static token LexNext(roo_parser& parser)
{
  token_type type = TOKEN_INVALID;

  while (*(parser.currentChar) != '\0')
  {
    /*
     * When lexing, the current char is actually the char after `c`.
     * Calling `NextChar()` will actually get the char after the char after `c`.
     */
    char c = NextChar(parser);

    switch (c)
    {
      case '.':
        type = TOKEN_DOT;
        goto EmitSimpleToken;
  
      case ',':
        type = TOKEN_COMMA;
        goto EmitSimpleToken;
 
      case ':':
        type = TOKEN_COLON;
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
      case '\r':
      case '\t':
        // Skip past any whitespace
        while (*(parser.currentChar) == ' '  ||
               *(parser.currentChar) == '\r' ||
               *(parser.currentChar) == '\t' ||
               *(parser.currentChar) == '\n')
        {
          NextChar(parser);
        }
        break;

      case '\n':
        type = TOKEN_LINE;
        goto EmitSimpleToken;
    }
  
    if (IsName(c))
    {
      return LexName(parser);
    }
  }

EmitSimpleToken:
  return MakeToken(parser, type, (uintptr_t)parser.currentChar - (uintptr_t)parser.source, nullptr, 0u);
}

/*static*/ token NextToken(roo_parser& parser, bool ignoreLines = true)
{
  parser.currentToken = parser.nextToken;
  parser.nextToken = LexNext(parser);

  if (ignoreLines)
  {
    while (parser.currentToken.type == TOKEN_LINE)
    {
      NextToken(parser);
      printf("INFO: Relexed current token to: %s\n", GetTokenName(parser.currentToken.type));
    }
  }

  return parser.currentToken;
}

static token PeekToken(roo_parser& parser, bool ignoreLines = true)
{
  if (ignoreLines)
  {
    while (parser.currentToken.type == TOKEN_LINE)
    {
      NextToken(parser);
      printf("INFO: Relexed current token to: %s\n", GetTokenName(parser.currentToken.type));
    }
  }

  return parser.currentToken;
}

// TODO(Isaac): should we remove this? It can be useful but because reasons (see below), it's a pain in the ass
static token PeekNextToken(roo_parser& parser, bool ignoreLines = true)
{
  if (ignoreLines && parser.nextToken.type == TOKEN_LINE)
  {
    /*
     * NOTE(Isaac): okay, so to be honest, I have literally no idea what would be an even vaguely sensible thing
     * to do here. The current plan is to shoot ourselves in the head and hope nobody tries to use this.
     */
    fprintf(stderr, "PeekNextToken called and TOKEN_LINE is next! Everything's gone tits up\n");
    exit(1);
  }

  return parser.nextToken;
}

void CreateParser(roo_parser& parser, const char* sourcePath)
{
  parser.source = ReadFile(sourcePath);
  parser.currentChar = parser.source;
  parser.currentLine = 0u;
  parser.currentLineOffset = 0u;

  parser.currentToken = LexNext(parser);
  parser.nextToken = LexNext(parser);

  parser.firstFunction = nullptr;
}

__attribute__((noreturn))
static void SyntaxError(roo_parser& parser, const char* messageFmt, ...)
{
  const int ERROR_MESSAGE_LENGTH = 1024;

  va_list args;
  va_start(args, messageFmt);

  char message[ERROR_MESSAGE_LENGTH];
  int length = vsprintf(message, messageFmt, args);
  assert(length < ERROR_MESSAGE_LENGTH);

  fprintf(stderr, "SYNTAX ERROR(%u:%u): %s\n", parser.currentLine, parser.currentLineOffset, message);

  va_end(args);
  exit(1);
}

static void PeekNPrint(roo_parser& parser, bool ignoreLines = true)
{
  if (PeekToken(parser, ignoreLines).type == TOKEN_IDENTIFIER)
  {
    printf("PEEK: (%s)\n", ExtractText(PeekToken(parser, ignoreLines)));
  }
  else
  {
    printf("PEEK: %s\n", GetTokenName(PeekToken(parser, ignoreLines).type));
  }
}

static void PeekNPrintNext(roo_parser& parser, bool ignoreLines = true)
{
  if (PeekNextToken(parser, ignoreLines).type == TOKEN_IDENTIFIER)
  {
    printf("PEEK_NEXT: (%s)\n", ExtractText(PeekNextToken(parser, ignoreLines)));
  }
  else
  {
    printf("PEEK_NEXT: %s\n", GetTokenName(PeekNextToken(parser, ignoreLines).type));
  }
}

static inline void Consume(roo_parser& parser, token_type expectedType, bool ignoreLines = true)
{
  if (PeekToken(parser, ignoreLines).type != expectedType)
  {
    SyntaxError(parser, "Expected %s, but got %s!", GetTokenName(expectedType), GetTokenName(parser.currentToken.type));
  }

  NextToken(parser, ignoreLines);
}

static inline void ConsumeNext(roo_parser& parser, token_type expectedType, bool ignoreLines = true)
{
  token_type next = NextToken(parser, ignoreLines).type;
  
  if (next != expectedType)
  {
    SyntaxError(parser, "Expected %s, but got %s!", GetTokenName(expectedType), GetTokenName(next));
  }

  NextToken(parser, ignoreLines);
}

// --- Parsing ---
static parameter_def* ParameterList(roo_parser& parser)
{
  ConsumeNext(parser, TOKEN_LEFT_PAREN);
  parameter_def* paramList = nullptr;

  // Check for an empty parameter list
  if (Match(TOKEN_RIGHT_PAREN))
  {
    Consume(parser, TOKEN_RIGHT_PAREN);
    return nullptr;
  }

  do
  {
    parameter_def* param = static_cast<parameter_def*>(malloc(sizeof(parameter_def)));
    param->name = ExtractText(PeekToken(parser));
    ConsumeNext(parser, TOKEN_COLON);
    param->typeName = ExtractText(PeekToken(parser));
    param->next = nullptr;

    printf("Param: %s of type %s\n", param->name, param->typeName);

    if (paramList)
    {
      parameter_def* tail = paramList;

      while (tail->next)
      {
        tail = tail->next;
      }

      tail->next = param;
    }
    else
    {
      paramList = param;
    }
  } while (MatchNext(TOKEN_COMMA));

  ConsumeNext(parser, TOKEN_RIGHT_PAREN);
  return paramList;
}

static void Import(roo_parser& parser)
{
  printf("--> Import\n");

  // Import local library
  if (MatchNext(TOKEN_IDENTIFIER) ||
      MatchNext(TOKEN_DOTTED_IDENTIFIER))
  {
    // TODO
    printf("Importing: %s\n", ExtractText(NextToken(parser)));
  }

  // Match a remote repository
  if (MatchNext(TOKEN_STRING))
  {
    printf("Importing remote: %s\n", ExtractText(NextToken(parser)));
  }

  NextToken(parser);
  printf("<-- Import\n");
}

static void Function(roo_parser& parser)
{
  printf("--> Function\n");
  function_def* definition = static_cast<function_def*>(malloc(sizeof(function_def)));
  definition->next = nullptr;

  // Find a place for the function
  if (parser.firstFunction)
  {
    function_def* tail = parser.firstFunction;

    while (tail->next)
    {
      tail = tail->next;
    }

    tail->next = definition;
  }
  else
  {
    parser.firstFunction = definition;
  }

  definition->name = ExtractText(NextToken(parser));
  printf("Function name: %s\n", definition->name);
  definition->params = ParameterList(parser);

  // TODO: parse a block
  Consume(parser, TOKEN_LEFT_BRACE);
  Consume(parser, TOKEN_RIGHT_BRACE);

  printf("<-- Function\n");
}

void Parse(roo_parser& parser)
{
  printf("--- Starting parse ---\n");

  while (!Match(TOKEN_INVALID))
  {
    PeekNPrint(parser);
    PeekNPrintNext(parser);

    if (Match(TOKEN_IMPORT))
    {
      Import(parser);
    }
  
    if (Match(TOKEN_FN))
    {
      Function(parser);
    }
  }

  printf("--- Finished parse ---\n");
}

void FreeParser(roo_parser& parser)
{
  free(parser.source);
  parser.source = nullptr;
  parser.currentChar = nullptr;

  function_def* temp;

  while (parser.firstFunction)
  {
    temp = parser.firstFunction;
    parser.firstFunction = parser.firstFunction->next;
    free(temp);
  }
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
    case TOKEN_IMPORT:
      return "TOKEN_IMPORT";

    case TOKEN_DOT:
      return "TOKEN_DOT";
    case TOKEN_COMMA:
      return "TOKEN_COMMA";
    case TOKEN_COLON:
      return "TOKEN_COLON";
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
