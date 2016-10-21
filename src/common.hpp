/*
 * Copyright (C) 2016, Isaac Woods. All rights reserved.
 */

#pragma once

#include <cstdlib>
#include <cstdint>
#include <cstdio>

// --- Linked list ---
template<typename T>
struct linked_list
{
  struct link
  {
    T payload;
    link* next;

    T& operator*()
    {
      return payload;
    }
  } *first;

  link* tail;
};

template<typename T>
void CreateLinkedList(linked_list<T>& list)
{
  list.first = nullptr;
  list.tail  = nullptr;
}

// O(n)
template<typename T>
void FreeLinkedList(linked_list<T>& list)
{
  list.tail = nullptr;

  while (list.first)
  {
    typename linked_list<T>::link* temp = list.first;
    list.first = list.first->next;

    // TODO: how should we free the payload? Callback sounds grim
    free(temp);
  }
}

// O(1)
template<typename T>
void AddToLinkedList(linked_list<T>& list, T thing)
{
  typename linked_list<T>::link* link = static_cast<typename linked_list<T>::link*>(malloc(sizeof(typename linked_list<T>::link)));
  link->next = nullptr;
  link->payload = thing;

  if (list.tail)
  {
    list.tail->next = link;
    list.tail = link;
  }
  else
  {
    list.first = list.tail = link;
  }
}

// O(n)
template<typename T>
unsigned int GetSizeOfLinkedList(linked_list<T>& list)
{
  unsigned int size = 0u;

  for (auto* i = list.first;
       i;
       i = i->next)
  {
    ++size;
  }

  return size;
}

/*
 * Turn the linked-list into a contiguous array of the same elements.
 */
// O(n)
template<typename T>
T* LinearizeLinkedList(linked_list<T>& list, unsigned int* size)
{
  T* result = static_cast<T*>(malloc(sizeof(T) * GetSizeOfLinkedList<T>(list)));

  unsigned int i = 0u;

  for (auto* it = list.first;
       it;
       it = it->next)
  {
    result[i++] = **it;
  }

  *size = i;
  return result;
}

// --- Common functions ---
char* itoa(int num, char* str, int base);
char* ReadFile(const char* path);

enum compile_result
{
  SUCCESS,
  SYNTAX_ERROR,
  LINKING_ERROR
};

// --- IR stuff ---
struct dependency_def;
struct function_def;
struct type_def;
struct string_constant;
struct node;
struct air_instruction;

/*
 * NOTE(Isaac): This allows the codegen module to store platform-dependent
 * information about each register.
 */
struct register_pimpl;

struct register_def
{
  enum reg_usage
  {
    GENERAL,
    SPECIAL
  }               usage;
  const char*     name;
  register_pimpl* pimpl;
};

struct codegen_target
{
  const char*   name;
  unsigned int  numRegisters;
  register_def* registerSet;
};

struct parse_result
{
  linked_list<dependency_def*>  dependencies;
  linked_list<function_def*>    functions;
  linked_list<type_def*>        types;
  linked_list<string_constant*> strings;
};

void CreateParseResult(parse_result& result);
void FreeParseResult(parse_result& result);

struct dependency_def
{
  enum dependency_type
  {
    LOCAL,
    REMOTE
  } type;

  /*
   * LOCAL:   a symbolic path from an arbitrary library source
   * REMOTE:  a URL to a Git repository containing a Roo project
   */
  char* path;
};

void FreeDependencyDef(dependency_def* dependency);

enum function_attribs : uint32_t
{
  ENTRY       = (1<<0),
};

void PrintFunctionAttribs(uint32_t attribMask);

struct string_constant
{
  unsigned int      handle;
  char*             string;
};

string_constant* CreateStringConstant(parse_result* result, char* string);
void FreeStringConstant(string_constant* string);

struct type_ref
{
  char* typeName;
};

type_ref* CreateTypeRef(char* typeName);
void FreeTypeRef(type_ref* typeRef);

struct variable_def
{
  char*         name;
  type_ref*     type;
  node*         initValue;
};

void FreeVariableDef(variable_def* variable);

struct air_function;

struct function_def
{
  char*                       name;
  linked_list<variable_def*>  params;
  linked_list<variable_def*>  locals;
  type_ref*                   returnType;
  bool                        shouldAutoReturn;
  uint32_t                    attribMask;

  node*                       ast;
  air_function*               air;
};

void FreeFunctionDef(function_def* function);

struct type_def
{
  char*                       name;
  linked_list<variable_def*>  members;
  uint32_t                    attribMask;
  unsigned int                size;
};

void FreeTypeDef(type_def* type);

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
