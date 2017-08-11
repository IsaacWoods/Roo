/*
 * Copyright (C) 2017, Isaac Woods.
 * See LICENCE.md
 */

#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <cstddef>
#include <cstring>
#include <common.hpp>
#include <token.hpp>
#include <error.hpp>

/*
 * `T` is the type of the enum that encompasses all of the keywords the parser may expect.
 */
template<typename T>
struct Token
{
  Token(TokenType type, unsigned int offset, unsigned int line, unsigned int lineOffset, const char* textStart, size_t textLength)
    :type(type)
    ,offset(offset)
    ,line(line)
    ,lineOffset(lineOffset)
    ,textStart(textStart)
    ,textLength(textLength)
    ,asUnsignedInt(0u)
  {
  }

  std::string AsString()
  {
    if (type == TOKEN_KEYWORD)
    {
      return GetKeywordName<T>(asKeyword);
    }
    else
    {
      return GetTokenName(type);
    }
  }

  std::string GetText()
  {
    /*
     * This is the upper bound of the amount of memory we need to store the string representation (because stuff
     * can get shorter, but never longer).
     */
    char buffer[textLength + 1u];

    /*
     * We need to manually traverse the buffer and convert escape sequences into the correct characters.
     */
    unsigned int i = 0u;
    for (const char* c = textStart;
         c < (textStart + static_cast<uintptr_t>(textLength));
         c++)
    {
      if (*c == '\\')
      {
        switch (*(++c))
        {
          case '0':   buffer[i++] = '\0'; break;
          case 'n':   buffer[i++] = '\n'; break;
          case 't':   buffer[i++] = '\t'; break;
          case '\\':  buffer[i++] = '\\'; break;

          default:
          {
            RaiseError(ERROR_ILLEGAL_ESCAPE_SEQUENCE, *c);
          } break;
        }
      }
      else
      {
        buffer[i++] = *c;
      }
    }
    buffer[i] = '\0';

    return std::string(buffer);
  }

  TokenType       type;
  unsigned int    offset;
  unsigned int    line;
  unsigned int    lineOffset;

  /*
   * Tokens do not actually store the text they contain, just a pointer to the section within the source (which is
   * allocated and managed by the parser). Because of this, tokens' text becomes invalid after the parser has been
   * freed. It is not null terminated.
   */
  const char*     textStart;
  size_t          textLength;

  union
  {
    T             asKeyword;
    int           asSignedInt;
    unsigned int  asUnsignedInt;
    float         asFloat;
  };
};

/*
 * This describes a general parser, templated on the enum of keywords that can appear in the token stream.
 */
template<typename T>
struct Parser
{
  Parser(const std::string& path)
    :path(path)
    ,source(ReadFile(path))
    ,currentChar(source)
    ,currentLine(1u)
    ,currentLineOffset(0u)
    ,currentToken(LexNext())
    ,nextToken(LexNext())
    ,errorState(new ParsingErrorState<T>(*this))
  {
  }

  virtual ~Parser()
  {
    delete source;
    delete errorState;
  }

  Token<T> PeekToken(bool ignoreLines = true)
  {
    if (ignoreLines)
    {
      while (currentToken.type == TOKEN_LINE)
      {
        NextToken();
      }
    }

    return currentToken;
  }

  Token<T> PeekNextToken(bool ignoreLines = true)
  {
    if (!ignoreLines)
    {
      return nextToken;
    }

    /*
     * If the next token should be ignored, we need to skip forward to the next token that doesn't, without
     * actually advancing the token stream.
     */
    const char*  cachedChar       = currentChar;
    unsigned int cachedLine       = currentLine;
    unsigned int cachedLineOffset = currentLineOffset;
    Token<T> next = nextToken;

    while (currentToken.type == TOKEN_LINE)
    {
      next = LexNext();
    }

    currentChar       = cachedChar;
    currentLine       = cachedLine;
    currentLineOffset = cachedLineOffset;

    return next;
  }

  Token<T> NextToken(bool ignoreLines = true)
  {
    currentToken = nextToken;
    nextToken = LexNext();

    if (ignoreLines)
    {
      while (currentToken.type == TOKEN_LINE)
      {
        NextToken();
      }
    }

    return currentToken;
  }

  void Consume(TokenType expectedType, bool ignoreLines = true)
  {
    if (PeekToken(ignoreLines).type != expectedType)
    {
      RaiseError(errorState, ERROR_EXPECTED_BUT_GOT, GetTokenName(expectedType), GetTokenName(currentToken.type));
    }
  
    NextToken(ignoreLines);
  }
  
  void ConsumeNext(TokenType expectedType, bool ignoreLines = true)
  {
    TokenType next = NextToken(ignoreLines).type;
    
    if (next != expectedType)
    {
      RaiseError(errorState, ERROR_EXPECTED_BUT_GOT, GetTokenName(expectedType), GetTokenName(next));
    }
  
    NextToken(ignoreLines);
  }
  
  bool Match(TokenType expectedType, bool ignoreLines = true)
  {
    return (PeekToken(ignoreLines).type == expectedType);
  }
  
  bool MatchNext(TokenType expectedType, bool ignoreLines = true)
  {
    return (PeekNextToken(ignoreLines).type == expectedType);
  }

  /*
   * These actually match keywords, but should look transparent, as if they are normal token types.
   */
  void Consume(T expected, bool ignoreLines = true)
  {
    if (!(PeekToken(ignoreLines).type == TOKEN_KEYWORD && PeekToken(ignoreLines).asKeyword == expected))
    {
      RaiseError(errorState, ERROR_EXPECTED_BUT_GOT, GetKeywordName<T>(expected), GetKeywordName<T>(currentToken.asKeyword));
    }
  
    NextToken(ignoreLines);
  }
  
  void ConsumeNext(T expected, bool ignoreLines = true)
  {
    Token<T> next = NextToken(ignoreLines);
    
    if (!(next.type == TOKEN_KEYWORD && next.asKeyword == expected))
    {
      RaiseError(errorState, ERROR_EXPECTED_BUT_GOT, GetKeywordName(expected), GetKeywordName(next.asKeyword));
    }
  
    NextToken(ignoreLines);
  }
  
  bool Match(T expected, bool ignoreLines = true)
  {
    return (PeekToken(ignoreLines).type == TOKEN_KEYWORD && PeekToken(ignoreLines).asKeyword == expected);
  }
  
  bool MatchNext(T expected, bool ignoreLines = true)
  {
    return (PeekNextToken(ignoreLines).type == TOKEN_KEYWORD && PeekNextToken(ignoreLines).asKeyword == expected);
  }

  std::string                         path;
  char*                               source;
  const char*                         currentChar;    // This points into source
  unsigned int                        currentLine;
  unsigned int                        currentLineOffset;

  std::unordered_map<const char*, T>  keywordMap;
  
  Token<T>                            currentToken;
  Token<T>                            nextToken;

  ErrorState*                         errorState;

private:
  char NextChar()
  {
    // Don't dereference memory past the end of the string
    if (*currentChar == '\0')
    {
      return '\0';
    }

    if (*currentChar == '\n')
    {
      currentLine++;
      currentLineOffset = 0;
    }
    else
    {
      currentLineOffset++;
    }

    return *(currentChar++);
  }

  Token<T> MakeToken(TokenType type, unsigned int offset, const char* startChar, size_t length)
  {
    return Token<T>(type, offset, currentLine, currentLineOffset, startChar, length);
  }

  Token<T> MakeToken(T keyword, unsigned int offset, const char* startChar, size_t length)
  {
    Token<T> token = Token<T>(TOKEN_KEYWORD, offset, currentLine, currentLineOffset, startChar, length);
    token.asKeyword = keyword;
    return token;
  }

  Token<T> LexName()
  {
    // We minus one to get the current char as well
    const char* startChar = currentChar - 1u;
    
    while (IsName(*currentChar, false))
    {
      NextChar();
    }

    ptrdiff_t length = (ptrdiff_t)((uintptr_t)currentChar - (uintptr_t)startChar);
    unsigned int tokenOffset = (unsigned int)((uintptr_t)currentChar - (uintptr_t)source);

    for (const std::pair<const char*, T>& keywordMapping : keywordMap)
    {
      if (memcmp(startChar, keywordMapping.first, strlen(keywordMapping.first)) == 0)
      {
        return MakeToken(keywordMapping.second, tokenOffset, startChar, (size_t)length);
      }
    }

    // It's not a keyword, so create an identifier token
    return MakeToken(TOKEN_IDENTIFIER, tokenOffset, startChar, (size_t)length);
  }

  Token<T> LexNumber()
  {
    const char* startChar = currentChar - 1u;
    char buffer[1024u] = {};
    unsigned int i = 0u;
    TokenType type = TOKEN_SIGNED_INT;
    buffer[i++] = *startChar;

    while (IsDigit(*(currentChar)) || *(currentChar) == '_')
    {
      char c = NextChar();
      if (c != '_')
      {
        buffer[i++] = c;
      }
    }

    // Check for a decimal point
    if (*(currentChar) == '.' && (IsDigit(*(currentChar + 1u)) || *(currentChar + 1u) == '_'))
    {
      NextChar();
      buffer[i++] = '.';
      type = TOKEN_FLOAT;

      while (IsDigit(*(currentChar)) || *(currentChar) == '_')
      {
        char c = NextChar();
        if (c != '_')
        {
          buffer[i++] = c;
        }
      }
    }

    unsigned int tokenOffset = (unsigned int)((uintptr_t)currentChar - (uintptr_t)source);

    if (type == TOKEN_SIGNED_INT && *(currentChar) == 'u')
    {
      NextChar();
      type = TOKEN_UNSIGNED_INT;
    }

    Token<T> token = MakeToken(type, tokenOffset, startChar, i);
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
        RaiseError(errorState, ICE_UNHANDLED_TOKEN_TYPE, "LexNumber", GetTokenName(type));
      } break;
    }

    return token;
  }

  Token<T> LexHexNumber()
  {
    NextChar(); // NOTE(Isaac): skip over the 'x'
    const char* startChar = currentChar;
    char buffer[1024u] = {};
    unsigned int i = 0u;

    while (IsHexDigit(*(currentChar)) || *(currentChar) == '_')
    {
      char c = NextChar();
      if (c != '_')
      {
        buffer[i++] = c;
      }
    }

    unsigned int tokenOffset = (unsigned int)((uintptr_t)currentChar - (uintptr_t)source);
    Token<T> token = MakeToken(TOKEN_UNSIGNED_INT, tokenOffset, startChar, i);
    token.asUnsignedInt = strtol(buffer, nullptr, 16);

    return token;
  }

  Token<T> LexBinaryNumber()
  {
    NextChar(); // NOTE(Isaac): skip over the 'b'
    const char* startChar = currentChar;
    char buffer[1024u] = {};
    unsigned int i = 0u;

    while (*(currentChar) == '0' ||
           *(currentChar) == '1' ||
           *(currentChar) == '_')
    {
      char c = NextChar();
      if (c != '_')
      {
        buffer[i++] = c;
      }
    }

    unsigned int tokenOffset = (unsigned int)((uintptr_t)currentChar - (uintptr_t)source);
    Token<T> token = MakeToken(TOKEN_UNSIGNED_INT, tokenOffset, startChar, (size_t)i);
    token.asUnsignedInt = strtol(buffer, nullptr, 2);

    return token;
  }

  Token<T> LexString()
  {
    const char* startChar = currentChar;

    while ((*(currentChar) != '\0') && (*(currentChar) != '"'))
    {
      NextChar();
    }

    ptrdiff_t length = (ptrdiff_t)((uintptr_t)currentChar - (uintptr_t)startChar);
    unsigned int tokenOffset = (unsigned int)((uintptr_t)currentChar - (uintptr_t)source);

    // NOTE(Isaac): skip the ending '"' character
    NextChar();

    return MakeToken(TOKEN_STRING, tokenOffset, startChar, (size_t)length);
  }

  Token<T> LexCharConstant()
  {
    const char* c = currentChar;
    NextChar();

    if (*(currentChar) != '\'')
    {
      RaiseError(errorState, ERROR_EXPECTED, "a ' to end the char constant");
    }

    return MakeToken(TOKEN_CHAR_CONSTANT, (unsigned int)((uintptr_t)c - (uintptr_t)source), c, (size_t)1u);
  }

  // TODO: Use an EMIT macro to make emitting single character tokens less painfull
  Token<T> LexNext()
  {
    TokenType type = TOKEN_INVALID;

    while (*currentChar != '\0')
    {
      /*
       * When lexing, the current char is actually the char after `c`.
       * Calling `NextChar()` will actually get the char after the char after `c`.
       */
      char c = NextChar();

      #define LEX_CHAR_TOKEN(character, token)\
        case character:\
          type = token;\
          goto EmitSimpleToken;

      switch (c)
      {
        LEX_CHAR_TOKEN('.', TOKEN_DOT);
        LEX_CHAR_TOKEN(',', TOKEN_COMMA);
        LEX_CHAR_TOKEN(':', TOKEN_COLON);
        LEX_CHAR_TOKEN('(', TOKEN_LEFT_PAREN);
        LEX_CHAR_TOKEN(')', TOKEN_RIGHT_PAREN);
        LEX_CHAR_TOKEN('{', TOKEN_LEFT_BRACE);
        LEX_CHAR_TOKEN('}', TOKEN_RIGHT_BRACE);
        LEX_CHAR_TOKEN('[', TOKEN_LEFT_BLOCK);
        LEX_CHAR_TOKEN(']', TOKEN_RIGHT_BLOCK);
        LEX_CHAR_TOKEN('*', TOKEN_ASTERIX);
        LEX_CHAR_TOKEN('~', TOKEN_TILDE);
        LEX_CHAR_TOKEN('%', TOKEN_PERCENT);
        LEX_CHAR_TOKEN('?', TOKEN_QUESTION_MARK);
        LEX_CHAR_TOKEN('^', TOKEN_XOR);
  
        case '+':
        {
          if (*currentChar == '+')
          {
            type = TOKEN_DOUBLE_PLUS;
            NextChar();
          }
          else
          {
            type = TOKEN_PLUS;
          }
  
          goto EmitSimpleToken;
        }
   
        case '-':
        {
          if (*currentChar == '>')
          {
            type = TOKEN_YIELDS;
            NextChar();
          }
          else if (*currentChar == '-')
          {
            type = TOKEN_DOUBLE_MINUS;
            NextChar();
          }
          else
          {
            type = TOKEN_MINUS;
          }
  
          goto EmitSimpleToken;
        }
   
        case '=':
        {
          if (*currentChar == '=')
          {
            type = TOKEN_EQUALS_EQUALS;
            NextChar();
          }
          else
          {
            type = TOKEN_EQUALS;
          }
  
          goto EmitSimpleToken;
        }
  
        case '!':
        {
          if (*currentChar == '=')
          {
            type = TOKEN_BANG_EQUALS;
            NextChar();
          }
          else
          {
            type = TOKEN_BANG;
          }
  
          goto EmitSimpleToken;
        }
  
        case '>':
        {
          if (*currentChar == '=')
          {
            type = TOKEN_GREATER_THAN_EQUAL_TO;
            NextChar();
          }
          else if (*currentChar == '>')
          {
            type = TOKEN_RIGHT_SHIFT;
            NextChar();
          }
          else
          {
            type = TOKEN_GREATER_THAN;
          }
  
          goto EmitSimpleToken;
        }
  
        case '<':
        {
          if (*currentChar == '=')
          {
            type = TOKEN_LESS_THAN_EQUAL_TO;
            NextChar();
          }
          else if (*currentChar == '<')
          {
            type = TOKEN_LEFT_SHIFT;
            NextChar();
          }
          else
          {
            type = TOKEN_LESS_THAN;
          }
  
          goto EmitSimpleToken;
        }
  
        case '&':
        {
          if (*currentChar == '&')
          {
            type = TOKEN_DOUBLE_AND;
            NextChar();
          }
          else
          {
            type = TOKEN_AND;
          }
  
          goto EmitSimpleToken;
        }
  
        case '|':
        {
          if (*currentChar == '|')
          {
            type = TOKEN_DOUBLE_OR;
            NextChar();
          }
          else
          {
            type = TOKEN_OR;
          }
  
          goto EmitSimpleToken;
        }
  
        case '/':
        {
          if (*currentChar == '/')
          {
            // NOTE(Isaac): We're lexing a line comment, skip forward to the next new-line
            do
            {
              NextChar();
            } while (*currentChar != '\n');
  
            NextChar();
          }
          else if (*currentChar == '*')
          {
            // NOTE(Isaac): We're lexing a block comment, skip forward to the ending token
            NextChar();
  
            while (currentChar)
            {
              if (*currentChar == '*')
              {
                NextChar();
  
                if (*currentChar == '/')
                {
                  NextChar();
                  break;
                }
              }
              else
              {
                NextChar();
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
          if (*currentChar == '[')
          {
            type = TOKEN_START_ATTRIBUTE;
            NextChar();
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
          while (*currentChar == ' '  ||
                 *currentChar == '\r' ||
                 *currentChar == '\n')
          {
            NextChar();
          }
        } break;
  
        case '\t':
        {
          RaiseError(errorState, WARNING_FOUND_TAB);
  
          while (*currentChar == '\t')
          {
            NextChar();
          }
        } break;
  
        case '\n':
          type = TOKEN_LINE;
          goto EmitSimpleToken;
  
        case '"':
          return LexString();
  
        case '\'':
          return LexCharConstant();
  
        default:
        {
          if (IsName(c, true))
          {
            return LexName();
          }
      
          if (c == '0' && *currentChar == 'x')
          {
            return LexHexNumber();
          }
  
          if (c == '0' && *currentChar == 'b')
          {
            return LexBinaryNumber();
          }
      
          if (IsDigit(c))
          {
            return LexNumber();
          }
  
          RaiseError(errorState, ERROR_UNLEXABLE_CHARACTER, c);
        } break;
      }
      #undef LEX_CHAR_TOKEN
    }
  
  EmitSimpleToken:
    return MakeToken(type, (uintptr_t)currentChar - (uintptr_t)source, nullptr, 0u);
  }

  /*
   * The first character of an identifier must be a lower or upper-case letter, but after that, underscores
   * and numbers are also allowed.
   */
  bool IsName(char c, bool isFirstChar)
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
  
  bool IsDigit(char c)
  {
    return (c >= '0' && c <= '9');
  }
  
  bool IsHexDigit(char c)
  {
    return ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'));
  }
};
