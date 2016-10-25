/*
 * Copyright (C) 2016, Isaac Woods. All rights reserved.
 */

#pragma once

#include <common.hpp>

struct dependency_def;
struct function_def;
struct type_def;
struct string_constant;

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
};

void FreeVariableDef(variable_def* variable);

struct air_function;

struct function_def
{
  char*                       name;
  linked_list<variable_def*>  params;
  linked_list<variable_def*>  locals;
  type_ref*                   returnType; // NOTE(Isaac): `nullptr` when function returns nothing
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

  /*
   * Size of this structure in bytes.
   * NOTE(Isaac): provided for inbuilt types, calculated for composite types by `CalculateTypeSizes`.
   */
  unsigned int                size;
};

void FreeTypeDef(type_def* type);

/*
 * This fills in all the stuff that couldn't be completed during parsing.
 * NOTE(Isaac): After this is called, the AST should be completely valid.
 */
void CompleteAST(parse_result& parse);
