/*
 * Copyright (C) 2017, Isaac Woods.
 * See LICENCE.md
 */

#include <token.hpp>

const char* GetTokenName(TokenType type)
{
  switch (type)
  {
    case TOKEN_KEYWORD:               return "TOKEN_KEYWORD";

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
}
