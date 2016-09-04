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

void FreeParseResult(parse_result& result)
{
  function_def* temp;

  while (result.firstFunction)
  {
    temp = result.firstFunction;
    FreeFunctionDef(temp);
    result.firstFunction = result.firstFunction->next;
    free(temp);
  }

  result.firstFunction = nullptr;
}

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
  {
    return '\0';
  }

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

static bool IsHexDigit(char c)
{
  return ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'));
}

static inline token MakeToken(roo_parser& parser, token_type type, unsigned int offset, const char* startChar,
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
  {
    NextChar(parser);
  }

  ptrdiff_t length = (ptrdiff_t)((uintptr_t)parser.currentChar - (uintptr_t)startChar);
  unsigned int tokenOffset = (unsigned int)((uintptr_t)parser.currentChar - (uintptr_t)parser.source);

  // Parse a keyword
  #define KEYWORD(keyword, tokenType) \
    if (memcmp(startChar, keyword, length) == 0) \
    { \
      return MakeToken(parser, tokenType, tokenOffset, startChar, (unsigned int)length); \
    }

  KEYWORD("type",     TOKEN_TYPE)
  KEYWORD("fn",       TOKEN_FN)
  KEYWORD("true",     TOKEN_TRUE)
  KEYWORD("false",    TOKEN_FALSE)
  KEYWORD("import",   TOKEN_IMPORT)
  KEYWORD("break",    TOKEN_BREAK)
  KEYWORD("return",   TOKEN_RETURN)

  // It's not a keyword, so create an identifier token
  return MakeToken(parser, TOKEN_IDENTIFIER, tokenOffset, startChar, (unsigned int)length);
}

static token LexNumber(roo_parser& parser)
{
  const char* startChar = parser.currentChar - 1u;
  token_type type = TOKEN_NUMBER_INT;

  while (IsDigit(*(parser.currentChar)))
  {
    NextChar(parser);
  }

  // Check for a decimal point
  if (*(parser.currentChar) == '.' && IsDigit(*(parser.currentChar + 1u)))
  {
    NextChar(parser);
    type = TOKEN_NUMBER_FLOAT;

    while (IsDigit(*(parser.currentChar)))
    {
      NextChar(parser);
    }
  }

  ptrdiff_t length = (ptrdiff_t)((uintptr_t)parser.currentChar - (uintptr_t)startChar);
  unsigned int tokenOffset = (unsigned int)((uintptr_t)parser.currentChar - (uintptr_t)parser.source);
  return MakeToken(parser, type, tokenOffset, startChar, (unsigned int) length);
}

static token LexHexNumber(roo_parser& parser)
{
  NextChar(parser); // NOTE(Isaac): skip over the 'x'
  const char* startChar = parser.currentChar;

  while (IsHexDigit(*(parser.currentChar)))
  {
    NextChar(parser);
  }

  ptrdiff_t length = (ptrdiff_t)((uintptr_t)parser.currentChar - (uintptr_t)startChar);
  unsigned int tokenOffset = (unsigned int)((uintptr_t)parser.currentChar - (uintptr_t)parser.source);
  return MakeToken(parser, TOKEN_NUMBER_INT, tokenOffset, startChar, (unsigned int)length);
}

static token LexString(roo_parser& parser)
{
  const char* startChar = parser.currentChar;

  while (*(parser.currentChar) != '"')
  {
    NextChar(parser);
  }

  ptrdiff_t length = (ptrdiff_t)((uintptr_t)parser.currentChar - (uintptr_t)startChar);
  unsigned int tokenOffset = (unsigned int)((uintptr_t)parser.currentChar - (uintptr_t)parser.source);
  return MakeToken(parser, TOKEN_STRING, tokenOffset, startChar, (unsigned int)length);
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
      {
        if (*(parser.currentChar) == '>')
        {
          type = TOKEN_YIELDS;
        }
        else
        {
          type = TOKEN_MINUS;
        }

        goto EmitSimpleToken;
      }
  
      case '/':
        type = TOKEN_SLASH;
        goto EmitSimpleToken;
  
      case ' ':
      case '\r':
      case '\t':
      {
        // Skip past any whitespace
        while (*(parser.currentChar) == ' '  ||
               *(parser.currentChar) == '\r' ||
               *(parser.currentChar) == '\t' ||
               *(parser.currentChar) == '\n')
        {
          NextChar(parser);
        }
      } break;

      case '\n':
        type = TOKEN_LINE;
        goto EmitSimpleToken;

      case '"':
        return LexString(parser);

      default:
      {
        if (IsName(c))
        {
          return LexName(parser);
        }
    
        if (c == '0' && *(parser.currentChar) == 'x')
        {
          return LexHexNumber(parser);
        }
    
        if (IsDigit(c))
        {
          return LexNumber(parser);
        }
      } break;
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
    }
  }

  return parser.currentToken;
}

// TODO(Isaac): should we remove this? It can be useful but because reasons (see below), it's a pain in the ass
static token PeekNextToken(roo_parser& parser, bool ignoreLines = true)
{
  if (!ignoreLines)
  {
    return parser.nextToken;
  }

  // NOTE(Isaac): We need to skip tokens denoting line breaks, without advancing the token stream
  const char* cachedChar = parser.currentChar;
  unsigned int cachedLine = parser.currentLine;
  unsigned int cachedLineOffset = parser.currentLineOffset;

  token next = parser.nextToken;

  while (next.type == TOKEN_LINE)
  {
    next = LexNext(parser);
  }

  parser.currentChar = cachedChar;
  parser.currentLine = cachedLine;
  parser.currentLineOffset = cachedLineOffset;

  return next;
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
    printf("PEEK: (%s)\n", GetTextFromToken(PeekToken(parser, ignoreLines)));
  }
  else if ((PeekToken(parser, ignoreLines).type == TOKEN_NUMBER_INT) ||
           (PeekToken(parser, ignoreLines).type == TOKEN_NUMBER_FLOAT))
  {
    printf("PEEK: [%s]\n", GetTextFromToken(PeekToken(parser, ignoreLines)));
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
    printf("PEEK_NEXT: (%s)\n", GetTextFromToken(PeekNextToken(parser, ignoreLines)));
  }
  else if ((PeekNextToken(parser, ignoreLines).type == TOKEN_NUMBER_INT) ||
           (PeekNextToken(parser, ignoreLines).type == TOKEN_NUMBER_FLOAT))
  {
    printf("PEEK_NEXT: [%s]\n", GetTextFromToken(PeekNextToken(parser, ignoreLines)));
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

static inline bool Match(roo_parser& parser, token_type expectedType, bool ignoreLines = true)
{
  return (PeekToken(parser, ignoreLines).type == expectedType);
}

static inline bool MatchNext(roo_parser& parser, token_type expectedType, bool ignoreLines = true)
{
  return (PeekNextToken(parser, ignoreLines).type == expectedType);
}

// --- Parsing ---
static parameter_def* ParameterList(roo_parser& parser)
{
  ConsumeNext(parser, TOKEN_LEFT_PAREN);
  parameter_def* paramList = nullptr;

  // Check for an empty parameter list
  if (Match(parser, TOKEN_RIGHT_PAREN))
  {
    Consume(parser, TOKEN_RIGHT_PAREN);
    return nullptr;
  }

  do
  {
    parameter_def* param = static_cast<parameter_def*>(malloc(sizeof(parameter_def)));
    param->name = GetTextFromToken(PeekToken(parser));
    ConsumeNext(parser, TOKEN_COLON);
    param->typeName = GetTextFromToken(PeekToken(parser));
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
  } while (MatchNext(parser, TOKEN_COMMA));

  ConsumeNext(parser, TOKEN_RIGHT_PAREN);
  return paramList;
}

static node* Expression(roo_parser& parser, unsigned int precedence = 0u)
{
  printf("--> Expression\n");
  prefix_parselet prefixParselet = parser.prefixMap[NextToken(parser).type];

  if (!prefixParselet)
  {
    SyntaxError(parser, "Unexpected token in expression position: %s!\n", GetTokenName(PeekToken(parser).type));
  }

  node* expression = prefixParselet(parser);

  while (precedence < parser.precedenceTable[PeekNextToken(parser, false).type])
  {
    infix_parselet infixParselet = parser.infixMap[PeekNextToken(parser, false).type];

    // NOTE(Isaac): there is no infix expression part - just return the prefix expression
    if (!infixParselet)
    {
      printf("<-- Expression\n");
      return expression;
    }

    NextToken(parser);
    expression = infixParselet(parser, expression);
  }

  printf("<-- Expression\n");
  return expression;
}

static node* Statement(roo_parser& parser, bool isInLoop)
{
  printf("--> Statement(");
  node* result = nullptr;

  switch (PeekToken(parser).type)
  {
    case TOKEN_BREAK:
    {
      if (!isInLoop)
      {
        SyntaxError(parser, "`break` only makes sense in a loop!\n");
      }

      result = CreateNode(BREAK_NODE);
      printf("BREAK)\n");
      NextToken(parser);
    } break;

    case TOKEN_RETURN:
    {
      printf("RETURN)\n");

      if (MatchNext(parser, TOKEN_LINE, false))
      {
        result = CreateNode(RETURN_NODE, nullptr);
      }
      else
      {
        result = CreateNode(RETURN_NODE, Expression(parser));
      }

      NextToken(parser);
    } break;

    default:
      fprintf(stderr, "Unhandled token type in Statement: %s!\n", GetTokenName(PeekToken(parser).type));
      exit(1);
  }

  printf("<-- Statement\n");
  return result;
}

static node* Block(roo_parser& parser)
{
  printf("--> Block\n");

  Consume(parser, TOKEN_LEFT_BRACE);
  node* code = nullptr;

  while (!Match(parser, TOKEN_RIGHT_BRACE))
  {
    node* statement = Statement(parser, false);

    if (code)
    {
      node* tail = statement;

      while (tail->next)
      {
        tail = tail->next;
      }

      tail->next = statement;
    }
    else
    {
      code = statement;
    }
  }

  Consume(parser, TOKEN_RIGHT_BRACE);
  printf("<-- Block\n");
  return code;
}

static void Import(roo_parser& parser)
{
  printf("--> Import\n");

  switch (parser.nextToken.type)
  {
    // NOTE(Isaac): Import a local library
    case TOKEN_IDENTIFIER:
    case TOKEN_DOTTED_IDENTIFIER:
    {
      printf("Importing: %s\n", GetTextFromToken(NextToken(parser)));
    } break;

    // NOTE(Isaac): Import a library from a remote repository
    case TOKEN_STRING:
    {
      printf("Importing remote: %s\n", GetTextFromToken(NextToken(parser)));
    } break;

    default:
    {
      SyntaxError(parser, "Expected [STRING LITERAL] or [DOTTED IDENTIFIER] after `import`, got %s!",
                  GetTokenName(parser.nextToken.type));
    }
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
  if (parser.result->firstFunction)
  {
    function_def* tail = parser.result->firstFunction;

    while (tail->next)
    {
      tail = tail->next;
    }

    tail->next = definition;
  }
  else
  {
    parser.result->firstFunction = definition;
  }

  definition->name = GetTextFromToken(NextToken(parser));
  printf("Function name: %s\n", definition->name);
  definition->params = ParameterList(parser);

  // Optionally parse a return type
  if (Match(parser, TOKEN_YIELDS))
  {
    Consume(parser, TOKEN_YIELDS);
    definition->returnType = static_cast<type_ref*>(malloc(sizeof(type_ref)));
    definition->returnType->typeName = GetTextFromToken(PeekToken(parser));
    NextToken(parser);

    printf("Return type: %s\n", definition->returnType->typeName);
  }
  else
  {
    definition->returnType = nullptr;
    printf("Return type: NONE\n");
  }

  definition->code = Block(parser);

  printf("<-- Function\n");
}

void Parse(roo_parser& parser)
{
  printf("--- Starting parse ---\n");

  while (!Match(parser, TOKEN_INVALID))
  {
    if (Match(parser, TOKEN_IMPORT))
    {
      Import(parser);
    }
  
    if (Match(parser, TOKEN_FN))
    {
      Function(parser);
    }
  }

  printf("--- Finished parse ---\n");
}

void CreateParser(roo_parser& parser, parse_result* result, const char* sourcePath)
{
  parser.source = ReadFile(sourcePath);
  parser.currentChar = parser.source;
  parser.currentLine = 0u;
  parser.currentLineOffset = 0u;
  parser.result = result;

  parser.currentToken = LexNext(parser);
  parser.nextToken = LexNext(parser);

  // --- Parselets ---
  memset(parser.prefixMap, 0, sizeof(prefix_parselet) * NUM_TOKENS);
  memset(parser.infixMap, 0, sizeof(infix_parselet) * NUM_TOKENS);
  memset(parser.precedenceTable, 0, sizeof(unsigned int) * NUM_TOKENS);

  parser.prefixMap[TOKEN_IDENTIFIER] =
    [](roo_parser& parser) -> node*
    {
      printf("Prefix parselet: TOKEN_IDENTIFIER!\n");
      return nullptr;
    };

  parser.prefixMap[TOKEN_NUMBER_INT] =
    [](roo_parser& parser) -> node*
    {
      printf("Prefix parselet: TOKEN_NUMBER_INT\n");
      return nullptr;
    };

  parser.prefixMap[TOKEN_NUMBER_FLOAT] =
    [](roo_parser& parser) -> node*
    {
      printf("Prefix parselet: TOKEN_NUMBER_FLOAT\n");
      return nullptr;
    };

  parser.infixMap[TOKEN_PLUS] =
  parser.infixMap[TOKEN_MINUS] =
  parser.infixMap[TOKEN_ASTERIX] =
  parser.infixMap[TOKEN_SLASH] =
    [](roo_parser& parser, node* left) -> node*
    {
      printf("Infix parselet: %s\n", GetTokenName(PeekToken(parser).type));
      return CreateNode(BINARY_OP_NODE, PeekToken(parser).type, left,
                        Expression(parser, parser.precedenceTable[PeekToken(parser).type]));
    };

  // --- Precedence table ---
  /*
   * NOTE(Isaac): This is mostly a copy of C++'s operator precedence, for maximum intuitiveness
   */
  enum Precedence
  {
    P_TERNARY,                  // a?b:c
    P_LOGICAL_OR,               // ||
    P_LOGICAL_AND,              // &&
    P_BITWISE_OR,               // |
    P_BITWISE_XOR,              // ^
    P_BITWISE_AND,              // &
    P_EQUALS_RELATIONAL,        // == and !=
    P_COMPARATIVE_RELATIONAL,   // <, <=, > and >=
    P_BITWISE_SHIFTING,         // >> and <<
    P_ADDITIVE,                 // + and -
    P_MULTIPLICATIVE,           // *, / and %
    P_PREFIX,                   // !, ~, +x, -x, ++x, --x, &, *
    P_SUFFIX,                   // x++, x--
  };
  
  parser.precedenceTable[TOKEN_PLUS]    = P_ADDITIVE;
  parser.precedenceTable[TOKEN_MINUS]   = P_ADDITIVE;
  parser.precedenceTable[TOKEN_ASTERIX] = P_MULTIPLICATIVE;
  parser.precedenceTable[TOKEN_SLASH]   = P_MULTIPLICATIVE;
}

void FreeParser(roo_parser& parser)
{
  free(parser.source);
  parser.source = nullptr;
  parser.currentChar = nullptr;
  parser.result = nullptr;
}
