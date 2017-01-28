/*
 * Copyright (C) 2016, Isaac Woods. All rights reserved.
 */

#pragma once

#include <cstdint>
#include <vector.hpp>
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

  unsigned int  functionReturnColor;
};

struct dependency_def;
struct function_def;
struct variable_def;
struct operator_def;
struct type_def;
struct string_constant;
struct air_instruction;
struct node;
struct elf_symbol;

enum slot_type
{
  VARIABLE,               // `var` field of payload is valid
  PARAMETER,              // `var` field of payload is valid
  TEMPORARY,              // `tag` field of payload is valid
  RETURN_RESULT,          // `tag` field of payload is valid
  SIGNED_INT_CONSTANT,    // `i` field of payload is valid
  UNSIGNED_INT_CONSTANT,  // `u` field of payload is valid
  FLOAT_CONSTANT,         // `f` field of payload is valid
  STRING_CONSTANT,        // `string` field of payload is valid
};

struct live_range
{
  /*
   * For slots of type `PARAMETER`, the defining instruction may be `nullptr`, as they have a value when coming
   * into the function.
   */
  air_instruction* definition;
  air_instruction* lastUse;
};

#define MAX_SLOT_INTERFERENCES 32u
struct slot_def
{
  union
  {
    variable_def*     variable;
    unsigned int      tag;
    int               i;
    unsigned int      u;
    float             f;
    string_constant*  string;
  };

  slot_type           type;
  signed int          color;  // NOTE(Isaac): -1 means it hasn't been colored
  unsigned int        numInterferences;
  slot_def*           interferences[MAX_SLOT_INTERFERENCES];
  vector<live_range>  liveRanges;

#ifdef OUTPUT_DOT
  unsigned int dotTag;
#endif
};

struct parse_result
{
  char*                     name;
  vector<dependency_def*>   dependencies;
  vector<function_def*>     functions;
  vector<operator_def*>     operators;
  vector<type_def*>         types;
  vector<string_constant*>  strings;
};

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

struct type_ref
{
  union
  {
    char*     name;
    type_def* def;
  };

  bool  isResolved;
  bool  isMutable;          // NOTE(Isaac): for references, this describes the mutability of the reference
  bool  isReference;
  bool  isReferenceMutable; // NOTE(Isaac): describes the mutability of the reference's contents
};

struct variable_def
{
  char*         name;
  type_ref      type;
  node*         initValue;
  slot_def*     slot;
};

struct attrib_set
{
  bool isEntry      : 1;
  bool isPrototype  : 1;
  bool isInline     : 1;
  bool isNoInline   : 1;
};

struct thing_of_code
{
  char*                 mangledName;
  vector<variable_def*> params;
  vector<variable_def*> locals;
  bool                  shouldAutoReturn;
  attrib_set            attribs;
  type_ref*             returnType;       // NOTE(Isaac): `nullptr` if it doesn't return anything

  node*                 ast;
  vector<slot_def*>     slots;
  air_instruction*      airHead;
  air_instruction*      airTail;
  unsigned int          numTemporaries;
  unsigned int          numReturnResults;
  elf_symbol*           symbol;
};

struct function_def
{
  char*             name;
  thing_of_code     code;
};

struct operator_def
{
  token_type        op;
  thing_of_code     code;
};

struct type_def
{
  char*                 name;
  vector<variable_def*> members;

  /*
   * Size of this structure in bytes.
   * NOTE(Isaac): provided for inbuilt types, calculated for composite types by `CalculateTypeSizes`.
   */
  unsigned int          size;
};

slot_def* CreateSlot(thing_of_code& code, slot_type type, ...);
char* SlotAsString(slot_def* slot);
void CreateParseResult(parse_result& result);
void InitAttribSet(attrib_set& set);
string_constant* CreateStringConstant(parse_result* result, char* string);
variable_def* CreateVariableDef(char* name, type_ref& typeRef, node* initValue);
function_def* CreateFunctionDef(char* name);
operator_def* CreateOperatorDef(token_type op);
type_def* GetTypeByName(parse_result& parse, const char* typeName);
char* MangleFunctionName(function_def* function);
char* MangleOperatorName(operator_def* op);
void CompleteIR(parse_result& parse);
