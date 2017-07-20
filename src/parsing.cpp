/*
 * Copyright (C) 2017, Isaac Woods.
 * See LICENCE.md
 */

#include <parsing.hpp>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cstddef>
#include <cstdint>
#include <cstdarg>
#include <climits>
#include <common.hpp>
#include <ir.hpp>
#include <ast.hpp>
#include <error.hpp>

/*
 * When this flag is set, the parser emits detailed logging throughout the parse.
 * It should probably be left off, unless debugging the lexer or parser.
 */
#if 0
  // NOTE(Isaac): format must be specified as the first vararg
  #define Log(parser, ...) Log_(parser, __VA_ARGS__);
  static void Log_(Parser& /*parser*/, const char* fmt, ...)
  {
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
  }
#else
  #define Log(parser, ...)
#endif

static inline char* GetTextFromToken(Parser& parser, const Token& token)
{
  // NOTE(Isaac): this is the upper bound of the amount of memory we need to store the string representation
  char* text = static_cast<char*>(malloc(sizeof(char) * (token.textLength + 1u)));

  // We need to manually escape string constants into actually correct chars
  if (token.type == TOKEN_STRING)
  {
    unsigned int i = 0u;

    for (const char* c = token.textStart;
         c < (token.textStart + static_cast<uintptr_t>(token.textLength));
         c++)
    {
      if (*c == '\\')
      {
        switch (*(++c))
        {
          case '0':   text[i++] = '\0'; break;
          case 'n':   text[i++] = '\n'; break;
          case 't':   text[i++] = '\t'; break;
          case '\\':  text[i++] = '\\'; break;

          default:
          {
            RaiseError(parser.errorState, ERROR_ILLEGAL_ESCAPE_SEQUENCE, *c);
          } break;
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
    memcpy(text, token.textStart, token.textLength);
    text[token.textLength] = '\0';
  }

  return text;
}

static char NextChar(Parser& parser)
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

/*
 * The first character of an identifier must be a lower or upper-case letter, but after that, underscores
 * and numbers are also allowed.
 */
static bool IsName(char c, bool isFirstChar)
{
  if (isFirstChar)
  {
    return ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'));
  }
  else
  {
    return ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c == '_') || (c >= '0' && c <= '9'));
  }
}

static bool IsDigit(char c)
{
  return (c >= '0' && c <= '9');
}

static bool IsHexDigit(char c)
{
  return ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'));
}

static inline Token MakeToken(Parser& parser, TokenType type, unsigned int offset, const char* startChar,
    unsigned int length)
{
  return Token{type, offset, parser.currentLine, parser.currentLineOffset, startChar, length, 0u};
}

static Token LexName(Parser& parser)
{
  // NOTE(Isaac): Minus one to get the current char as well
  const char* startChar = parser.currentChar - 1u;

  while (IsName(*(parser.currentChar), false))
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

static Token LexNumber(Parser& parser)
{
  const char* startChar = parser.currentChar - 1u;
  char buffer[1024u] = {};
  unsigned int i = 0u;
  TokenType type = TOKEN_SIGNED_INT;
  buffer[i++] = *startChar;

  while (IsDigit(*(parser.currentChar)) || *(parser.currentChar) == '_')
  {
    char c = NextChar(parser);
    if (c != '_')
    {
      buffer[i++] = c;
    }
  }

  // Check for a decimal point
  if (*(parser.currentChar) == '.' && (IsDigit(*(parser.currentChar + 1u)) || *(parser.currentChar + 1u) == '_'))
  {
    NextChar(parser);
    buffer[i++] = '.';
    type = TOKEN_FLOAT;

    while (IsDigit(*(parser.currentChar)) || *(parser.currentChar) == '_')
    {
      char c = NextChar(parser);
      if (c != '_')
      {
        buffer[i++] = c;
      }
    }
  }

  unsigned int tokenOffset = (unsigned int)((uintptr_t)parser.currentChar - (uintptr_t)parser.source);

  if (type == TOKEN_SIGNED_INT && *(parser.currentChar) == 'u')
  {
    NextChar(parser);
    type = TOKEN_UNSIGNED_INT;
  }

  Token token = MakeToken(parser, type, tokenOffset, startChar, i);
  switch (type)
  {
    case TOKEN_SIGNED_INT:
    {
      token.asSignedInt = strtol(buffer, nullptr, 10);
    } break;

    case TOKEN_UNSIGNED_INT:
    {
      token.asUnsignedInt = strtol(buffer, nullptr, 10);
    } break;

    case TOKEN_FLOAT:
    {
      token.asFloat = strtof(buffer, nullptr);
    } break;

    default:
    {
      RaiseError(parser.errorState, ICE_UNHANDLED_TOKEN_TYPE, "LexNumber", GetTokenName(type));
    } break;
  }

  return token;
}

static Token LexHexNumber(Parser& parser)
{
  NextChar(parser); // NOTE(Isaac): skip over the 'x'
  const char* startChar = parser.currentChar;
  char buffer[1024u] = {};
  unsigned int i = 0u;

  while (IsHexDigit(*(parser.currentChar)) || *(parser.currentChar) == '_')
  {
    char c = NextChar(parser);
    if (c != '_')
    {
      buffer[i++] = c;
    }
  }

  unsigned int tokenOffset = (unsigned int)((uintptr_t)parser.currentChar - (uintptr_t)parser.source);
  Token token = MakeToken(parser, TOKEN_UNSIGNED_INT, tokenOffset, startChar, i);
  token.asUnsignedInt = strtol(buffer, nullptr, 16);
  return token;
}

static Token LexBinaryNumber(Parser& parser)
{
  NextChar(parser); // NOTE(Isaac): skip over the 'b'
  const char* startChar = parser.currentChar;
  char buffer[1024u] = {};
  unsigned int i = 0u;

  while (*(parser.currentChar) == '0' ||
         *(parser.currentChar) == '1' ||
         *(parser.currentChar) == '_')
  {
    char c = NextChar(parser);
    if (c != '_')
    {
      buffer[i++] = c;
    }
  }

  unsigned int tokenOffset = (unsigned int)((uintptr_t)parser.currentChar - (uintptr_t)parser.source);
  Token token = MakeToken(parser, TOKEN_UNSIGNED_INT, tokenOffset, startChar, i);
  token.asUnsignedInt = strtol(buffer, nullptr, 2);
  return token;
}

static Token LexString(Parser& parser)
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

static Token LexCharConstant(Parser& parser)
{
  const char* c = parser.currentChar;
  NextChar(parser);

  if (*(parser.currentChar) != '\'')
  {
    RaiseError(parser.errorState, ERROR_EXPECTED, "a ' to end the char constant");
  }

  return MakeToken(parser, TOKEN_CHAR_CONSTANT, (unsigned int)((uintptr_t)c - (uintptr_t)parser.source), c, 1u);
}

static Token LexNext(Parser& parser)
{
  TokenType type = TOKEN_INVALID;

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
            if (*(parser.currentChar) == '*')
            {
              NextChar(parser);

              if (*(parser.currentChar) == '/')
              {
                NextChar(parser);
                break;
              }
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
      {
        while (*(parser.currentChar) == ' '  ||
               *(parser.currentChar) == '\r' ||
               *(parser.currentChar) == '\n')
        {
          NextChar(parser);
        }
      } break;

      case '\t':
      {
        RaiseError(parser.errorState, WARNING_FOUND_TAB);

        while (*(parser.currentChar) == '\t')
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
        if (IsName(c, true))
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

        RaiseError(parser.errorState, ERROR_UNLEXABLE_CHARACTER, c);
      } break;
    }
  }

EmitSimpleToken:
  return MakeToken(parser, type, (uintptr_t)parser.currentChar - (uintptr_t)parser.source, nullptr, 0u);
}

Token PeekToken(Parser& parser, bool ignoreLines)
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

Token NextToken(Parser& parser, bool ignoreLines)
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

static Token PeekNextToken(Parser& parser, bool ignoreLines = true)
{
  if (!ignoreLines)
  {
    return parser.nextToken;
  }

  // NOTE(Isaac): We need to skip tokens denoting line breaks, without advancing the token stream
  const char*  cachedChar        = parser.currentChar;
  unsigned int cachedLine        = parser.currentLine;
  unsigned int cachedLineOffset  = parser.currentLineOffset;

  Token next = parser.nextToken;

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
static void PeekNPrint(Parser& parser, bool ignoreLines = true)
{
  if (PeekToken(parser, ignoreLines).type == TOKEN_IDENTIFIER)
  {
    printf("PEEK: (%s)\n", GetTextFromToken(parser, PeekToken(parser, ignoreLines)));
  }
  else if ( (PeekToken(parser, ignoreLines).type == TOKEN_SIGNED_INT)   ||
            (PeekToken(parser, ignoreLines).type == TOKEN_UNSIGNED_INT) ||
            (PeekToken(parser, ignoreLines).type == TOKEN_FLOAT))
  {
    printf("PEEK: [%s]\n", GetTextFromToken(parser, PeekToken(parser, ignoreLines)));
  }
  else
  {
    printf("PEEK: %s\n", GetTokenName(PeekToken(parser, ignoreLines).type));
  }
}

static void PeekNPrintNext(Parser& parser, bool ignoreLines = true)
{
  if (PeekNextToken(parser, ignoreLines).type == TOKEN_IDENTIFIER)
  {
    printf("PEEK_NEXT: (%s)\n", GetTextFromToken(parser, PeekNextToken(parser, ignoreLines)));
  }
  else if ( (PeekToken(parser, ignoreLines).type == TOKEN_SIGNED_INT)   ||
            (PeekToken(parser, ignoreLines).type == TOKEN_UNSIGNED_INT) ||
            (PeekToken(parser, ignoreLines).type == TOKEN_FLOAT))
  {
    printf("PEEK_NEXT: [%s]\n", GetTextFromToken(parser, PeekNextToken(parser, ignoreLines)));
  }
  else
  {
    printf("PEEK_NEXT: %s\n", GetTokenName(PeekNextToken(parser, ignoreLines).type));
  }
}
#pragma GCC diagnostic pop

static inline void Consume(Parser& parser, TokenType expectedType, bool ignoreLines = true)
{
  if (PeekToken(parser, ignoreLines).type != expectedType)
  {
    RaiseError(parser.errorState, ERROR_EXPECTED_BUT_GOT, GetTokenName(expectedType), GetTokenName(parser.currentToken.type));
  }

  NextToken(parser, ignoreLines);
}

static inline void ConsumeNext(Parser& parser, TokenType expectedType, bool ignoreLines = true)
{
  TokenType next = NextToken(parser, ignoreLines).type;
  
  if (next != expectedType)
  {
    RaiseError(parser.errorState, ERROR_EXPECTED_BUT_GOT, GetTokenName(expectedType), GetTokenName(next));
  }

  NextToken(parser, ignoreLines);
}

static inline bool Match(Parser& parser, TokenType expectedType, bool ignoreLines = true)
{
  return (PeekToken(parser, ignoreLines).type == expectedType);
}

static inline bool MatchNext(Parser& parser, TokenType expectedType, bool ignoreLines = true)
{
  return (PeekNextToken(parser, ignoreLines).type == expectedType);
}

// --- Parsing ---
typedef ASTNode* (*PrefixParselet)(Parser&);
typedef ASTNode* (*InfixParselet)(Parser&, ASTNode*);

PrefixParselet g_prefixMap      [NUM_TOKENS];
InfixParselet  g_infixMap       [NUM_TOKENS];
unsigned int   g_precedenceTable[NUM_TOKENS];

/*
 * Parses expressions.
 * If the previous operator is right-associative, the new precedence should be one less than that of the operator
 */
static ASTNode* ParseExpression(Parser& parser, unsigned int precedence = 0u)
{
  Log(parser, "--> ParseExpression(%u)\n", precedence);
  PrefixParselet prefixParselet = g_prefixMap[PeekToken(parser).type];

  if (!prefixParselet)
  {
    RaiseError(parser.errorState, ERROR_UNEXPECTED, "prefix-expression", GetTokenName(PeekToken(parser).type));
    RaiseError(parser.errorState, ERROR_COMPILE_ERRORS);
  }

  ASTNode* expression = prefixParselet(parser);

  while (precedence < g_precedenceTable[PeekToken(parser, false).type])
  {
    InfixParselet infixParselet = g_infixMap[PeekToken(parser, false).type];

    // NOTE(Isaac): there is no infix expression part - just return the prefix expression
    if (!infixParselet)
    {
      Log(parser, "<-- ParseExpression(NO INFIX)\n");
      return expression;
    }

    expression = infixParselet(parser, expression);
  }

  Log(parser, "<-- ParseExpression\n");
  return expression;
}

static TypeRef ParseTypeRef(Parser& parser)
{
  TypeRef ref;
  
  if (Match(parser, TOKEN_MUT))
  {
    ref.isMutable = true;
    NextToken(parser);
  }

  ref.name = GetTextFromToken(parser, PeekToken(parser));
  NextToken(parser);

  if (Match(parser, TOKEN_LEFT_BLOCK))
  {
    Consume(parser, TOKEN_LEFT_BLOCK);
    ref.isArray = true;
    ref.arraySizeExpression = ParseExpression(parser);
    Consume(parser, TOKEN_RIGHT_BLOCK);
  }

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

static void ParseParameterList(Parser& parser, std::vector<VariableDef*>& params)
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
    char* varName = GetTextFromToken(parser, PeekToken(parser));
    ConsumeNext(parser, TOKEN_COLON);

    TypeRef typeRef = ParseTypeRef(parser);
    VariableDef* param = new VariableDef(varName, typeRef, nullptr);

    Log(parser, "Param: %s of type %s\n", param->name.c_str(), param->type.name.c_str());
    params.push_back(param);

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

static VariableDef* ParseVariableDef(Parser& parser)
{
  char* name = GetTextFromToken(parser, PeekToken(parser));
  ConsumeNext(parser, TOKEN_COLON);

  TypeRef typeRef = ParseTypeRef(parser);
  ASTNode* initValue = nullptr;

  if (Match(parser, TOKEN_EQUALS))
  {
    Consume(parser, TOKEN_EQUALS);
    initValue = ParseExpression(parser);
  }
  else if (Match(parser, TOKEN_LEFT_PAREN))
  {
    // Parse a type construction of the form `x : X(a, b, c)`
    Log(parser, "--> Type construction\n");
    Consume(parser, TOKEN_LEFT_PAREN);
    std::vector<ASTNode*> items;

    if (Match(parser, TOKEN_RIGHT_PAREN))
    {
      Consume(parser, TOKEN_RIGHT_PAREN, false);
    }
    else
    {
      while (true)
      {
        items.push_back(ParseExpression(parser));

        if (Match(parser, TOKEN_COMMA))
        {
          Consume(parser, TOKEN_COMMA);
        }
        else
        {
          Consume(parser, TOKEN_RIGHT_PAREN, false);
          break;
        }
      }
    }

    initValue = new ConstructNode(typeRef.name, items);
    Log(parser, "<-- Type construction\n");
  }

  VariableDef* variable = new VariableDef(name, typeRef, initValue);

  Log(parser, "Defined variable: '%s' which is %s%s'%s', which is %s\n",
              variable->name.c_str(),
              (variable->type.isArray ? "an array of " : "a "),
              (variable->type.isReference ? (variable->type.isReferenceMutable ? "mutable reference to a " : "reference to a ") : ""),
              variable->type.name.c_str(),
              (variable->type.isMutable ? "mutable" : "immutable"));

  return variable;
}

static ASTNode* ParseStatement(Parser& parser);
static ASTNode* ParseBlock(Parser& parser)
{
  Log(parser, "--> Block\n");
  Consume(parser, TOKEN_LEFT_BRACE);
  ASTNode* code = nullptr;

  // Each block should introduce a new scope
  ScopeDef* scope = new ScopeDef(parser.currentThing, (parser.scopeStack.size() > 0u ? parser.scopeStack.top()
                                                                                     : nullptr));
  parser.scopeStack.push(scope);

  while (!Match(parser, TOKEN_RIGHT_BRACE))
  {
    ASTNode* statement = ParseStatement(parser);

    Assert(statement, "Failed to parse statement");
    statement->containingScope = scope;

    if (code)
    {
      AppendNodeOntoTail(code, statement);
    }
    else
    {
      code = statement;
    }
  }

  Consume(parser, TOKEN_RIGHT_BRACE);
  parser.scopeStack.pop();  // Remove the block's scope from the stack
  Log(parser, "<-- Block\n");
  return code;
}

static ASTNode* ParseIf(Parser& parser)
{
  Log(parser, "--> If\n");

  Consume(parser, TOKEN_IF);
  Consume(parser, TOKEN_LEFT_PAREN);
  ASTNode* condition = ParseExpression(parser);
  Consume(parser, TOKEN_RIGHT_PAREN);

  ASTNode* thenCode = ParseBlock(parser);
  ASTNode* elseCode = nullptr;

  if (Match(parser, TOKEN_ELSE))
  {
    NextToken(parser);
    elseCode = ParseBlock(parser);
  }

  Log(parser, "<-- If\n");
  return new BranchNode(condition, thenCode, elseCode);
}

static ASTNode* ParseWhile(Parser& parser)
{
  Log(parser, "--> While\n");
  parser.isInLoop = true;

  Consume(parser, TOKEN_WHILE);
  Consume(parser, TOKEN_LEFT_PAREN);
  ASTNode* condition = ParseExpression(parser);
  Consume(parser, TOKEN_RIGHT_PAREN);
  ASTNode* code = ParseBlock(parser);

  parser.isInLoop = false;
  Log(parser, "<-- While\n");
  return new WhileNode(condition, code);
}

static ASTNode* ParseStatement(Parser& parser)
{
  Log(parser, "--> Statement(%s)", (parser.isInLoop ? "IN LOOP" : "NOT IN LOOP"));
  ASTNode* result = nullptr;
  bool needTerminatingLine = true;

  switch (PeekToken(parser).type)
  {
    case TOKEN_BREAK:
    {
      if (!(parser.isInLoop))
      {
        RaiseError(parser.errorState, ERROR_UNEXPECTED, "not-in-a-loop", GetTokenName(TOKEN_BREAK));
        return nullptr;
      }

      result = new BreakNode();
      Log(parser, "(BREAK)\n");
      NextToken(parser);
    } break;

    case TOKEN_RETURN:
    {
      Log(parser, "(RETURN)\n");
      parser.currentThing->shouldAutoReturn = false;
      NextToken(parser, false);

      if (Match(parser, TOKEN_LINE, false))
      {
        result = new ReturnNode(nullptr);
      }
      else
      {
        result = new ReturnNode(ParseExpression(parser));
      }
    } break;

    case TOKEN_IF:
    {
      Log(parser, "(IF)\n");
      result = ParseIf(parser);
      needTerminatingLine = false;
    } break;

    case TOKEN_WHILE:
    {
      Log(parser, "(WHILE)\n");
      result = ParseWhile(parser);
      needTerminatingLine = false;
    } break;

    case TOKEN_IDENTIFIER:
    {
      // It's a variable definition (probably)
      if (MatchNext(parser, TOKEN_COLON))
      {
        Log(parser, "(VARIABLE DEFINITION)\n");
        VariableDef* variable = ParseVariableDef(parser);

        // Assign the initial value to the variable
        if (variable->initialValue)
        {
          VariableNode* variableNode = new VariableNode(variable);
          result = new VariableAssignmentNode((ASTNode*)variableNode, variable->initialValue, true);
        }

        /*
         * Put the variable into the inner-most scope.
         */
        Assert(parser.scopeStack.size() >= 1u, "Parsed statement without surrounding scope");
        parser.scopeStack.top()->locals.push_back(variable);
        break;
      }
    } // NOTE(Isaac): no break

    default:
    {
      Log(parser, "(EXPRESSION STATEMENT)\n");
      result = ParseExpression(parser);
    }
  }

  if (needTerminatingLine)
  {
    Consume(parser, TOKEN_LINE, false);
  }

  Log(parser, "<-- Statement\n");
  return result;
}

static void ParseTypeDef(Parser& parser)
{
  Log(parser, "--> TypeDef(");
  Consume(parser, TOKEN_TYPE);
  TypeDef* type = new TypeDef(GetTextFromToken(parser, PeekToken(parser)));
  Log(parser, "%s)\n", type->name.c_str());
  
  ConsumeNext(parser, TOKEN_LEFT_BRACE);

  while (PeekToken(parser).type != TOKEN_RIGHT_BRACE)
  {
    VariableDef* member = ParseVariableDef(parser);
    type->members.push_back(member);
  }

  Consume(parser, TOKEN_RIGHT_BRACE);
  parser.result.types.push_back(type);
  Log(parser, "<-- TypeDef\n");
}

static void ParseImport(Parser& parser)
{
  Log(parser, "--> Import\n");
  Consume(parser, TOKEN_IMPORT);

  DependencyDef* dependency;
  switch (PeekToken(parser).type)
  {
    case TOKEN_IDENTIFIER:    // Local dependency
    {
      // TODO(Isaac): handle dotted identifiers
      Log(parser, "Importing: %s\n", GetTextFromToken(parser, PeekToken(parser)));
      dependency = new DependencyDef(DependencyDef::Type::LOCAL, GetTextFromToken(parser, PeekToken(parser)));
    } break;

    case TOKEN_STRING:        // Remote repository
    {
      Log(parser, "Importing remote: %s\n", GetTextFromToken(parser, PeekToken(parser)));
      dependency = new DependencyDef(DependencyDef::Type::REMOTE, GetTextFromToken(parser, PeekToken(parser)));
    } break;

    default:
    {
      RaiseError(parser.errorState, ERROR_EXPECTED_BUT_GOT, "string-literal or dotted-identifier", GetTokenName(PeekToken(parser).type));
    }
  }

  parser.result.dependencies.push_back(dependency);
  NextToken(parser);
  Log(parser, "<-- Import\n");
}

static void ParseFunction(Parser& parser, AttribSet& attribs)
{
  Log(parser, "--> Function(");
  Assert(parser.scopeStack.size() == 0, "Scope stack is not empty at function entry");

  FunctionThing* function = new FunctionThing(GetTextFromToken(parser, NextToken(parser)));
  function->attribs = attribs;
  Log(parser, "%s)\n", function->name.c_str());
  parser.result.codeThings.push_back(function);
  parser.currentThing = function;

  NextToken(parser);
  ParseParameterList(parser, function->params);

  // Optionally parse a return type
  if (Match(parser, TOKEN_YIELDS))
  {
    Consume(parser, TOKEN_YIELDS);
    function->returnType = new TypeRef(ParseTypeRef(parser));
    Log(parser, "Function returns a: %s\n", function->returnType->name.c_str());
  }
  else
  {
    function->returnType = nullptr;
  }

  if (function->attribs.isPrototype)
  {
    function->ast = nullptr;
  }
  else
  {
    function->ast = ParseBlock(parser);
  }

  Log(parser, "<-- Function\n");
}

static void ParseOperator(Parser& parser, AttribSet& attribs)
{
  Log(parser, "--> Operator(");

  OperatorThing* operatorDef = new OperatorThing(NextToken(parser).type);
  operatorDef->attribs = attribs;
  Log(parser, "%s)\n", GetTokenName(operatorDef->token));
  parser.result.codeThings.push_back(operatorDef);
  parser.currentThing = operatorDef;

  switch (operatorDef->token)
  {
    case TOKEN_PLUS:
    case TOKEN_MINUS:
    case TOKEN_ASTERIX:
    case TOKEN_SLASH:
    case TOKEN_DOUBLE_PLUS:
    case TOKEN_DOUBLE_MINUS:
    {
      NextToken(parser);
    } break;

    case TOKEN_LEFT_BLOCK:
    {
      ConsumeNext(parser, TOKEN_RIGHT_BLOCK);
    } break;

    default:
    {
      RaiseError(parser.errorState, ERROR_INVALID_OPERATOR, GetTokenName(operatorDef->token));
    } break;
  }

  ParseParameterList(parser, operatorDef->params);

  Consume(parser, TOKEN_YIELDS);
  operatorDef->returnType = new TypeRef();
  *(operatorDef->returnType) = ParseTypeRef(parser);
  Log(parser, "Return type: %s\n", operatorDef->returnType->name.c_str());

  if (operatorDef->attribs.isPrototype)
  {
    operatorDef->ast = nullptr;
  }
  else
  {
    operatorDef->ast = ParseBlock(parser);
    Assert(!(operatorDef->shouldAutoReturn), "Parsed an operator that should apparently auto-return");
  }

  Log(parser, "<-- Operator\n");
}

static void ParseAttribute(Parser& parser, AttribSet& attribs)
{
  char* attribName = GetTextFromToken(parser, NextToken(parser));

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
      RaiseError(parser.errorState, ERROR_EXPECTED, "identifier as program / library name");
      return;
    }

    parser.result.name = GetTextFromToken(parser, PeekToken(parser));
    ConsumeNext(parser, TOKEN_RIGHT_PAREN);
  }
  else if (strcmp(attribName, "TargetArch") == 0)
  {
    ConsumeNext(parser, TOKEN_LEFT_PAREN);

    if (!Match(parser, TOKEN_STRING))
    {
      RaiseError(parser.errorState, ERROR_EXPECTED, "string as target identifier");
      return;
    }

    parser.result.targetArch = GetTextFromToken(parser, PeekToken(parser));
    ConsumeNext(parser, TOKEN_RIGHT_PAREN);
  }
  else if (strcmp(attribName, "Module") == 0)
  {
    ConsumeNext(parser, TOKEN_LEFT_PAREN);

    if (!Match(parser, TOKEN_IDENTIFIER))
    {
      RaiseError(parser.errorState, ERROR_EXPECTED, "identifier as module name");
      return;
    }

    parser.result.isModule = true;
    parser.result.name = GetTextFromToken(parser, PeekToken(parser));
    ConsumeNext(parser, TOKEN_RIGHT_PAREN);
  }
  else if (strcmp(attribName, "LinkFile") == 0)
  {
    ConsumeNext(parser, TOKEN_LEFT_PAREN);

    if (!Match(parser, TOKEN_STRING))
    {
      RaiseError(parser.errorState, ERROR_EXPECTED, "string constant to specify path of file to manually link");
      return;
    }

    parser.result.filesToLink.push_back(GetTextFromToken(parser, PeekToken(parser)));
    ConsumeNext(parser, TOKEN_RIGHT_PAREN);
  }
  else if (strcmp(attribName, "DefinePrimitive") == 0)
  {
    ConsumeNext(parser, TOKEN_LEFT_PAREN);

    if (!Match(parser, TOKEN_STRING))
    {
      RaiseError(parser.errorState, ERROR_EXPECTED, "string containing name of primitive");
      return;
    }

    TypeDef* type = new TypeDef(GetTextFromToken(parser, PeekToken(parser)));

    ConsumeNext(parser, TOKEN_COMMA);
    if (!Match(parser, TOKEN_UNSIGNED_INT))
    {
      RaiseError(parser.errorState, ERROR_EXPECTED, "size of primitive as unsigned int (in bytes)");
      return;
    }
    type->size = PeekToken(parser).asUnsignedInt;

    parser.result.types.push_back(type);
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
    RaiseError(parser.errorState, ERROR_ILLEGAL_ATTRIBUTE, attribName);
  }

  Consume(parser, TOKEN_RIGHT_BLOCK);
  free(attribName);
}

Parser::Parser(ParseResult& result, const char* sourcePath)
  :path(sourcePath)
  ,source(ReadFile(sourcePath))
  ,currentChar(source)
  ,currentLine(1u)
  ,currentLineOffset(0u)
  ,isInLoop(false)
  ,currentToken(LexNext(*this))
  ,nextToken(LexNext(*this))
  ,scopeStack()
  ,result(result)
  ,errorState(ErrorState::Type::PARSING_UNIT, this)
{
  AttribSet attribs;

  while (!Match(*this, TOKEN_INVALID))
  {
    if (Match(*this, TOKEN_IMPORT))
    {
      ParseImport(*this);
    }
    else if (Match(*this, TOKEN_FN))
    {
      ParseFunction(*this, attribs);
      attribs = AttribSet();
    }
    else if (Match(*this, TOKEN_OPERATOR))
    {
      ParseOperator(*this, attribs);
      attribs = AttribSet();
    }
    else if (Match(*this, TOKEN_TYPE))
    {
      ParseTypeDef(*this);
    }
    else if (Match(*this, TOKEN_START_ATTRIBUTE))
    {
      ParseAttribute(*this, attribs);
    }
    else
    {
      RaiseError(errorState, ERROR_UNEXPECTED, "top-level", GetTokenName(PeekToken(*this).type));
    }
  }
}

Parser::~Parser()
{
  delete source;
}

__attribute__((constructor))
static void InitParseletMaps()
{
  // --- Precedence table ---
  memset(g_precedenceTable, 0, sizeof(unsigned int) * NUM_TOKENS);

  /*
   * The higher the precedence, the tighter binding an operator is (higher-precendence operations are done first)
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
  
  g_precedenceTable[TOKEN_EQUALS]                 = P_ASSIGNMENT;
  g_precedenceTable[TOKEN_QUESTION_MARK]          = P_TERNARY;
  g_precedenceTable[TOKEN_PLUS]                   = P_ADDITIVE;
  g_precedenceTable[TOKEN_MINUS]                  = P_ADDITIVE;
  g_precedenceTable[TOKEN_ASTERIX]                = P_MULTIPLICATIVE;
  g_precedenceTable[TOKEN_SLASH]                  = P_MULTIPLICATIVE;
  g_precedenceTable[TOKEN_LEFT_PAREN]             = P_PRIMARY;
  g_precedenceTable[TOKEN_LEFT_BLOCK]             = P_PRIMARY;
  g_precedenceTable[TOKEN_DOT]                    = P_MEMBER_ACCESS;
  g_precedenceTable[TOKEN_DOUBLE_PLUS]            = P_PREFIX;
  g_precedenceTable[TOKEN_DOUBLE_MINUS]           = P_PREFIX;
  g_precedenceTable[TOKEN_EQUALS_EQUALS]          = P_EQUALS_RELATIONAL;
  g_precedenceTable[TOKEN_BANG_EQUALS]            = P_EQUALS_RELATIONAL;
  g_precedenceTable[TOKEN_GREATER_THAN]           = P_COMPARATIVE_RELATIONAL;
  g_precedenceTable[TOKEN_GREATER_THAN_EQUAL_TO]  = P_COMPARATIVE_RELATIONAL;
  g_precedenceTable[TOKEN_LESS_THAN]              = P_COMPARATIVE_RELATIONAL;
  g_precedenceTable[TOKEN_LESS_THAN_EQUAL_TO]     = P_COMPARATIVE_RELATIONAL;

  // --- Parselets ---
  memset(g_prefixMap, 0, sizeof(PrefixParselet) * NUM_TOKENS);
  memset(g_infixMap,  0, sizeof(InfixParselet)  * NUM_TOKENS);

  // --- Prefix Parselets
  g_prefixMap[TOKEN_IDENTIFIER] =
    [](Parser& parser) -> ASTNode*
    {
      Log(parser, "--> [PARSELET] Identifier\n");
      char* name = GetTextFromToken(parser, PeekToken(parser));

      NextToken(parser, false);
      Log(parser, "<-- [PARSELET] Identifier\n");
      return new VariableNode(name);
    };

  g_prefixMap[TOKEN_SIGNED_INT] =
    [](Parser& parser) -> ASTNode*
    {
      Log(parser, "--> [PARSELET] Number constant (signed integer)\n");
      int value = PeekToken(parser).asSignedInt;
      NextToken(parser, false);
      Log(parser, "<-- [PARSELET] Number constant (signed integer)\n");
      return new ConstantNode<int>(value);
    };

  g_prefixMap[TOKEN_UNSIGNED_INT] =
    [](Parser& parser) -> ASTNode*
    {
      Log(parser, "--> [PARSELET] Number constant (unsigned integer)\n");
      unsigned int value = PeekToken(parser).asUnsignedInt;
      NextToken(parser, false);
      Log(parser, "<-- [PARSELET] Number constant (unsigned integer)\n");
      return new ConstantNode<unsigned int>(value);
    };

  g_prefixMap[TOKEN_FLOAT] =
    [](Parser& parser) -> ASTNode*
    {
      Log(parser, "--> [PARSELET] Number constant (floating point)\n");
      float value = PeekToken(parser).asFloat;
      NextToken(parser, false);
      Log(parser, "<-- [PARSELET] Number constant (floating point)\n");
      return new ConstantNode<float>(value);
    };

  g_prefixMap[TOKEN_TRUE]  =
  g_prefixMap[TOKEN_FALSE] =
    [](Parser& parser) -> ASTNode*
    {
      Log(parser, "--> [PARSELET] Bool\n");
      bool value = (PeekToken(parser).type == TOKEN_TRUE);
      NextToken(parser, false);

      Log(parser, "<-- [PARSELET] Bool\n");
      return new ConstantNode<bool>(value);
    };

  g_prefixMap[TOKEN_STRING] =
    [](Parser& parser) -> ASTNode*
    {
      Log(parser, "--> [PARSELET] String\n");
      char* tokenText = GetTextFromToken(parser, PeekToken(parser));
      NextToken(parser, false);

      Log(parser, "<-- [PARSELET] String\n");
      return new StringNode(new StringConstant(parser.result, tokenText));
    };

  g_prefixMap[TOKEN_PLUS]         =
  g_prefixMap[TOKEN_MINUS]        =
  g_prefixMap[TOKEN_BANG]         =
  g_prefixMap[TOKEN_TILDE]        =
  g_prefixMap[TOKEN_AND]          =
  g_prefixMap[TOKEN_DOUBLE_PLUS]  =   // ++i
  g_prefixMap[TOKEN_DOUBLE_MINUS] =   // --i
    [](Parser& parser) -> ASTNode*
    {
      Log(parser, "--> [PARSELET] Prefix operator (%s)\n", GetTokenName(PeekToken(parser).type));
      TokenType operation = PeekToken(parser).type;
      UnaryOpNode::Operator unaryOp;

      switch (operation)
      {
        case TOKEN_PLUS:          unaryOp = UnaryOpNode::Operator::POSITIVE;          break;
        case TOKEN_MINUS:         unaryOp = UnaryOpNode::Operator::NEGATIVE;          break;
        case TOKEN_BANG:          unaryOp = UnaryOpNode::Operator::LOGICAL_NOT;       break;
        case TOKEN_TILDE:         unaryOp = UnaryOpNode::Operator::NEGATE;            break;
        case TOKEN_AND:           unaryOp = UnaryOpNode::Operator::TAKE_REFERENCE;    break;
        case TOKEN_DOUBLE_PLUS:   unaryOp = UnaryOpNode::Operator::PRE_INCREMENT;     break;
        case TOKEN_DOUBLE_MINUS:  unaryOp = UnaryOpNode::Operator::PRE_DECREMENT;     break;

        default:
        {
          RaiseError(parser.errorState, ICE_UNHANDLED_TOKEN_TYPE, "PrefixParselet", GetTokenName(PeekToken(parser).type));
          __builtin_unreachable();
        } break;
      }

      NextToken(parser);
      ASTNode* operand = ParseExpression(parser, P_PREFIX);
      Log(parser, "<-- [PARSELET] Prefix operation\n");
      return new UnaryOpNode(unaryOp, operand);
    };

  g_prefixMap[TOKEN_LEFT_PAREN] =
    [](Parser& parser) -> ASTNode*
    {
      Log(parser, "--> [PARSELET] Parentheses\n");
      
      NextToken(parser);
      ASTNode* expression = ParseExpression(parser);
      Consume(parser, TOKEN_RIGHT_PAREN);

      Log(parser, "<-- [PARSELET] Parentheses\n");
      return expression;
    };

  // Parses an array literal
  g_prefixMap[TOKEN_LEFT_BRACE] =
    [](Parser& parser) -> ASTNode*
    {
      Log(parser, "--> [PARSELET] Array literal\n");
      
      NextToken(parser);
      std::vector<ASTNode*> items;

      // Check for an empty initialiser-list
      if (Match(parser, TOKEN_RIGHT_BRACE))
      {
        Consume(parser, TOKEN_RIGHT_BRACE);
      }
      else
      {
        while (true)
        {
          ASTNode* item = ParseExpression(parser, 0);
          items.push_back(item);

          if (Match(parser, TOKEN_COMMA))
          {
            Consume(parser, TOKEN_COMMA);
          }
          else
          {
            Consume(parser, TOKEN_RIGHT_BRACE);
            break;
          }
        }
      }

      Log(parser, "<-- [PARSELET] Array literal\n");
      return new ArrayInitNode(items);
    };

  // --- Infix Parselets ---
  // Parses binary operators
  g_infixMap[TOKEN_PLUS]          =
  g_infixMap[TOKEN_MINUS]         =
  g_infixMap[TOKEN_ASTERIX]       =
  g_infixMap[TOKEN_SLASH]         =
  g_infixMap[TOKEN_DOUBLE_PLUS]   =   // i++
  g_infixMap[TOKEN_DOUBLE_MINUS]  =   // i--
    [](Parser& parser, ASTNode* left) -> ASTNode*
    {
      Log(parser, "--> [PARSELET] Binary operator (%s)\n", GetTokenName(PeekToken(parser).type));
      TokenType operation = PeekToken(parser).type;

      // Special ones - these parse like infix operations but are actually unary ops
      if (operation == TOKEN_DOUBLE_PLUS ||
          operation == TOKEN_DOUBLE_MINUS)
      {
        NextToken(parser);
        Log(parser, "<-- [PARSELET] Binary operator\n");
        return new UnaryOpNode((operation == TOKEN_DOUBLE_PLUS ? UnaryOpNode::Operator::POST_INCREMENT :
                                                                 UnaryOpNode::Operator::POST_DECREMENT),
                               left);
      }

      BinaryOpNode::Operator binaryOp;

      switch (operation)
      {
        case TOKEN_PLUS:          binaryOp = BinaryOpNode::Operator::ADD;       break;
        case TOKEN_MINUS:         binaryOp = BinaryOpNode::Operator::SUBTRACT;  break;
        case TOKEN_ASTERIX:       binaryOp = BinaryOpNode::Operator::MULTIPLY;  break;
        case TOKEN_SLASH:         binaryOp = BinaryOpNode::Operator::DIVIDE;    break;
        case TOKEN_DOUBLE_PLUS:   binaryOp = BinaryOpNode::Operator::DIVIDE;    break;
        case TOKEN_DOUBLE_MINUS:  binaryOp = BinaryOpNode::Operator::DIVIDE;    break;

        default:
        {
          RaiseError(parser.errorState, ICE_UNHANDLED_TOKEN_TYPE, "BinaryOpParselet", GetTokenName(PeekToken(parser).type));
          __builtin_unreachable();
        } break;
      }

      NextToken(parser);
      ASTNode* right = ParseExpression(parser, g_precedenceTable[operation]);
      Log(parser, "<-- [PARSELET] Binary operator\n");
      return new BinaryOpNode(binaryOp, left, right);
    };

  // Parses an array index
  g_infixMap[TOKEN_LEFT_BLOCK] =
    [](Parser& parser, ASTNode* left) -> ASTNode*
    {
      Log(parser, "--> [PARSELET] Array index\n");
      Consume(parser, TOKEN_LEFT_BLOCK);
      ASTNode* indexParseExpression = ParseExpression(parser, 0u);
      Consume(parser, TOKEN_RIGHT_BLOCK);

      Log(parser, "<-- [PARSELET] Array index\n");
      return new BinaryOpNode(BinaryOpNode::Operator::INDEX_ARRAY, left, indexParseExpression);
    };

  // Parses a conditional
  g_infixMap[TOKEN_EQUALS_EQUALS]         =
  g_infixMap[TOKEN_BANG_EQUALS]           =
  g_infixMap[TOKEN_GREATER_THAN]          =
  g_infixMap[TOKEN_GREATER_THAN_EQUAL_TO] =
  g_infixMap[TOKEN_LESS_THAN]             =
  g_infixMap[TOKEN_LESS_THAN_EQUAL_TO]    =
    [](Parser& parser, ASTNode* left) -> ASTNode*
    {
      Log(parser, "--> [PARSELET] Conditional\n");

      TokenType conditionToken = PeekToken(parser).type;
      NextToken(parser);
      ASTNode* right = ParseExpression(parser, g_precedenceTable[conditionToken]);

      ConditionNode::Condition condition;
      switch (conditionToken)
      {
        case TOKEN_EQUALS_EQUALS:           condition = ConditionNode::Condition::EQUAL;                  break;
        case TOKEN_BANG_EQUALS:             condition = ConditionNode::Condition::NOT_EQUAL;              break;
        case TOKEN_GREATER_THAN:            condition = ConditionNode::Condition::GREATER_THAN;           break;
        case TOKEN_GREATER_THAN_EQUAL_TO:   condition = ConditionNode::Condition::GREATER_THAN_OR_EQUAL;  break;
        case TOKEN_LESS_THAN:               condition = ConditionNode::Condition::LESS_THAN;              break;
        case TOKEN_LESS_THAN_EQUAL_TO:      condition = ConditionNode::Condition::LESS_THAN_OR_EQUAL;     break;

        default:
        {
          RaiseError(parser.errorState, ICE_UNHANDLED_TOKEN_TYPE, "ConditionalParselet", GetTokenName(PeekToken(parser).type));
          __builtin_unreachable();
        } break;
      }

      Log(parser, "<-- [PARSELET] Conditional\n");
      return new ConditionNode(condition, left, right);
    };

  // Parses a function call
  g_infixMap[TOKEN_LEFT_PAREN] =
    [](Parser& parser, ASTNode* left) -> ASTNode*
    {
      Log(parser, "--> [PARSELET] Function Call\n");

      if (!IsNodeOfType<VariableNode>(left))
      {
        // FIXME: Print out the source that forms the expression
        RaiseError(parser.errorState, ERROR_EXPECTED_BUT_GOT, "function-name", "Potato");
      }

      VariableNode* leftAsVariable = reinterpret_cast<VariableNode*>(left);
      char* functionName = static_cast<char*>(malloc(sizeof(char) * (strlen(leftAsVariable->name) + 1u)));
      strcpy(functionName, leftAsVariable->name);
      delete left;

      std::vector<ASTNode*> params;
      Consume(parser, TOKEN_LEFT_PAREN);
      while (!Match(parser, TOKEN_RIGHT_PAREN))
      {
        params.push_back(ParseExpression(parser));
      }

      /*
       * XXX: We can't ignore new-lines here, but I think we should be able to? Maybe this is showing a bug
       * in the lexer where it doesn't roll back to TOKEN_LINEs correctly when we ask it to?
       */
      Consume(parser, TOKEN_RIGHT_PAREN, false);

      Log(parser, "<-- [PARSELET] Function call\n");
      return new CallNode(functionName, params);
    };

  // Parses a member access
  g_infixMap[TOKEN_DOT] =
    [](Parser& parser, ASTNode* left) -> ASTNode*
    {
      Log(parser, "--> [PARSELET] Member access\n");

      if (!(IsNodeOfType<VariableNode>(left) || IsNodeOfType<MemberAccessNode>(left)))
      {
        // FIXME: Print out the source that forms the expression
        RaiseError(parser.errorState, ERROR_EXPECTED_BUT_GOT, "variable-binding or member-binding", "Potato");
      }

      NextToken(parser);
      ASTNode* child = ParseExpression(parser, P_MEMBER_ACCESS); 

      Log(parser, "<-- [PARSELET] Member access\n");
      return new MemberAccessNode(left, child);
    };

  // Parses a ternary expression
  g_infixMap[TOKEN_QUESTION_MARK] =
    [](Parser& parser, ASTNode* left) -> ASTNode*
    {
      Log(parser, "--> [PARSELET] Ternary\n");
      
      if (!IsNodeOfType<ConditionNode>(left))
      {
        // FIXME: Print out the source that forms the expression
        RaiseError(parser.errorState, ERROR_UNEXPECTED_EXPRESSION, "conditional", "Potato");
      }

      ConditionNode* condition = reinterpret_cast<ConditionNode*>(left);

      NextToken(parser);
      ASTNode* thenBody = ParseExpression(parser, P_TERNARY-1u);
      Consume(parser, TOKEN_COLON);
      ASTNode* elseBody = ParseExpression(parser, P_TERNARY-1u);

      Log(parser, "<-- [PARSELET] Ternary\n");
      return new BranchNode(condition, thenBody, elseBody);
    };

  // Parses a variable assignment
  g_infixMap[TOKEN_EQUALS] =
    [](Parser& parser, ASTNode* left) -> ASTNode*
    {
      Log(parser, "--> [PARSELET] Variable assignment\n");

      NextToken(parser);
      ASTNode* expression = ParseExpression(parser, P_ASSIGNMENT-1u);

      Log(parser, "<-- [PARSELET] Variable assignment\n");
      return new VariableAssignmentNode(left, expression, false);
    };
}

const char* GetTokenName(TokenType type)
{
  switch (type)
  {
    case TOKEN_TYPE:                  return "TOKEN_TYPE";
    case TOKEN_FN:                    return "TOKEN_FN";
    case TOKEN_TRUE:                  return "TOKEN_TRUE";
    case TOKEN_FALSE:                 return "TOKEN_FALSE";
    case TOKEN_IMPORT:                return "TOKEN_IMPORT";
    case TOKEN_BREAK:                 return "TOKEN_BREAK";
    case TOKEN_RETURN:                return "TOKEN_RETURN";
    case TOKEN_IF:                    return "TOKEN_IF";
    case TOKEN_ELSE:                  return "TOKEN_ELSE";
    case TOKEN_WHILE:                 return "TOKEN_WHILE";
    case TOKEN_MUT:                   return "TOKEN_MUT";
    case TOKEN_OPERATOR:              return "TOKEN_OPERATOR";

    case TOKEN_DOT:                   return "TOKEN_DOT";
    case TOKEN_COMMA:                 return "TOKEN_COMMA";
    case TOKEN_COLON:                 return "TOKEN_COLON";
    case TOKEN_LEFT_PAREN:            return "TOKEN_LEFT_PAREN";
    case TOKEN_RIGHT_PAREN:           return "TOKEN_RIGHT_PAREN";
    case TOKEN_LEFT_BRACE:            return "TOKEN_LEFT_BRACE";
    case TOKEN_RIGHT_BRACE:           return "TOKEN_RIGHT_BRACE";
    case TOKEN_LEFT_BLOCK:            return "TOKEN_LEFT_BLOCK";
    case TOKEN_RIGHT_BLOCK:           return "TOKEN_RIGHT_BLOCK";
    case TOKEN_ASTERIX:               return "TOKEN_ASTERIX";
    case TOKEN_PLUS:                  return "TOKEN_PLUS";
    case TOKEN_MINUS:                 return "TOKEN_MINUS";
    case TOKEN_SLASH:                 return "TOKEN_SLASH";
    case TOKEN_EQUALS:                return "TOKEN_EQUALS";
    case TOKEN_BANG:                  return "TOKEN_BANG";
    case TOKEN_TILDE:                 return "TOKEN_TILDE";
    case TOKEN_PERCENT:               return "TOKEN_PERCENT";
    case TOKEN_QUESTION_MARK:         return "TOKEN_QUESTION_MARK";
    case TOKEN_POUND:                 return "TOKEN_POUND";

    case TOKEN_YIELDS:                return "TOKEN_YIELDS";
    case TOKEN_START_ATTRIBUTE:       return "TOKEN_START_ATTRIBUTE";
    case TOKEN_EQUALS_EQUALS:         return "TOKEN_EQUALS_EQUALS";
    case TOKEN_BANG_EQUALS:           return "TOKEN_BANG_EQUALS";
    case TOKEN_GREATER_THAN:          return "TOKEN_GREATER_THAN";
    case TOKEN_GREATER_THAN_EQUAL_TO: return "TOKEN_GREATER_THAN_EQUAL_TO";
    case TOKEN_LESS_THAN:             return "TOKEN_LESS_THAN";
    case TOKEN_LESS_THAN_EQUAL_TO:    return "TOKEN_LESS_THAN_EQUAL_TO";
    case TOKEN_DOUBLE_PLUS:           return "TOKEN_DOUBLE_PLUS";
    case TOKEN_DOUBLE_MINUS:          return "TOKEN_DOUBLE_MINUS";
    case TOKEN_LEFT_SHIFT:            return "TOKEN_LEFT_SHIFT";
    case TOKEN_RIGHT_SHIFT:           return "TOKEN_RIGHT_SHIFT";
    case TOKEN_AND:                   return "TOKEN_AND";
    case TOKEN_OR:                    return "TOKEN_OR";
    case TOKEN_XOR:                   return "TOKEN_XOR";
    case TOKEN_DOUBLE_AND:            return "TOKEN_DOUBLE_AND";
    case TOKEN_DOUBLE_OR:             return "TOKEN_DOUBLE_OR";

    case TOKEN_IDENTIFIER:            return "TOKEN_IDENTIFIER";
    case TOKEN_STRING:                return "TOKEN_STRING";
    case TOKEN_SIGNED_INT:            return "TOKEN_SIGNED_INT";
    case TOKEN_UNSIGNED_INT:          return "TOKEN_UNSIGNED_INT";
    case TOKEN_FLOAT:                 return "TOKEN_FLOAT";
    case TOKEN_CHAR_CONSTANT:         return "TOKEN_CHAR_CONSTANT";
    case TOKEN_LINE:                  return "TOKEN_LINE";
    case TOKEN_INVALID:               return "TOKEN_INVALID";
    case NUM_TOKENS:                  return "NUM_TOKENS";
  }

  return nullptr;
}
