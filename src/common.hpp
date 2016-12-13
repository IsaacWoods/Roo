/*
 * Copyright (C) 2016, Isaac Woods. All rights reserved.
 */

#pragma once

#include <cstdlib>
#include <cstdint>
#include <cstdio>
#include <linked_list.hpp>

#define USING_GDB

// --- Ameanable crashes ---
#ifdef USING_GDB
  void Crash();
#else
  [[noreturn]] void Crash();
#endif

// --- File stuff and things ---
struct file
{
  char* name;

  /*
   * NOTE(Isaac): if the file doesn't have an extension, this is null
   * NOTE(Isaac): this points into `name`, because it'll be at the end
   */
  char* extension;
};

struct directory
{
  char* path;
  linked_list<file*> files;
};

void OpenDirectory(directory& dir, const char* path);

// --- Common functions ---
char* itoa(int num, char* str, int base);
char* ReadFile(const char* path);

enum compile_result
{
  SUCCESS,
  SYNTAX_ERROR,
  LINKING_ERROR
};

enum token_type
{
  // Keywords
  TOKEN_TYPE,
  TOKEN_FN,
  TOKEN_TRUE,
  TOKEN_FALSE,
  TOKEN_IMPORT,
  TOKEN_BREAK,
  TOKEN_RETURN,
  TOKEN_IF,
  TOKEN_ELSE,
  TOKEN_MUT,

  // Punctuation n' shit
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
  TOKEN_LOGICAL_AND,
  TOKEN_LOGICAL_OR,
  TOKEN_BITWISE_AND,
  TOKEN_BITWISE_OR,
  TOKEN_BITWISE_XOR,

  // Other stuff
  TOKEN_IDENTIFIER,
  TOKEN_STRING,
  TOKEN_NUMBER_INT,
  TOKEN_NUMBER_FLOAT,
  TOKEN_CHAR_CONSTANT,
  TOKEN_LINE,
  TOKEN_INVALID,

  NUM_TOKENS
};

const char* GetTokenName(token_type type);
