/*
 * Copyright (C) 2016, Isaac Woods. All rights reserved.
 */

#pragma once

#include <linked_list.hpp>
#include <cstdint>

struct node;
struct air_instruction;
struct elf_symbol;

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

  unsigned int  numIntParamColors;
  unsigned int* intParamColors;
};

struct dependency_def;
struct function_def;
struct type_def;
struct string_constant;

struct air_function;
struct slot;

struct program_attrib
{
  enum attrib_type
  {
    NAME,
  } type;

  union
  {
    char* name;
  } payload;
};

struct parse_result
{
  linked_list<dependency_def*>  dependencies;
  linked_list<function_def*>    functions;
  linked_list<type_def*>        types;
  linked_list<string_constant*> strings;
  linked_list<program_attrib>   attribs;
};

program_attrib* GetAttrib(parse_result& result, program_attrib::attrib_type type);

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

struct string_constant
{
  unsigned int      handle;
  char*             string;

  /*
   * NOTE(Isaac): this is set by the code generator when it's emitted into the executable. `UINT_MAX` beforehand.
   */
  uint64_t offset;
};

string_constant* CreateStringConstant(parse_result* result, char* string);

struct type_ref
{
  union
  {
    char*     name;
    type_def* def;
  }     type;
  bool  isResolved;
};

struct variable_def
{
  char*         name;
  type_ref      type;
  node*         initValue;
  slot*         mostRecentSlot;
};

variable_def* CreateVariableDef(char* name, char* typeName, node* initValue);

struct function_attrib
{
  enum attrib_type
  {
    ENTRY,
    PROTOTYPE,
  } type;
};

struct function_def
{
  char*                         name;
  bool                          isPrototype;
  linked_list<variable_def*>    params;
  linked_list<variable_def*>    locals;
  type_ref*                     returnType; // NOTE(Isaac): `nullptr` when function returns nothing
  bool                          shouldAutoReturn;
  linked_list<function_attrib>  attribs;

  node*                         ast;
  air_function*                 air;
  elf_symbol*                   symbol;
};

/*
 * NOTE(Isaac): returns `nullptr` if the function doesn't have the specified attribute
 */
function_attrib* GetAttrib(function_def* function, function_attrib::attrib_type type);
char* MangleFunctionName(function_def* function);

struct type_attrib
{
  enum attrib_type
  {

  } type;
};

struct type_def
{
  char*                       name;
  linked_list<variable_def*>  members;
  linked_list<type_attrib>    attribs;

  /*
   * Size of this structure in bytes.
   * NOTE(Isaac): provided for inbuilt types, calculated for composite types by `CalculateTypeSizes`.
   */
  unsigned int                size;
};

type_attrib* GetAttrib(type_def* typeDef, type_attrib::attrib_type type);

/*
 * This fills in all the stuff that couldn't be completed during parsing.
 * NOTE(Isaac): After this is called, the AST should be completely valid.
 */
void CompleteAST(parse_result& parse);
