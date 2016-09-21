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
#include <ir.hpp>

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
  KEYWORD("if",       TOKEN_IF)
  KEYWORD("else",     TOKEN_ELSE)

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

  while ((*(parser.currentChar) != '\0') && (*(parser.currentChar) != '"'))
  {
    NextChar(parser);
  }

  ptrdiff_t length = (ptrdiff_t)((uintptr_t)parser.currentChar - (uintptr_t)startChar);
  unsigned int tokenOffset = (unsigned int)((uintptr_t)parser.currentChar - (uintptr_t)parser.source);

  // NOTE(Isaac): skip the ending '"' character
  NextChar(parser);

  return MakeToken(parser, TOKEN_STRING, tokenOffset, startChar, (unsigned int)length);
}

static token LexCharConstant(roo_parser& parser)
{
  const char* c = parser.currentChar;
  NextChar(parser);

  if (*(parser.currentChar) != '\'')
  {
    SyntaxError(parser, "Expected ' to end the char constant!\n");
  }

  return MakeToken(parser, TOKEN_CHAR_CONSTANT, (unsigned int)((uintptr_t)c - (uintptr_t)parser.source), c, 1u);
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
  
      case '~':
        type = TOKEN_TILDE;
        goto EmitSimpleToken;

      case '%':
        type = TOKEN_PERCENT;
        goto EmitSimpleToken;

      case '?':
        type = TOKEN_QUESTION_MARK;
        goto EmitSimpleToken;

      case '^':
        type = TOKEN_BITWISE_XOR;
        goto EmitSimpleToken;

      case '+':
      {
        if (*(parser.currentChar) == '+')
        {
          type = TOKEN_DOUBLE_PLUS;
          NextChar(parser);
        }
        else
        {
          type = TOKEN_PLUS;
        }

        goto EmitSimpleToken;
      }
 
      case '-':
      {
        if (*(parser.currentChar) == '>')
        {
          type = TOKEN_YIELDS;
          NextChar(parser);
        }
        else if (*(parser.currentChar) == '-')
        {
          type = TOKEN_DOUBLE_MINUS;
          NextChar(parser);
        }
        else
        {
          type = TOKEN_MINUS;
        }

        goto EmitSimpleToken;
      }
 
      case '=':
      {
        if (*(parser.currentChar) == '=')
        {
          type = TOKEN_EQUALS_EQUALS;
          NextChar(parser);
        }
        else
        {
          type = TOKEN_EQUALS;
        }

        goto EmitSimpleToken;
      }

      case '!':
      {
        if (*(parser.currentChar) == '=')
        {
          type = TOKEN_BANG_EQUALS;
          NextChar(parser);
        }
        else
        {
          type = TOKEN_BANG;
        }

        goto EmitSimpleToken;
      }

      case '>':
      {
        if (*(parser.currentChar) == '=')
        {
          type = TOKEN_GREATER_THAN_EQUAL_TO;
          NextChar(parser);
        }
        else if (*(parser.currentChar) == '>')
        {
          type = TOKEN_RIGHT_SHIFT;
          NextChar(parser);
        }
        else
        {
          type = TOKEN_GREATER_THAN;
        }

        goto EmitSimpleToken;
      }

      case '<':
      {
        if (*(parser.currentChar) == '=')
        {
          type = TOKEN_LESS_THAN_EQUAL_TO;
          NextChar(parser);
        }
        else if (*(parser.currentChar) == '<')
        {
          type = TOKEN_LEFT_SHIFT;
          NextChar(parser);
        }
        else
        {
          type = TOKEN_LESS_THAN;
        }

        goto EmitSimpleToken;
      }

      case '&':
      {
        if (*(parser.currentChar) == '&')
        {
          type = TOKEN_LOGICAL_AND;
          NextChar(parser);
        }
        else
        {
          type = TOKEN_BITWISE_AND;
        }

        goto EmitSimpleToken;
      }

      case '|':
      {
        if (*(parser.currentChar) == '|')
        {
          type = TOKEN_LOGICAL_OR;
          NextChar(parser);
        }
        else
        {
          type = TOKEN_BITWISE_OR;
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

      case '\'':
        return LexCharConstant(parser);

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

        // NOTE(Isaac): We aren't handling whatever character's next
        fprintf(stderr, "WARNING: Skipping unlexable character: '%c'\n", c);
      } break;
    }
  }

EmitSimpleToken:
  return MakeToken(parser, type, (uintptr_t)parser.currentChar - (uintptr_t)parser.source, nullptr, 0u);
}

static token NextToken(roo_parser& parser, bool ignoreLines = true)
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

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
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
#pragma GCC diagnostic pop

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
  prefix_parselet prefixParselet = parser.prefixMap[PeekToken(parser).type];

  if (!prefixParselet)
  {
    SyntaxError(parser, "Unexpected token in expression(PREFIX) position: %s!\n",
                GetTokenName(PeekToken(parser).type));
  }

  node* expression = prefixParselet(parser);

  while (precedence < parser.precedenceTable[PeekToken(parser, false).type])
  {
    infix_parselet infixParselet = parser.infixMap[PeekToken(parser, false).type];

    // NOTE(Isaac): there is no infix expression part - just return the prefix expression
    if (!infixParselet)
    {
      printf("<-- Expression\n");
      return expression;
    }

    expression = infixParselet(parser, expression);
  }

  printf("<-- Expression\n");
  return expression;
}

static type_ref TypeRef(roo_parser& parser)
{
  type_ref result;
  result.typeName = GetTextFromToken(PeekToken(parser));
  NextToken(parser);

  return result;
}

static variable_def* VariableDef(roo_parser& parser)
{
  variable_def* definition = static_cast<variable_def*>(malloc(sizeof(variable_def)));
  definition->name = GetTextFromToken(PeekToken(parser));
  definition->next = nullptr;

  ConsumeNext(parser, TOKEN_COLON);
  definition->type = TypeRef(parser);
  
  if (Match(parser, TOKEN_EQUALS))
  {
    Consume(parser, TOKEN_EQUALS);
    definition->initValue = Expression(parser);
    NextToken(parser);
  }
  else
  {
    definition->initValue = nullptr;
  }

  printf("Defined variable: %s of type: %s\n", definition->name, definition->type.typeName);
  return definition;
}

static node* Statement(roo_parser& parser, bool isInLoop = false);

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

static node* Condition(roo_parser& parser)
{
  printf("--> Condition\n");
  node* left = Expression(parser);

  token_type condition = PeekToken(parser).type;

  if ((condition != TOKEN_EQUALS_EQUALS)          &&
      (condition != TOKEN_BANG_EQUALS)            &&
      (condition != TOKEN_GREATER_THAN)           &&
      (condition != TOKEN_GREATER_THAN_EQUAL_TO)  &&
      (condition != TOKEN_LESS_THAN)              &&
      (condition != TOKEN_LESS_THAN_EQUAL_TO))
  {
    SyntaxError(parser, "Expected [CONDITION], got %s instead!", GetTokenName(condition));
  }

  NextToken(parser);
  node* right = Expression(parser);

  printf("<-- Condition\n");
  return CreateNode(CONDITION_NODE, condition, left, right);
}

static node* If(roo_parser& parser)
{
  printf("--> If\n");

  Consume(parser, TOKEN_IF);
  Consume(parser, TOKEN_LEFT_PAREN);
  node* condition = Condition(parser);
  Consume(parser, TOKEN_RIGHT_PAREN);

  node* thenCode = Block(parser);
  node* elseCode = nullptr;

  if (Match(parser, TOKEN_ELSE))
  {
    NextToken(parser);
    elseCode = Block(parser);
  }

  printf("<-- If\n");
  return CreateNode(IF_NODE, condition, thenCode, elseCode);
}

static node* Statement(roo_parser& parser, bool isInLoop)
{
  printf("--> Statement");
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
      printf("(BREAK)\n");
      NextToken(parser);
    } break;

    case TOKEN_RETURN:
    {
      printf("(RETURN)\n");
      parser.currentFunction->shouldAutoReturn = false;

      if (MatchNext(parser, TOKEN_LINE, false))
      {
        result = CreateNode(RETURN_NODE, nullptr);
      }
      else
      {
        NextToken(parser);
        result = CreateNode(RETURN_NODE, Expression(parser));
      }
    } break;

    case TOKEN_IF:
    {
      printf("(IF)\n");

      result = If(parser);
    } break;

    case TOKEN_IDENTIFIER:
    {
      printf("(IDENTIFIER)\n");

      // It's a variable definition (probably)
      if (MatchNext(parser, TOKEN_COLON))
      {
        variable_def* variable = VariableDef(parser);
        NextToken(parser);

        // Find somewhere to put it
        if (parser.currentFunction->firstLocal)
        {
          variable_def* tail = parser.currentFunction->firstLocal;

          while (tail->next)
          {
            tail = tail->next;
          }

          tail->next = variable;
        }
        else
        {
          parser.currentFunction->firstLocal = variable;
        }
      } break;
    } // NOTE(Isaac): no break

    default:
      printf("(EXPRESSION STATEMENT)\n");
      result = Expression(parser);

      // TODO(Isaac): make sure it's a valid node type to appear at top level
  }

  printf("<-- Statement\n");
  return result;
}

static void TypeDef(roo_parser& parser)
{
  printf("--> TypeDef(");
  Consume(parser, TOKEN_TYPE);
  type_def* type = static_cast<type_def*>(malloc(sizeof(type_def)));
  type->next = nullptr;

  // NOTE(Isaac): find a place for the type def
  if (parser.result->firstType)
  {
    type_def* tail = parser.result->firstType;

    while (tail->next)
    {
      tail = tail->next;
    }

    tail->next = type;
  }
  else
  {
    parser.result->firstType = type;
  }

  type->name = GetTextFromToken(PeekToken(parser));
  printf("%s)\n", type->name);
  type->firstMember = nullptr;
  
  ConsumeNext(parser, TOKEN_LEFT_BRACE);

  while (PeekToken(parser).type != TOKEN_RIGHT_BRACE)
  {
    variable_def* member = VariableDef(parser);

    if (type->firstMember)
    {
      variable_def* tail = type->firstMember;

      while (tail->next)
      {
        tail = tail->next;
      }

      tail->next = member;
    }
    else
    {
      type->firstMember = member;
    }
  }

  Consume(parser, TOKEN_RIGHT_BRACE);
  printf("<-- TypeDef\n");
}

static void Import(roo_parser& parser)
{
  printf("--> Import\n");
  Consume(parser, TOKEN_IMPORT);

  dependency_def* dependency = static_cast<dependency_def*>(malloc(sizeof(dependency_def)));
  dependency->next = nullptr;

  switch (PeekToken(parser).type)
  {
    // NOTE(Isaac): Import a local library
    case TOKEN_IDENTIFIER:
    {
      // TODO(Isaac): handle dotted identifiers
      printf("Importing: %s\n", GetTextFromToken(PeekToken(parser)));
      dependency->type = dependency_def::dependency_type::LOCAL;
      dependency->path = GetTextFromToken(PeekToken(parser));
    } break;

    // NOTE(Isaac): Import a library from a remote repository
    case TOKEN_STRING:
    {
      printf("Importing remote: %s\n", GetTextFromToken(PeekToken(parser)));
      dependency->type = dependency_def::dependency_type::REMOTE;
      dependency->path = GetTextFromToken(PeekToken(parser));
    } break;

    default:
    {
      SyntaxError(parser, "Expected [STRING LITERAL] or [DOTTED IDENTIFIER] after `import`, got %s!",
                  GetTokenName(PeekToken(parser).type));
    }
  }

  if (parser.result->firstDependency)
  {
    dependency_def* tail = parser.result->firstDependency;

    while (tail->next)
    {
      tail = tail->next;
    }

    tail->next = dependency;
  }
  else
  {
    parser.result->firstDependency = dependency;
  }

  NextToken(parser);
  printf("<-- Import\n");
}

static void Function(roo_parser& parser)
{
  printf("--> Function(");
  function_def* definition = static_cast<function_def*>(malloc(sizeof(function_def)));
  parser.currentFunction = definition;
  definition->shouldAutoReturn = true;
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
  printf("%s)\n", definition->name);
  definition->firstParam = ParameterList(parser);
  definition->firstLocal = nullptr;

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
    else if (Match(parser, TOKEN_FN))
    {
      Function(parser);
    }
    else if (Match(parser, TOKEN_TYPE))
    {
      TypeDef(parser);
    }
    else
    {
      SyntaxError(parser, "Unexpected token at top-level: %s!", GetTokenName(PeekToken(parser).type));
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

  parser.currentFunction = nullptr;

  // --- Parselets ---
  memset(parser.prefixMap, 0, sizeof(prefix_parselet) * NUM_TOKENS);
  memset(parser.infixMap, 0, sizeof(infix_parselet) * NUM_TOKENS);
  memset(parser.precedenceTable, 0, sizeof(unsigned int) * NUM_TOKENS);

  parser.prefixMap[TOKEN_IDENTIFIER] =
    [](roo_parser& parser) -> node*
    {
      printf("--> [PARSELET] Identifier\n");
      char* name = GetTextFromToken(PeekToken(parser));

      NextToken(parser);
      printf("<-- [PARSELET] Identifier\n");
      return CreateNode(VARIABLE_NODE, name);
    };

  parser.prefixMap[TOKEN_NUMBER_INT] =
    [](roo_parser& parser) -> node*
    {
      printf("--> [PARSELET] Number constant (integer)\n");
      char* tokenText = GetTextFromToken(PeekToken(parser));
      int value = strtol(tokenText, nullptr, 10); // TODO(Isaac): parse hexadecimal here too
      free(tokenText);

      NextToken(parser);
      printf("<-- [PARSELET] Number constant (integer)\n");
      return CreateNode(NUMBER_CONSTANT_NODE, number_constant_part::constant_type::CONSTANT_TYPE_INT, value);
    };

  parser.prefixMap[TOKEN_NUMBER_FLOAT] =
    [](roo_parser& parser) -> node*
    {
      printf("--> [PARSELET] Number constant (floating point)\n");
      char* tokenText = GetTextFromToken(PeekToken(parser));
      float value = strtof(tokenText, nullptr);
      free(tokenText);

      NextToken(parser);
      printf("<-- [PARSELET] Number constant (floating point)\n");
      return CreateNode(NUMBER_CONSTANT_NODE, number_constant_part::constant_type::CONSTANT_TYPE_FLOAT, value);
    };

  parser.prefixMap[TOKEN_STRING] =
    [](roo_parser& parser) -> node*
    {
      printf("--> [PARSELET] String\n");
      char* tokenText = GetTextFromToken(PeekToken(parser));
      NextToken(parser);

      printf("<-- [PARSELET] String\n");
      return CreateNode(STRING_CONSTANT_NODE, CreateStringConstant(parser.result, tokenText));
    };

  parser.infixMap[TOKEN_PLUS] =
  parser.infixMap[TOKEN_MINUS] =
  parser.infixMap[TOKEN_ASTERIX] =
  parser.infixMap[TOKEN_SLASH] =
    [](roo_parser& parser, node* left) -> node*
    {
      printf("--> [PARSELET] Binary operator (%s)\n", GetTokenName(PeekToken(parser).type));
      token_type operation = PeekToken(parser).type;

      NextToken(parser);
      printf("<-- [PARSELET] Binary operator\n");
      return CreateNode(BINARY_OP_NODE, operation, left, Expression(parser, parser.precedenceTable[operation]));
    };

  // Parses a function call
  parser.infixMap[TOKEN_LEFT_PAREN] =
    [](roo_parser& parser, node* left) -> node*
    {
      printf("--> [PARSELET] Function Call\n");

      if (left->type != VARIABLE_NODE)
      {
        SyntaxError(parser, "Unrecognised function name!");
      }

      char* functionName = static_cast<char*>(malloc(sizeof(char) * strlen(left->payload.variable.name)));
      strcpy(functionName, left->payload.variable.name);
      FreeNode(left);
      free(left);

      function_call_part::param_def* firstParam = nullptr;
      Consume(parser, TOKEN_LEFT_PAREN);

      while (!Match(parser, TOKEN_RIGHT_PAREN))
      {
        function_call_part::param_def* param = static_cast<function_call_part::param_def*>(malloc(sizeof(function_call_part::param_def)));
        NextToken(parser);
        param->expression = Expression(parser);
        param->next = nullptr;

        if (firstParam)
        {
          function_call_part::param_def* tail = firstParam;

          while (tail->next)
          {
            tail = tail->next;
          }

          tail->next = param;
        }
        else
        {
          firstParam = param;
        }
      }

      Consume(parser, TOKEN_RIGHT_PAREN);
      printf("<-- [PARSELET] Function call\n");
      return CreateNode(FUNCTION_CALL_NODE, functionName, firstParam);
    };

  // --- Precedence table ---
  /*
   * NOTE(Isaac): This is mostly the same as C++'s operator precedence, for maximum intuitiveness
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
    P_PREFIX,                   // !, ~, +x, -x, ++x, --x, &, *, x(...)
    P_SUFFIX,                   // x++, x--
  };
  
  parser.precedenceTable[TOKEN_PLUS]          = P_ADDITIVE;
  parser.precedenceTable[TOKEN_MINUS]         = P_ADDITIVE;
  parser.precedenceTable[TOKEN_ASTERIX]       = P_MULTIPLICATIVE;
  parser.precedenceTable[TOKEN_SLASH]         = P_MULTIPLICATIVE;
  parser.precedenceTable[TOKEN_LEFT_PAREN]    = P_PREFIX;
}

void FreeParser(roo_parser& parser)
{
  free(parser.source);
  parser.source = nullptr;
  parser.currentChar = nullptr;
  parser.result = nullptr;
}
