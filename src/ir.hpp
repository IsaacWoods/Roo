/*
 * Copyright (C) 2016, Isaac Woods. All rights reserved.
 */

#pragma once

#include <common.hpp>

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

struct function_attrib
{
  enum attrib_type
  {
    ENTRY
  } type;
};

struct function_def
{
  char*                         name;
  linked_list<variable_def*>    params;
  linked_list<variable_def*>    locals;
  type_ref*                     returnType; // NOTE(Isaac): `nullptr` when function returns nothing
  bool                          shouldAutoReturn;
  linked_list<function_attrib>  attribs;

  node*                         ast;
  air_function*                 air;
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
