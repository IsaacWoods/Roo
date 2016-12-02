/*
 * Copyright (C) 2016, Isaac Woods. All rights reserved.
 */

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cstddef>
#include <cstdint>
#include <cstdarg>
#include <climits>
#include <cassert>
#include <ir.hpp>
#include <ast.hpp>

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
    int   i;    // Valid if token type is TOKEN_NUMBER_INT
    float f;    // Valid if token type is TOKEN_NUMBER_FLOAT
  } payload;
};

struct roo_parser
{
  char*           source;
  const char*     currentChar; // NOTE(Isaac): this points into `source`
  unsigned int    currentLine;
  unsigned int    currentLineOffset;

  token           currentToken;
  token           nextToken;

  parse_result*   result;
  function_def*   currentFunction;
};

static inline char* GetTextFromToken(const token& tkn)
{
  // NOTE(Isaac): this is the upper bound of the amount of memory we need to store the string representation
  char* text = static_cast<char*>(malloc(sizeof(char) * (tkn.textLength + 1u)));

  // We need to manually escape string constants into actually correct chars
  if (tkn.type == TOKEN_STRING)
  {
    unsigned int i = 0u;

    for (const char* c = tkn.textStart;
         c < (tkn.textStart + static_cast<uintptr_t>(tkn.textLength));
         c++)
    {
      if (*c == '\\')
      {
        switch (*(++c))
        {
          case '0':
          {
            text[i++] = '\0';
          } break;

          case 'n':
          {
            text[i++] = '\n';
          } break;

          case 't':
          {
            text[i++] = '\t';
          } break;

          case '\\':
          {
            text[i++] = '\\';
          } break;

          default:
          {
            // TODO: use real syntax error system
            fprintf(stderr, "ERROR: Unrecognised escape sequence found '\\%c'!\n", *c);
            exit(1);
          }
        }
      }
      else
      {
        text[i++] = *c;
      }
    }

    text[i] = '\0';
  }
  else
  {
    // NOTE(Isaac): In this case, we just need to shove the thing into the other thing
    memcpy(text, tkn.textStart, tkn.textLength);
    text[tkn.textLength] = '\0';
  }

  return text;
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

  token tkn = MakeToken(parser, type, tokenOffset, startChar, (unsigned int)length);
  char* text = GetTextFromToken(tkn);

  switch (type)
  {
    case TOKEN_NUMBER_INT:
    {
      tkn.payload.i = strtol(text, nullptr, 10);
    } break;

    case TOKEN_NUMBER_FLOAT:
    {
      tkn.payload.f = strtof(text, nullptr);
    } break;

    default:
    {
      assert(false);
    }
  }

  free(text);
  return tkn;
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

  token tkn = MakeToken(parser, TOKEN_NUMBER_INT, tokenOffset, startChar, (unsigned int)length);
  char* text = GetTextFromToken(tkn);
  tkn.payload.i = strtol(text, nullptr, 16);
  free(text);

  return tkn;
}

static token LexBinaryNumber(roo_parser& parser)
{
  NextChar(parser); // NOTE(Isaac): skip over the 'b'
  const char* startChar = parser.currentChar;

  while (*(parser.currentChar) == '0' ||
         *(parser.currentChar) == '1')
  {
    NextChar(parser);
  }

  ptrdiff_t length = (ptrdiff_t)((uintptr_t)parser.currentChar - (uintptr_t)startChar);
  unsigned int tokenOffset = (unsigned int)((uintptr_t)parser.currentChar - (uintptr_t)parser.source);

  token tkn = MakeToken(parser, TOKEN_NUMBER_INT, tokenOffset, startChar, (unsigned int)length);
  char* text = GetTextFromToken(tkn);
  tkn.payload.i = strtol(text, nullptr, 2);
  free(text);

  return tkn;
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

      case '#':
      {
        if (*(parser.currentChar) == '[')
        {
          type = TOKEN_START_ATTRIBUTE;
          NextChar(parser);
        }
        else
        {
          type = TOKEN_POUND;
        }

        goto EmitSimpleToken;
      }

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

        if (c == '0' && *(parser.currentChar) == 'b')
        {
          return LexBinaryNumber(parser);
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

#if 0
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
#endif

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
typedef node* (*prefix_parselet)(roo_parser&);
typedef node* (*infix_parselet)(roo_parser&, node*);

prefix_parselet g_prefixMap[NUM_TOKENS];
infix_parselet  g_infixMap[NUM_TOKENS];
unsigned int    g_precedenceTable[NUM_TOKENS];

// TODO: doesn't appear to work for multiple parameters
static void ParameterList(roo_parser& parser, linked_list<variable_def*>& params)
{
  ConsumeNext(parser, TOKEN_LEFT_PAREN);

  // Check for an empty parameter list
  if (Match(parser, TOKEN_RIGHT_PAREN))
  {
    Consume(parser, TOKEN_RIGHT_PAREN);
    return;
  }

  do
  {
    char* varName = GetTextFromToken(PeekToken(parser));
    ConsumeNext(parser, TOKEN_COLON);
    char* typeName = GetTextFromToken(PeekToken(parser));

    variable_def* param = CreateVariableDef(varName, typeName, nullptr);

    printf("Param: %s of type %s\n", param->name, param->type.type.name);
    AddToLinkedList<variable_def*>(params, param);
  } while (MatchNext(parser, TOKEN_COMMA));

  ConsumeNext(parser, TOKEN_RIGHT_PAREN);
}

static node* Expression(roo_parser& parser, unsigned int precedence = 0u)
{
  printf("--> Expression(%u)\n", precedence);
  prefix_parselet prefixParselet = g_prefixMap[PeekToken(parser).type];

  if (!prefixParselet)
  {
    SyntaxError(parser, "Unexpected token in expression(PREFIX) position: %s!\n",
                GetTokenName(PeekToken(parser).type));
  }

  node* expression = prefixParselet(parser);

  while (precedence < g_precedenceTable[PeekToken(parser, false).type])
  {
    infix_parselet infixParselet = g_infixMap[PeekToken(parser, false).type];

    // NOTE(Isaac): there is no infix expression part - just return the prefix expression
    if (!infixParselet)
    {
      printf("<-- Expression(NO INFIX)\n");
      return expression;
    }

    expression = infixParselet(parser, expression);
  }

  printf("<-- Expression\n");
  return expression;
}

static variable_def* VariableDef(roo_parser& parser)
{
  char* name = GetTextFromToken(PeekToken(parser));
  ConsumeNext(parser, TOKEN_COLON);
  char* typeName = GetTextFromToken(PeekToken(parser));
  node* initValue;

  if (MatchNext(parser, TOKEN_EQUALS))
  {
    ConsumeNext(parser, TOKEN_EQUALS);
    initValue = Expression(parser);
  }
  else
  {
    initValue = nullptr;
    NextToken(parser);
  }

  variable_def* variable = CreateVariableDef(name, typeName, initValue);
  printf("Defined variable: %s of type: %s\n", variable->name, variable->type.type.name);
  return variable;
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
      node* tail = code;

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
      // It's a variable definition (probably)
      if (MatchNext(parser, TOKEN_COLON))
      {
        printf("(VARIABLE DEFINITION)\n");
        variable_def* variable = VariableDef(parser);

        // Assign the initial value to the variable
        result = CreateNode(VARIABLE_ASSIGN_NODE, variable->name, variable->initValue);
        result->payload.variableAssignment.variable = variable;

        AddToLinkedList<variable_def*>(parser.currentFunction->locals, variable);
        break;
      }
    } // NOTE(Isaac): no break

    default:
      printf("(EXPRESSION STATEMENT)\n");
      result = Expression(parser);

      // TODO(Isaac): make sure it's a valid node type to appear at top level
  }

  printf("<-- Statement\n");
  return result;
}

static void TypeDef(roo_parser& parser, linked_list<type_attrib>& attribs)
{
  printf("--> TypeDef(");
  Consume(parser, TOKEN_TYPE);
  type_def* type = static_cast<type_def*>(malloc(sizeof(type_def)));
  CreateLinkedList<variable_def*>(type->members);
  type->size = UINT_MAX;

  CreateLinkedList<type_attrib>(type->attribs);
  CopyLinkedList<type_attrib>(type->attribs, attribs);

  type->name = GetTextFromToken(PeekToken(parser));
  printf("%s)\n", type->name);
  
  ConsumeNext(parser, TOKEN_LEFT_BRACE);

  while (PeekToken(parser).type != TOKEN_RIGHT_BRACE)
  {
    variable_def* member = VariableDef(parser);
    AddToLinkedList<variable_def*>(type->members, member);
  }

  Consume(parser, TOKEN_RIGHT_BRACE);
  AddToLinkedList<type_def*>(parser.result->types, type);
  printf("<-- TypeDef\n");
}

static void Import(roo_parser& parser)
{
  printf("--> Import\n");
  Consume(parser, TOKEN_IMPORT);

  dependency_def* dependency = static_cast<dependency_def*>(malloc(sizeof(dependency_def)));

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

  AddToLinkedList<dependency_def*>(parser.result->dependencies, dependency);
  NextToken(parser);
  printf("<-- Import\n");
}

static void Function(roo_parser& parser, linked_list<function_attrib>& attribs)
{
  printf("--> Function(");
  function_def* definition = static_cast<function_def*>(malloc(sizeof(function_def)));
  parser.currentFunction = definition;
  CreateLinkedList<variable_def*>(definition->params);
  CreateLinkedList<variable_def*>(definition->locals);
  definition->shouldAutoReturn = true;
  definition->air = nullptr;

  CreateLinkedList<function_attrib>(definition->attribs);
  CopyLinkedList<function_attrib>(definition->attribs, attribs);
  FreeLinkedList<function_attrib>(attribs);

  AddToLinkedList<function_def*>(parser.result->functions, definition);

  definition->name = GetTextFromToken(NextToken(parser));
  printf("%s)\n", definition->name);

  ParameterList(parser, definition->params);

  // Optionally parse a return type
  if (Match(parser, TOKEN_YIELDS))
  {
    Consume(parser, TOKEN_YIELDS);
    definition->returnType = static_cast<type_ref*>(malloc(sizeof(type_ref)));
    definition->returnType->type.name = GetTextFromToken(PeekToken(parser));
    definition->returnType->isResolved = false;
    NextToken(parser);

    printf("Return type: %s\n", definition->returnType->type.name);
  }
  else
  {
    definition->returnType = nullptr;
    printf("Return type: NONE\n");
  }

  if (GetAttrib(definition, function_attrib::attrib_type::PROTOTYPE))
  {
    definition->isPrototype = true;
    definition->ast = nullptr;
  }
  else
  {
    definition->isPrototype = false;
    definition->ast = Block(parser);
  }

  printf("<-- Function\n");
}

enum class attrib_type
{
  NONE,
  FUNCTION,
  TYPE,
  STATEMENT,
  PROGRAM
};

/*
 * NOTE(Isaac): this will parse all types of attribute, and then return the type of the attrib parsed
 */
static attrib_type Attribute(roo_parser& parser, linked_list<program_attrib>& programAttribs,
                                                 linked_list<function_attrib>& functionAttribs,
                                                 linked_list<type_attrib>& typeAttribs)
{
  attrib_type type = attrib_type::NONE;
  char* attribName = GetTextFromToken(NextToken(parser));

  if (strcmp(attribName, "Entry") == 0)
  {
    function_attrib attrib;
    attrib.type = function_attrib::attrib_type::ENTRY;

    AddToLinkedList<function_attrib>(functionAttribs, attrib);
    type = attrib_type::FUNCTION;
    NextToken(parser);
  }
  else if (strcmp(attribName, "Name") == 0)
  {
    ConsumeNext(parser, TOKEN_LEFT_PAREN);

    if (!Match(parser, TOKEN_IDENTIFIER))
    {
      SyntaxError(parser, "Expected identifier as program name!");
    }

    program_attrib attrib;
    attrib.type = program_attrib::attrib_type::NAME;
    attrib.payload.name = GetTextFromToken(PeekToken(parser));

    ConsumeNext(parser, TOKEN_RIGHT_PAREN);

    AddToLinkedList<program_attrib>(programAttribs, attrib);
    type = attrib_type::PROGRAM;
  }
  else if (strcmp(attribName, "Prototype") == 0)
  {
    function_attrib attrib;
    attrib.type = function_attrib::attrib_type::PROTOTYPE;

    AddToLinkedList<function_attrib>(functionAttribs, attrib);
    type = attrib_type::FUNCTION;
    NextToken(parser);
  }
  else
  {
    SyntaxError(parser, "Unrecognised attribute: '%s'!", attribName);
  }

  Consume(parser, TOKEN_RIGHT_BLOCK);
  free(attribName);
  return type;
}

void Parse(parse_result* result, const char* sourcePath)
{
  roo_parser parser;
  parser.source = ReadFile(sourcePath);
  parser.currentChar = parser.source;
  parser.currentLine = 0u;
  parser.currentLineOffset = 0u;
  parser.result = result;

  parser.currentToken = LexNext(parser);
  parser.nextToken    = LexNext(parser);

  printf("--- Starting parse ---\n");

  linked_list<function_attrib> functionAttribs;
  linked_list<type_attrib> typeAttribs;

  CreateLinkedList<function_attrib>(functionAttribs);
  CreateLinkedList<type_attrib>(typeAttribs);

  attrib_type parsedAttribType = attrib_type::NONE;

  while (!Match(parser, TOKEN_INVALID))
  {
    if (Match(parser, TOKEN_IMPORT))
    {
      Import(parser);
    }
    else if (Match(parser, TOKEN_FN))
    {
      if (parsedAttribType != attrib_type::NONE &&
          parsedAttribType != attrib_type::FUNCTION)
      {
        SyntaxError(parser, "Unexpected attribute to be applied to a function!");
      }

      Function(parser, functionAttribs);
      UnlinkLinkedList<function_attrib>(functionAttribs);
      parsedAttribType = attrib_type::NONE;
    }
    else if (Match(parser, TOKEN_TYPE))
    {
      if (parsedAttribType != attrib_type::NONE &&
          parsedAttribType != attrib_type::TYPE)
      {
        SyntaxError(parser, "Unexpected attibute to be applied to a type!");
      }

      TypeDef(parser, typeAttribs);
      UnlinkLinkedList<type_attrib>(typeAttribs);
      parsedAttribType = attrib_type::NONE;
    }
    else if (Match(parser, TOKEN_START_ATTRIBUTE))
    {
      Attribute(parser, result->attribs, functionAttribs, typeAttribs);
    }
    else
    {
      SyntaxError(parser, "Unexpected token at top-level: %s!", GetTokenName(PeekToken(parser).type));
    }
  }

  if (parsedAttribType != attrib_type::NONE)
  {
    SyntaxError(parser, "Trailing attribute not applied to anything!");
  }

  printf("--- Finished parse ---\n");

  free(parser.source);
  parser.source = nullptr;
  parser.currentChar = nullptr;
  parser.result = nullptr;
}

void InitParseletMaps()
{
  // --- Precedence table ---
  memset(g_precedenceTable, 0, sizeof(unsigned int) * NUM_TOKENS);

  /*
   * NOTE(Isaac): This is mostly the same as C++'s operator precedence, for maximum intuitiveness
   * NOTE(Isaac): Precedence starts at 1, as a precedence of 0 has special meaning
   */
  enum Precedence
  {
    P_ASSIGNMENT = 1u,          // =
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
    P_MEMBER_ACCESS,            // x.y
  };
  
  g_precedenceTable[TOKEN_EQUALS]     = P_ASSIGNMENT;
  g_precedenceTable[TOKEN_PLUS]       = P_ADDITIVE;
  g_precedenceTable[TOKEN_MINUS]      = P_ADDITIVE;
  g_precedenceTable[TOKEN_ASTERIX]    = P_MULTIPLICATIVE;
  g_precedenceTable[TOKEN_SLASH]      = P_MULTIPLICATIVE;
  g_precedenceTable[TOKEN_LEFT_PAREN] = P_PREFIX;
  g_precedenceTable[TOKEN_DOT]        = P_MEMBER_ACCESS;

  // --- Parselets ---
  memset(g_prefixMap, 0, sizeof(prefix_parselet) * NUM_TOKENS);
  memset(g_infixMap, 0, sizeof(infix_parselet) * NUM_TOKENS);

  // --- Prefix Parselets
  g_prefixMap[TOKEN_IDENTIFIER] =
    [](roo_parser& parser) -> node*
    {
      printf("--> [PARSELET] Identifier\n");
      char* name = GetTextFromToken(PeekToken(parser));

      NextToken(parser);
      printf("<-- [PARSELET] Identifier\n");
      return CreateNode(VARIABLE_NODE, name);
    };

  g_prefixMap[TOKEN_NUMBER_INT] =
    [](roo_parser& parser) -> node*
    {
      printf("--> [PARSELET] Number constant (integer)\n");
      int value = PeekToken(parser).payload.i;
      NextToken(parser);
      printf("<-- [PARSELET] Number constant (integer)\n");
      return CreateNode(NUMBER_CONSTANT_NODE, number_constant_part::constant_type::CONSTANT_TYPE_INT, value);
    };

  g_prefixMap[TOKEN_NUMBER_FLOAT] =
    [](roo_parser& parser) -> node*
    {
      printf("--> [PARSELET] Number constant (floating point)\n");
      float value = PeekToken(parser).payload.f;
      NextToken(parser);
      printf("<-- [PARSELET] Number constant (floating point)\n");
      return CreateNode(NUMBER_CONSTANT_NODE, number_constant_part::constant_type::CONSTANT_TYPE_FLOAT, value);
    };

  g_prefixMap[TOKEN_STRING] =
    [](roo_parser& parser) -> node*
    {
      printf("--> [PARSELET] String\n");
      char* tokenText = GetTextFromToken(PeekToken(parser));
      NextToken(parser);

      printf("<-- [PARSELET] String\n");
      return CreateNode(STRING_CONSTANT_NODE, CreateStringConstant(parser.result, tokenText));
    };

  g_prefixMap[TOKEN_PLUS]   =
  g_prefixMap[TOKEN_MINUS]  =
  g_prefixMap[TOKEN_BANG]   =
  g_prefixMap[TOKEN_TILDE]  =
    [](roo_parser& parser) -> node*
    {
      printf("--> [PARSELET] Prefix operator (%s)\n", GetTokenName(PeekToken(parser).type));
      token_type operation = PeekToken(parser).type;

      NextToken(parser);
      printf("<-- [PARSELET] Prefix operation\n");
      return CreateNode(PREFIX_OP_NODE, operation, Expression(parser, P_PREFIX));
    };

  // --- Infix Parselets ---
  g_infixMap[TOKEN_PLUS] =
  g_infixMap[TOKEN_MINUS] =
  g_infixMap[TOKEN_ASTERIX] =
  g_infixMap[TOKEN_SLASH] =
    [](roo_parser& parser, node* left) -> node*
    {
      printf("--> [PARSELET] Binary operator (%s)\n", GetTokenName(PeekToken(parser).type));
      token_type operation = PeekToken(parser).type;

      NextToken(parser);
      printf("<-- [PARSELET] Binary operator\n");
      return CreateNode(BINARY_OP_NODE, operation, left, Expression(parser, g_precedenceTable[operation]));
    };

  // Parses a function call
  g_infixMap[TOKEN_LEFT_PAREN] =
    [](roo_parser& parser, node* left) -> node*
    {
      printf("--> [PARSELET] Function Call\n");

      if (left->type != VARIABLE_NODE)
      {
        SyntaxError(parser, "Unrecognised function name!");
      }

      char* functionName = static_cast<char*>(malloc(sizeof(char) * (strlen(left->payload.variable.var.name) + 1u)));
      strcpy(functionName, left->payload.variable.var.name);
      Free<node*>(left);

      node* result = CreateNode(FUNCTION_CALL_NODE, functionName);
      Consume(parser, TOKEN_LEFT_PAREN);

      while (!Match(parser, TOKEN_RIGHT_PAREN))
      {
        AddToLinkedList<node*>(result->payload.functionCall.params, Expression(parser));
      }

      Consume(parser, TOKEN_RIGHT_PAREN);
      printf("<-- [PARSELET] Function call\n");
      return result;
    };

  // Parses a member access
  g_infixMap[TOKEN_DOT] =
    [](roo_parser& parser, node* left) -> node*
    {
      printf("--> [PARSELET] Member access\n");

      if (left->type != VARIABLE_NODE &&
          left->type != MEMBER_ACCESS_NODE)
      {
        SyntaxError(parser, "Can only access members of existing variables or their members!");
      }

      char* memberName = GetTextFromToken(PeekToken(parser));
      NextToken(parser);

      printf("<-- [PARSELET] Member access\n");
      return CreateNode(MEMBER_ACCESS_NODE, left, memberName);
    };

  // Parses a variable assignment
  g_infixMap[TOKEN_EQUALS] =
    [](roo_parser& parser, node* left) -> node*
    {
      printf("--> [PARSELET] Variable assignment\n");

      if (left->type != VARIABLE_NODE)
      {
        SyntaxError(parser, "Expected variable name before '=' token!");
      }

      char* variableName = static_cast<char*>(malloc(sizeof(char) * strlen(left->payload.variable.var.name)));
      strcpy(variableName, left->payload.variable.var.name);
      Free<node*>(left);

      NextToken(parser);
      // NOTE(Isaac): minus one from the precedence because assignment is right-associative
      node* expression = Expression(parser, P_ASSIGNMENT - 1u);

      printf("<-- [PARSELET] Variable assignment\n");
      return CreateNode(VARIABLE_ASSIGN_NODE, variableName, expression);
    };
}
