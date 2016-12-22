/*
 * Copyright (C) 2016, Isaac Woods. All rights reserved.
 */

#pragma once

#include <cstdint>
#include <linked_list.hpp>
#include <common.hpp>

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
struct operator_def;
struct type_def;
struct string_constant;
struct air_instruction;
struct node;
struct elf_symbol;

enum slot_type
{
  VARIABLE,         // `var` field of payload is valid
  TEMPORARY,        // `tag` field of payload is valid
  INT_CONSTANT,     // `i` field of payload is valid
  FLOAT_CONSTANT,   // `f` field of payload is valid
  STRING_CONSTANT,  // `string` field of payload is valid
};

struct live_range
{
  // TODO
};

#define MAX_SLOT_INTERFERENCES 32u
struct variable_def;
struct slot_def
{
  union
  {
    variable_def*     variable;
    unsigned int      tag;
    int               i;
    float             f;
    string_constant*  string;
  }               payload;
  slot_type       type;
  signed int      color;  // NOTE(Isaac): -1 means it hasn't been colored
  unsigned int    numInterferences;
  slot_def*       interferences[MAX_SLOT_INTERFERENCES];

  // TODO: collection of live ranges

#ifdef OUTPUT_DOT
  unsigned int dotTag;
#endif
};

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
  linked_list<operator_def*>    operators;
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

  bool  isMutable;
};

struct variable_def
{
  char*         name;
  type_ref      type;
  node*         initValue;
  slot_def*     slot;
};

variable_def* CreateVariableDef(char* name, char* typeName, bool isMutable, node* initValue);

struct function_attrib
{
  enum attrib_type
  {
    ENTRY,
    PROTOTYPE,
  } type;
};

struct block_def
{
  linked_list<variable_def*>  params;
  linked_list<variable_def*>  locals;
  bool                        shouldAutoReturn;
};

struct function_def
{
  char*                         name;
  bool                          isPrototype;
  block_def                     scope;
  type_ref*                     returnType; // NOTE(Isaac): `nullptr` when function returns nothing
  linked_list<function_attrib>  attribs;

  node*                         ast;
  linked_list<slot_def*>        slots;
  air_instruction*              airHead;
  air_instruction*              airTail;
  unsigned int                  numTemporaries;
  elf_symbol*                   symbol;
};

/*struct operator_attrib
{
  enum attrib_type
  {
    PROTOTYPE,
  } type;
};*/

struct operator_def
{
  token_type                    op;
  bool                          isPrototype;
  block_def                     scope;
  type_ref                      returnType; // NOTE(Isaac): operators have to return something
//  linked_list<operator_attrib>  attribs;

  node*                         ast;
  linked_list<slot_def*>        slots;
  air_instruction*              airHead;
  air_instruction*              airTail;
  unsigned int                  numTemporaries;
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

function_def* CreateFunctionDef(char* name);
operator_def* CreateOperatorDef(token_type op);
void CompleteIR(parse_result& parse);
