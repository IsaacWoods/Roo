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
#include <error.hpp>

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
  char*           source;
  const char*     currentChar; // NOTE(Isaac): this points into `source`
  unsigned int    currentLine;
  unsigned int    currentLineOffset;

  token           currentToken;
  token           nextToken;

  parse_result*   result;
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
            fprintf(stderr, "FATAL: Unrecognised escape sequence found '\\%c'!\n", *c);
            Crash();
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
  return token{type, offset, parser.currentLine, parser.currentLineOffset, startChar, length, 0u};
}

static void Log(roo_parser& /*parser*/, const char* fmt, ...)
{
  va_list args;
  va_start(args, fmt);

#if 1
  vprintf(fmt, args);
#endif

  va_end(args);
}

static token LexName(roo_parser& parser)
{
  // NOTE(Isaac): Minus one to get the current char as well
  const char* startChar = parser.currentChar - 1u;

  while (IsName(*(parser.currentChar)))
  {
    NextChar(parser);
  }

  ptrdiff_t length = (ptrdiff_t)((uintptr_t)parser.currentChar - (uintptr_t)startChar);
  unsigned int tokenOffset = (unsigned int)((uintptr_t)parser.currentChar - (uintptr_t)parser.source);

  #define KEYWORD(keyword, tokenType) \
    if (memcmp(startChar, keyword, strlen(keyword)) == 0) \
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
  KEYWORD("while",    TOKEN_WHILE)
  KEYWORD("mut",      TOKEN_MUT)
  KEYWORD("operator", TOKEN_OPERATOR)

  // It's not a keyword, so create an identifier token
  return MakeToken(parser, TOKEN_IDENTIFIER, tokenOffset, startChar, (unsigned int)length);
}

static token LexNumber(roo_parser& parser)
{
  const char* startChar = parser.currentChar - 1u;
  token_type type = TOKEN_SIGNED_INT;

  while (IsDigit(*(parser.currentChar)))
  {
    NextChar(parser);
  }

  // Check for a decimal point
  if (*(parser.currentChar) == '.' && IsDigit(*(parser.currentChar + 1u)))
  {
    NextChar(parser);
    type = TOKEN_FLOAT;

    while (IsDigit(*(parser.currentChar)))
    {
      NextChar(parser);
    }
  }

  ptrdiff_t length = (ptrdiff_t)((uintptr_t)parser.currentChar - (uintptr_t)startChar);
  unsigned int tokenOffset = (unsigned int)((uintptr_t)parser.currentChar - (uintptr_t)parser.source);

  if (type == TOKEN_SIGNED_INT && *(parser.currentChar) == 'u')
  {
    NextChar(parser);
    type = TOKEN_UNSIGNED_INT;
  }

  token tkn = MakeToken(parser, type, tokenOffset, startChar, (unsigned int)length);
  char* text = GetTextFromToken(tkn);

  switch (type)
  {
    case TOKEN_SIGNED_INT:
    {
      tkn.asSignedInt = strtol(text, nullptr, 10);
    } break;

    case TOKEN_UNSIGNED_INT:
    {
      tkn.asUnsignedInt = strtol(text, nullptr, 10);
    } break;

    case TOKEN_FLOAT:
    {
      tkn.asFloat = strtof(text, nullptr);
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

  token tkn = MakeToken(parser, TOKEN_UNSIGNED_INT, tokenOffset, startChar, (unsigned int)length);
  char* text = GetTextFromToken(tkn);
  tkn.asUnsignedInt = strtol(text, nullptr, 16);
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

  token tkn = MakeToken(parser, TOKEN_UNSIGNED_INT, tokenOffset, startChar, (unsigned int)length);
  char* text = GetTextFromToken(tkn);
  tkn.asUnsignedInt = strtol(text, nullptr, 2);
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
    RaiseError(ERROR_EXPECTED, "a ' to end the char constant");
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
        type = TOKEN_XOR;
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
          type = TOKEN_DOUBLE_AND;
          NextChar(parser);
        }
        else
        {
          type = TOKEN_AND;
        }

        goto EmitSimpleToken;
      }

      case '|':
      {
        if (*(parser.currentChar) == '|')
        {
          type = TOKEN_DOUBLE_OR;
          NextChar(parser);
        }
        else
        {
          type = TOKEN_OR;
        }

        goto EmitSimpleToken;
      }

      case '/':
      {
        if (*(parser.currentChar) == '/')
        {
          // NOTE(Isaac): We're lexing a line comment, skip forward to the next new-line
          do
          {
            NextChar(parser);
          } while (*(parser.currentChar) != '\n');

          NextChar(parser);
        }
        else if (*(parser.currentChar) == '*')
        {
          // NOTE(Isaac): We're lexing a block comment, skip forward to the ending token
          NextChar(parser);

          while (parser.currentChar)
          {
            if (*(parser.currentChar) == '/')
            {
              NextChar(parser);
              break;
            }
            else
            {
              NextChar(parser);
            }
          }
        }
        else
        {
          type = TOKEN_SLASH;
          goto EmitSimpleToken;
        }
      } break;

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

#if 1
static void PeekNPrint(roo_parser& parser, bool ignoreLines = true)
{
  if (PeekToken(parser, ignoreLines).type == TOKEN_IDENTIFIER)
  {
    printf("PEEK: (%s)\n", GetTextFromToken(PeekToken(parser, ignoreLines)));
  }
  else if ( (PeekToken(parser, ignoreLines).type == TOKEN_SIGNED_INT)   ||
            (PeekToken(parser, ignoreLines).type == TOKEN_UNSIGNED_INT) ||
            (PeekToken(parser, ignoreLines).type == TOKEN_FLOAT))
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
  else if ( (PeekToken(parser, ignoreLines).type == TOKEN_SIGNED_INT)   ||
            (PeekToken(parser, ignoreLines).type == TOKEN_UNSIGNED_INT) ||
            (PeekToken(parser, ignoreLines).type == TOKEN_FLOAT))
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
    RaiseError(ERROR_EXPECTED_BUT_GOT, GetTokenName(expectedType), GetTokenName(parser.currentToken.type));
  }

  NextToken(parser, ignoreLines);
}

static inline void ConsumeNext(roo_parser& parser, token_type expectedType, bool ignoreLines = true)
{
  token_type next = NextToken(parser, ignoreLines).type;
  
  if (next != expectedType)
  {
    RaiseError(ERROR_EXPECTED_BUT_GOT, GetTokenName(expectedType), GetTokenName(next));
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

static type_ref TypeRef(roo_parser& parser)
{
  type_ref ref;
  ref.isMutable           = false;
  ref.isResolved          = false;
  ref.isReference         = false;
  ref.isReferenceMutable  = false;
  
  if (Match(parser, TOKEN_MUT))
  {
    ref.isMutable = true;
    NextToken(parser);
  }

  ref.name = GetTextFromToken(PeekToken(parser));
  NextToken(parser);

  if (Match(parser, TOKEN_MUT))
  {
    Consume(parser, TOKEN_MUT);
    Consume(parser, TOKEN_AND);
    ref.isReference = true;
    ref.isReferenceMutable = true;
  }
  else if (Match(parser, TOKEN_AND))
  {
    Consume(parser, TOKEN_AND);
    ref.isReference = true;
  }

  return ref;
}

static void ParameterList(roo_parser& parser, vector<variable_def*>& params)
{
  Consume(parser, TOKEN_LEFT_PAREN);

  // Check for an empty parameter list
  if (Match(parser, TOKEN_RIGHT_PAREN))
  {
    Consume(parser, TOKEN_RIGHT_PAREN);
    return;
  }

  while (true)
  {
    char* varName = GetTextFromToken(PeekToken(parser));
    ConsumeNext(parser, TOKEN_COLON);

    type_ref typeRef = TypeRef(parser);
    variable_def* param = CreateVariableDef(varName, typeRef, nullptr);

    Log(parser, "Param: %s of type %s\n", param->name, param->type.name);
    Add<variable_def*>(params, param);

    if (Match(parser, TOKEN_COMMA))
    {
      Consume(parser, TOKEN_COMMA);
    }
    else
    {
      Consume(parser, TOKEN_RIGHT_PAREN);
      break;
    }
  }
}

static node* Expression(roo_parser& parser, unsigned int precedence = 0u)
{
  Log(parser, "--> Expression(%u)\n", precedence);
  prefix_parselet prefixParselet = g_prefixMap[PeekToken(parser).type];

  if (!prefixParselet)
  {
    RaiseError(ERROR_UNEXPECTED, "prefix-expression", GetTokenName(PeekToken(parser).type));
  }

  node* expression = prefixParselet(parser);

  while (precedence < g_precedenceTable[PeekToken(parser, false).type])
  {
    infix_parselet infixParselet = g_infixMap[PeekToken(parser, false).type];

    // NOTE(Isaac): there is no infix expression part - just return the prefix expression
    if (!infixParselet)
    {
      Log(parser, "<-- Expression(NO INFIX)\n");
      return expression;
    }

    expression = infixParselet(parser, expression);
  }

  Log(parser, "<-- Expression\n");
  return expression;
}

static variable_def* VariableDef(roo_parser& parser)
{
  char* name = GetTextFromToken(PeekToken(parser));
  ConsumeNext(parser, TOKEN_COLON);

  type_ref typeRef = TypeRef(parser);
  node* initValue = nullptr;

  if (Match(parser, TOKEN_EQUALS))
  {
    Consume(parser, TOKEN_EQUALS);
    initValue = Expression(parser);
  }

  variable_def* variable = CreateVariableDef(name, typeRef, initValue);

  Log(parser, "Defined variable: '%s' which is a %s'%s', which is %s\n",
              variable->name,
              (variable->type.isReference ? (variable->type.isReferenceMutable ? "mutable reference to a " : "reference to a ") : ""),
              variable->type.name,
              (variable->type.isMutable ? "mutable" : "immutable"));

  return variable;
}

static node* Statement(roo_parser& parser, thing_of_code& scope, bool isInLoop = false);
static node* Block(roo_parser& parser, thing_of_code& scope, bool isInLoop = false)
{
  Log(parser, "--> Block\n");
  Consume(parser, TOKEN_LEFT_BRACE);
  node* code = nullptr;

  while (!Match(parser, TOKEN_RIGHT_BRACE))
  {
    node* statement = Statement(parser, scope, isInLoop);

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
  Log(parser, "<-- Block\n");
  return code;
}

static node* Condition(roo_parser& parser, bool reverseOnJump)
{
  Log(parser, "--> Condition\n");
  node* left = Expression(parser);

  token_type condition = PeekToken(parser).type;

  if ((condition != TOKEN_EQUALS_EQUALS)          &&
      (condition != TOKEN_BANG_EQUALS)            &&
      (condition != TOKEN_GREATER_THAN)           &&
      (condition != TOKEN_GREATER_THAN_EQUAL_TO)  &&
      (condition != TOKEN_LESS_THAN)              &&
      (condition != TOKEN_LESS_THAN_EQUAL_TO))
  {
    RaiseError(ERROR_EXPECTED_BUT_GOT, "condition", GetTokenName(condition));
  }

  NextToken(parser);
  node* right = Expression(parser);

  Log(parser, "<-- Condition\n");
  return CreateNode(CONDITION_NODE, condition, left, right, reverseOnJump);
}

static node* If(roo_parser& parser, thing_of_code& scope)
{
  Log(parser, "--> If\n");

  Consume(parser, TOKEN_IF);
  Consume(parser, TOKEN_LEFT_PAREN);
  node* condition = Condition(parser, true);
  Consume(parser, TOKEN_RIGHT_PAREN);

  node* thenCode = Block(parser, scope);
  node* elseCode = nullptr;

  if (Match(parser, TOKEN_ELSE))
  {
    NextToken(parser);
    elseCode = Block(parser, scope);
  }

  Log(parser, "<-- If\n");
  return CreateNode(IF_NODE, condition, thenCode, elseCode);
}

static node* While(roo_parser& parser, thing_of_code& scope)
{
  Log(parser, "--> While\n");

  Consume(parser, TOKEN_WHILE);
  Consume(parser, TOKEN_LEFT_PAREN);
  node* condition = Condition(parser, false);
  Consume(parser, TOKEN_RIGHT_PAREN);

  node* code = Block(parser, scope, true);

  Log(parser, "<-- While\n");
  return CreateNode(WHILE_NODE, condition, code);
}

static node* Statement(roo_parser& parser, thing_of_code& scope, bool isInLoop)
{
  Log(parser, "--> Statement");
  node* result = nullptr;

  switch (PeekToken(parser).type)
  {
    case TOKEN_BREAK:
    {
      if (!isInLoop)
      {
        RaiseError(ERROR_UNEXPECTED, "not-in-a-loop", "break");
      }

      result = CreateNode(BREAK_NODE);
      Log(parser, "(BREAK)\n");
      NextToken(parser);
    } break;

    case TOKEN_RETURN:
    {
      Log(parser, "(RETURN)\n");
      scope.shouldAutoReturn = false;

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
      Log(parser, "(IF)\n");
      result = If(parser, scope);
    } break;

    case TOKEN_WHILE:
    {
      Log(parser, "(WHILE)\n");
      result = While(parser, scope);
    } break;

    case TOKEN_IDENTIFIER:
    {
      // It's a variable definition (probably)
      if (MatchNext(parser, TOKEN_COLON))
      {
        Log(parser, "(VARIABLE DEFINITION)\n");
        variable_def* variable = VariableDef(parser);

        // Assign the initial value to the variable
        node* variableNode = static_cast<node*>(malloc(sizeof(node)));
        variableNode->type = VARIABLE_NODE;
        variableNode->next = nullptr;
        variableNode->variable.var = variable;
        variableNode->variable.isResolved = true;

        result = CreateNode(VARIABLE_ASSIGN_NODE, variableNode, variable->initValue, true);

        Add<variable_def*>(scope.locals, variable);
        break;
      }
    } // NOTE(Isaac): no break

    default:
      Log(parser, "(EXPRESSION STATEMENT)\n");
      result = Expression(parser);

      // TODO(Isaac): make sure it's a valid node type to appear at top level
  }

  Log(parser, "<-- Statement\n");
  return result;
}

static void TypeDef(roo_parser& parser)
{
  Log(parser, "--> TypeDef(");
  Consume(parser, TOKEN_TYPE);
  type_def* type = static_cast<type_def*>(malloc(sizeof(type_def)));
  InitVector<variable_def*>(type->members);
  type->size = UINT_MAX;

  type->name = GetTextFromToken(PeekToken(parser));
  Log(parser, "%s)\n", type->name);
  
  ConsumeNext(parser, TOKEN_LEFT_BRACE);

  while (PeekToken(parser).type != TOKEN_RIGHT_BRACE)
  {
    variable_def* member = VariableDef(parser);
    Add<variable_def*>(type->members, member);
  }

  Consume(parser, TOKEN_RIGHT_BRACE);
  Add<type_def*>(parser.result->types, type);
  Log(parser, "<-- TypeDef\n");
}

static void Import(roo_parser& parser)
{
  Log(parser, "--> Import\n");
  Consume(parser, TOKEN_IMPORT);

  dependency_def* dependency = static_cast<dependency_def*>(malloc(sizeof(dependency_def)));

  switch (PeekToken(parser).type)
  {
    // NOTE(Isaac): Import a local library
    case TOKEN_IDENTIFIER:
    {
      // TODO(Isaac): handle dotted identifiers
      Log(parser, "Importing: %s\n", GetTextFromToken(PeekToken(parser)));
      dependency->type = dependency_def::dependency_type::LOCAL;
      dependency->path = GetTextFromToken(PeekToken(parser));
    } break;

    // NOTE(Isaac): Import a library from a remote repository
    case TOKEN_STRING:
    {
      Log(parser, "Importing remote: %s\n", GetTextFromToken(PeekToken(parser)));
      dependency->type = dependency_def::dependency_type::REMOTE;
      dependency->path = GetTextFromToken(PeekToken(parser));
    } break;

    default:
    {
      RaiseError(ERROR_EXPECTED_BUT_GOT, "string-literal or dotted-identifier", GetTokenName(PeekToken(parser).type));
    }
  }

  Add<dependency_def*>(parser.result->dependencies, dependency);
  NextToken(parser);
  Log(parser, "<-- Import\n");
}

static void Function(roo_parser& parser, attrib_set& attribs)
{
  Log(parser, "--> Function(");

  function_def* function = CreateFunctionDef(GetTextFromToken(NextToken(parser)));
  function->code.attribs = attribs;
  Log(parser, "%s)\n", function->name);
  Add<function_def*>(parser.result->functions, function);

  NextToken(parser);
  ParameterList(parser, function->code.params);

  // Optionally parse a return type
  if (Match(parser, TOKEN_YIELDS))
  {
    Consume(parser, TOKEN_YIELDS);
    function->code.returnType = static_cast<type_ref*>(malloc(sizeof(type_ref)));
    function->code.returnType->name = GetTextFromToken(PeekToken(parser));
    function->code.returnType->isResolved = false;
    NextToken(parser);

    Log(parser, "Function returns a: %s\n", function->code.returnType->name);
  }
  else
  {
    function->code.returnType = nullptr;
  }

  if (function->code.attribs.isPrototype)
  {
    function->code.ast = nullptr;
  }
  else
  {
    function->code.ast = Block(parser, function->code);
  }

  Log(parser, "<-- Function\n");
}

static void Operator(roo_parser& parser, attrib_set& attribs)
{
  Log(parser, "--> Operator(");
  operator_def* operatorDef = CreateOperatorDef(NextToken(parser).type);
  operatorDef->code.attribs = attribs;
  Log(parser, "%s)\n", GetTokenName(operatorDef->op));
  Add<operator_def*>(parser.result->operators, operatorDef);

  switch (operatorDef->op)
  {
    case TOKEN_PLUS:
    case TOKEN_MINUS:
    case TOKEN_ASTERIX:
    case TOKEN_SLASH:
    case TOKEN_DOUBLE_PLUS:
    case TOKEN_DOUBLE_MINUS:
    {
    } break;

    case TOKEN_LEFT_BLOCK:
    {
      ConsumeNext(parser, TOKEN_RIGHT_BLOCK);
    } break;

    default:
    {
      RaiseError(ERROR_INVALID_OPERATOR, GetTokenName(operatorDef->op));
    } break;
  }

  ParameterList(parser, operatorDef->code.params);

  Consume(parser, TOKEN_YIELDS);
  operatorDef->code.returnType = static_cast<type_ref*>(malloc(sizeof(type_ref)));
  operatorDef->code.returnType->name = GetTextFromToken(PeekToken(parser));
  operatorDef->code.returnType->isResolved = false;
  NextToken(parser);
  Log(parser, "Return type: %s\n", operatorDef->code.returnType->name);

  if (operatorDef->code.attribs.isPrototype)
  {
    operatorDef->code.ast = nullptr;
  }
  else
  {
    operatorDef->code.ast = Block(parser, operatorDef->code);
    assert(!(operatorDef->code.shouldAutoReturn));
  }

  Log(parser, "<-- Operator\n");
}

static void Attribute(roo_parser& parser, attrib_set& attribs)
{
  char* attribName = GetTextFromToken(NextToken(parser));

  if (strcmp(attribName, "Entry") == 0)
  {
    attribs.isEntry = true;
    NextToken(parser);
  }
  else if (strcmp(attribName, "Name") == 0)
  {
    ConsumeNext(parser, TOKEN_LEFT_PAREN);

    if (!Match(parser, TOKEN_IDENTIFIER))
    {
      RaiseError(ERROR_EXPECTED, "identifier as program / library name");
    }

    // NOTE(Isaac): while this is parsed like an attribute, it isn't created like one
    parser.result->name = GetTextFromToken(PeekToken(parser));
    ConsumeNext(parser, TOKEN_RIGHT_PAREN);
  }
  else if (strcmp(attribName, "Prototype") == 0)
  {
    attribs.isPrototype = true;
    NextToken(parser);
  }
  else if (strcmp(attribName, "Inline") == 0)
  {
    attribs.isInline = true;
    NextToken(parser);
  }
  else if (strcmp(attribName, "NoInline") == 0)
  {
    attribs.isNoInline = true;
    NextToken(parser);
  }
  else
  {
    RaiseError(ERROR_ILLEGAL_ATTRIBUTE, attribName);
  }

  Consume(parser, TOKEN_RIGHT_BLOCK);
  free(attribName);
}

void Parse(parse_result* result, const char* sourcePath)
{
  roo_parser parser;
  parser.source             = ReadFile(sourcePath);
  parser.currentChar        = parser.source;
  parser.currentLine        = 0u;
  parser.currentLineOffset  = 0u;
  parser.result             = result;
  parser.currentToken       = LexNext(parser);
  parser.nextToken          = LexNext(parser);

  attrib_set attribs;

  while (!Match(parser, TOKEN_INVALID))
  {
    if (Match(parser, TOKEN_IMPORT))
    {
      Import(parser);
    }
    else if (Match(parser, TOKEN_FN))
    {
      Function(parser, attribs);
      InitAttribSet(attribs);
    }
    else if (Match(parser, TOKEN_OPERATOR))
    {
      Operator(parser, attribs);
      InitAttribSet(attribs);
    }
    else if (Match(parser, TOKEN_TYPE))
    {
      TypeDef(parser);
    }
    else if (Match(parser, TOKEN_START_ATTRIBUTE))
    {
      Attribute(parser, attribs);
    }
    else
    {
      RaiseError(ERROR_UNEXPECTED, "block", GetTokenName(PeekToken(parser).type));
    }
  }

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
    P_PREFIX,                   // !, ~, +x, -x, ++x, --x, &, *
    P_PRIMARY,                  // x(...), x[...]
    P_SUFFIX,                   // x++, x--
    P_MEMBER_ACCESS,            // x.y
  };
  
  g_precedenceTable[TOKEN_EQUALS]       = P_ASSIGNMENT;
  g_precedenceTable[TOKEN_PLUS]         = P_ADDITIVE;
  g_precedenceTable[TOKEN_MINUS]        = P_ADDITIVE;
  g_precedenceTable[TOKEN_ASTERIX]      = P_MULTIPLICATIVE;
  g_precedenceTable[TOKEN_SLASH]        = P_MULTIPLICATIVE;
  g_precedenceTable[TOKEN_LEFT_PAREN]   = P_PRIMARY;
  g_precedenceTable[TOKEN_LEFT_BLOCK]   = P_PRIMARY;
  g_precedenceTable[TOKEN_DOT]          = P_MEMBER_ACCESS;
  g_precedenceTable[TOKEN_DOUBLE_PLUS]  = P_PREFIX;
  g_precedenceTable[TOKEN_DOUBLE_MINUS] = P_PREFIX;

  // --- Parselets ---
  memset(g_prefixMap, 0, sizeof(prefix_parselet) * NUM_TOKENS);
  memset(g_infixMap, 0, sizeof(infix_parselet) * NUM_TOKENS);

  // --- Prefix Parselets
  g_prefixMap[TOKEN_IDENTIFIER] =
    [](roo_parser& parser) -> node*
    {
      Log(parser, "--> [PARSELET] Identifier\n");
      char* name = GetTextFromToken(PeekToken(parser));

      NextToken(parser);
      Log(parser, "<-- [PARSELET] Identifier\n");
      return CreateNode(VARIABLE_NODE, name);
    };

  g_prefixMap[TOKEN_SIGNED_INT] =
    [](roo_parser& parser) -> node*
    {
      Log(parser, "--> [PARSELET] Number constant (signed integer)\n");
      int value = PeekToken(parser).asSignedInt;
      NextToken(parser);
      Log(parser, "<-- [PARSELET] Number constant (signed integer)\n");
      return CreateNode(NUMBER_CONSTANT_NODE, number_constant_part::constant_type::SIGNED_INT, value);
    };

  g_prefixMap[TOKEN_UNSIGNED_INT] =
    [](roo_parser& parser) -> node*
    {
      Log(parser, "--> [PARSELET] Number constant (unsigned integer)\n");
      unsigned int value = PeekToken(parser).asUnsignedInt;
      NextToken(parser);
      Log(parser, "<-- [PARSELET] Number constant (unsigned integer)\n");
      return CreateNode(NUMBER_CONSTANT_NODE, number_constant_part::constant_type::UNSIGNED_INT, value);
    };

  g_prefixMap[TOKEN_FLOAT] =
    [](roo_parser& parser) -> node*
    {
      Log(parser, "--> [PARSELET] Number constant (floating point)\n");
      float value = PeekToken(parser).asFloat;
      NextToken(parser);
      Log(parser, "<-- [PARSELET] Number constant (floating point)\n");
      return CreateNode(NUMBER_CONSTANT_NODE, number_constant_part::constant_type::FLOAT, value);
    };

  g_prefixMap[TOKEN_STRING] =
    [](roo_parser& parser) -> node*
    {
      Log(parser, "--> [PARSELET] String\n");
      char* tokenText = GetTextFromToken(PeekToken(parser));
      NextToken(parser);

      Log(parser, "<-- [PARSELET] String\n");
      return CreateNode(STRING_CONSTANT_NODE, CreateStringConstant(parser.result, tokenText));
    };

  g_prefixMap[TOKEN_PLUS]   =
  g_prefixMap[TOKEN_MINUS]  =
  g_prefixMap[TOKEN_BANG]   =
  g_prefixMap[TOKEN_TILDE]  =
    [](roo_parser& parser) -> node*
    {
      Log(parser, "--> [PARSELET] Prefix operator (%s)\n", GetTokenName(PeekToken(parser).type));
      token_type operation = PeekToken(parser).type;

      NextToken(parser);
      Log(parser, "<-- [PARSELET] Prefix operation\n");
      return CreateNode(PREFIX_OP_NODE, operation, Expression(parser, P_PREFIX));
    };

  // NOTE(Isaac): this parses taking the reference of another expression
  g_prefixMap[TOKEN_AND] =
    [](roo_parser& parser) -> node*
    {
      Log(parser, "--> [PARSELET] Reference operator\n");

      NextToken(parser);
      node* expression = Expression(parser, P_PREFIX);

      Log(parser, "<-- [PARSELET] Reference operator\n");
      return CreateNode(PREFIX_OP_NODE, TOKEN_AND, expression);
    };

  // --- Infix Parselets ---
  g_infixMap[TOKEN_PLUS] =
  g_infixMap[TOKEN_MINUS] =
  g_infixMap[TOKEN_ASTERIX] =
  g_infixMap[TOKEN_SLASH] =
    [](roo_parser& parser, node* left) -> node*
    {
      Log(parser, "--> [PARSELET] Binary operator (%s)\n", GetTokenName(PeekToken(parser).type));
      token_type operation = PeekToken(parser).type;

      NextToken(parser);
      Log(parser, "<-- [PARSELET] Binary operator\n");
      return CreateNode(BINARY_OP_NODE, operation, left, Expression(parser, g_precedenceTable[operation]));
    };

  // NOTE(Isaac): these are binary operations that don't have a right side (like increment and decrement)
  g_infixMap[TOKEN_DOUBLE_PLUS] =
  g_infixMap[TOKEN_DOUBLE_MINUS] =
    [](roo_parser& parser, node* left) -> node*
    {
      Log(parser, "--> [PARSELET] Binary operator (%s)\n", GetTokenName(PeekToken(parser).type));
      token_type operation = PeekToken(parser).type;

      NextToken(parser);
      Log(parser, "<-- [PARSELET] Binary operator\n");
      return CreateNode(BINARY_OP_NODE, operation, left, nullptr);
    };

  // NOTE(Isaac): parses an array index
  g_infixMap[TOKEN_LEFT_BLOCK] =
    [](roo_parser& parser, node* left) -> node*
    {
      Log(parser, "--> [PARSELET] Array index\n");
      Consume(parser, TOKEN_LEFT_BLOCK);
      node* indexExpression = Expression(parser, 0u);
      Consume(parser, TOKEN_RIGHT_BLOCK);

      Log(parser, "<-- [PARSELET] Array index\n");
      return CreateNode(BINARY_OP_NODE, TOKEN_LEFT_BLOCK, left, indexExpression);
    };

  // NOTE(Isaac): Parses a function call
  g_infixMap[TOKEN_LEFT_PAREN] =
    [](roo_parser& parser, node* left) -> node*
    {
      Log(parser, "--> [PARSELET] Function Call\n");

      if (left->type != VARIABLE_NODE)
      {
        RaiseError(ERROR_EXPECTED_BUT_GOT, "function-name", GetNodeName(left->type));
      }

      char* functionName = static_cast<char*>(malloc(sizeof(char) * (strlen(left->variable.name) + 1u)));
      strcpy(functionName, left->variable.name);
      Free<node*>(left);

      node* result = CreateNode(CALL_NODE, call_part::call_type::FUNCTION, functionName);
      Consume(parser, TOKEN_LEFT_PAREN);

      while (!Match(parser, TOKEN_RIGHT_PAREN))
      {
        Add<node*>(result->call.params, Expression(parser));
      }

      Consume(parser, TOKEN_RIGHT_PAREN);
      Log(parser, "<-- [PARSELET] Function call\n");
      return result;
    };

  // NOTE(Isaac): Parses a member access
  g_infixMap[TOKEN_DOT] =
    [](roo_parser& parser, node* left) -> node*
    {
      Log(parser, "--> [PARSELET] Member access\n");

      if (left->type != VARIABLE_NODE &&
          left->type != MEMBER_ACCESS_NODE)
      {
        RaiseError(ERROR_EXPECTED_BUT_GOT, "variable-binding or member-binding", GetNodeName(left->type));
      }

      char* memberName = GetTextFromToken(PeekToken(parser));
      NextToken(parser);

      Log(parser, "<-- [PARSELET] Member access\n");
      return CreateNode(MEMBER_ACCESS_NODE, left, memberName);
    };

  // NOTE(Isaac): Parses a variable assignment
  g_infixMap[TOKEN_EQUALS] =
    [](roo_parser& parser, node* left) -> node*
    {
      Log(parser, "--> [PARSELET] Variable assignment\n");

      if (left->type != VARIABLE_NODE &&
          left->type != MEMBER_ACCESS_NODE)
      {
        RaiseError(ERROR_EXPECTED, "variable-binding before '=' token");
      }

      NextToken(parser);
      // NOTE(Isaac): minus one from the precedence because assignment is right-associative
      node* expression = Expression(parser, P_ASSIGNMENT - 1u);

      Log(parser, "<-- [PARSELET] Variable assignment\n");
      return CreateNode(VARIABLE_ASSIGN_NODE, left, expression, false);
    };
}
