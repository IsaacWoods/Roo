/*
 * Copyright (C) 2016, Isaac Woods. All rights reserved.
 */

#include <token_type.hpp>
#include <cstdio>
#include <cstdlib>

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
    case TOKEN_BREAK:
      return "TOKEN_BREAK";
    case TOKEN_RETURN:
      return "TOKEN_RETURN";
    case TOKEN_IF:
      return "TOKEN_IF";
    case TOKEN_ELSE:
      return "TOKEN_ELSE";

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
    case TOKEN_ASTERIX:
      return "TOKEN_ASTERIX";
    case TOKEN_PLUS:
      return "TOKEN_PLUS";
    case TOKEN_MINUS:
      return "TOKEN_MINUS";
    case TOKEN_SLASH:
      return "TOKEN_SLASH";
    case TOKEN_EQUALS:
      return "TOKEN_EQUALS";
    case TOKEN_BANG:
      return "TOKEN_BANG";
    case TOKEN_TILDE:
      return "TOKEN_TILDE";
    case TOKEN_PERCENT:
      return "TOKEN_PERCENT";
    case TOKEN_QUESTION_MARK:
      return "TOKEN_QUESTION_MARK";

    case TOKEN_YIELDS:
      return "TOKEN_YIELDS";
    case TOKEN_EQUALS_EQUALS:
      return "TOKEN_EQUALS_EQUALS";
    case TOKEN_BANG_EQUALS:
      return "TOKEN_BANG_EQUALS";
    case TOKEN_GREATER_THAN:
      return "TOKEN_GREATER_THAN";
    case TOKEN_GREATER_THAN_EQUAL_TO:
      return "TOKEN_GREATER_THAN_EQUAL_TO";
    case TOKEN_LESS_THAN:
      return "TOKEN_LESS_THAN";
    case TOKEN_LESS_THAN_EQUAL_TO:
      return "TOKEN_LESS_THAN_EQUAL_TO";
    case TOKEN_DOUBLE_PLUS:
      return "TOKEN_DOUBLE_PLUS";
    case TOKEN_DOUBLE_MINUS:
      return "TOKEN_DOUBLE_MINUS";
    case TOKEN_LEFT_SHIFT:
      return "TOKEN_LEFT_SHIFT";
    case TOKEN_RIGHT_SHIFT:
      return "TOKEN_RIGHT_SHIFT";
    case TOKEN_LOGICAL_AND:
      return "TOKEN_LOGICAL_AND";
    case TOKEN_LOGICAL_OR:
      return "TOKEN_LOGICAL_OR";
    case TOKEN_BITWISE_AND:
      return "TOKEN_BITWISE_AND";
    case TOKEN_BITWISE_OR:
      return "TOKEN_BITWISE_OR";
    case TOKEN_BITWISE_XOR:
      return "TOKEN_BITWISE_XOR";

    case TOKEN_IDENTIFIER:
      return "TOKEN_IDENTIFIER";
    case TOKEN_STRING:
      return "TOKEN_STRING";
    case TOKEN_NUMBER_INT:
      return "TOKEN_NUMBER_INT";
    case TOKEN_NUMBER_FLOAT:
      return "TOKEN_NUMBER_FLOAT";
    case TOKEN_CHAR_CONSTANT:
      return "TOKEN_CHAR_CONSTANT";
    case TOKEN_LINE:
      return "TOKEN_LINE";
    case TOKEN_INVALID:
      return "TOKEN_INVALID";

    default:
      fprintf(stderr, "Unhandled token type in GetTokenName!\n");
      exit(1);
  }
}
