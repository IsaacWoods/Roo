/*
 * Copyright (C) 2017, Isaac Woods.
 * See LICENCE.md
 */

#pragma once

template<typename T>
const char* GetKeywordName(T keyword);

enum TokenType
{
  TOKEN_KEYWORD,

  // Punctuation
  TOKEN_DOT,
  TOKEN_COMMA,
  TOKEN_COLON,
  TOKEN_LEFT_PAREN,
  TOKEN_RIGHT_PAREN,
  TOKEN_LEFT_BRACE,             // {
  TOKEN_RIGHT_BRACE,            // }
  TOKEN_LEFT_BLOCK,             // [
  TOKEN_RIGHT_BLOCK,            // ]
  TOKEN_ASTERIX,
  TOKEN_PLUS,
  TOKEN_MINUS,
  TOKEN_SLASH,
  TOKEN_EQUALS,
  TOKEN_BANG,                   // !
  TOKEN_TILDE,                  // ~
  TOKEN_PERCENT,
  TOKEN_QUESTION_MARK,
  TOKEN_POUND,                  // #

  TOKEN_YIELDS,                 // ->
  TOKEN_START_ATTRIBUTE,        // #[
  TOKEN_EQUALS_EQUALS,
  TOKEN_BANG_EQUALS,
  TOKEN_GREATER_THAN,
  TOKEN_GREATER_THAN_EQUAL_TO,
  TOKEN_LESS_THAN,
  TOKEN_LESS_THAN_EQUAL_TO,
  TOKEN_DOUBLE_PLUS,
  TOKEN_DOUBLE_MINUS,
  TOKEN_LEFT_SHIFT,
  TOKEN_RIGHT_SHIFT,
  TOKEN_AND,
  TOKEN_OR,
  TOKEN_XOR,
  TOKEN_DOUBLE_AND,
  TOKEN_DOUBLE_OR,

  // Other stuff
  TOKEN_IDENTIFIER,
  TOKEN_STRING,
  TOKEN_SIGNED_INT,
  TOKEN_UNSIGNED_INT,
  TOKEN_FLOAT,
  TOKEN_CHAR_CONSTANT,
  TOKEN_LINE,
  TOKEN_INVALID,

  NUM_TOKENS
};

const char* GetTokenName(TokenType type);
