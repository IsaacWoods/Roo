/*
 * Copyright (C) 2017, Isaac Woods.
 * See LICENCE.md
 */

#pragma once

#include <cstdint>
#include <vector>
#include <common.hpp>
#include <parsing.hpp>
#include <error.hpp>

struct Slot;
struct AirInstruction;

struct DependencyDef;
struct ThingOfCode;
struct VariableDef;
struct TypeDef;
struct StringConstant;
struct elf_symbol;

struct ParseResult
{
  ParseResult();
  ~ParseResult();

  bool                          isModule;
  char*                         name;
  char*                         targetArch;     // `nullptr` leaves the compiler to assume the correct target arch
  std::vector<DependencyDef*>   dependencies;
  std::vector<ThingOfCode*>     codeThings;
  std::vector<TypeDef*>         types;
  std::vector<StringConstant*>  strings;
  std::vector<char*>            filesToLink;
};

struct DependencyDef
{
  enum Type
  {
    LOCAL,
    REMOTE
  };

  DependencyDef(Type type, char* path);
  ~DependencyDef();

  Type type;
  /*
   * LOCAL:   the name of a local package
   * REMOTE:  a URL to a Git repository containing a Roo project
   */
  char* path;
};

struct StringConstant
{
  StringConstant(ParseResult& parse, char* string);
  ~StringConstant();

  unsigned int      handle;
  char*             string;

  /*
   * NOTE(Isaac): this is set by the code generator when it's emitted into the executable. `UINT_MAX` beforehand.
   */
  uint64_t offset;
};

struct TypeDef
{
  TypeDef(char* name);
  ~TypeDef();

  char*                       name;
  std::vector<VariableDef*>   members;
  ErrorState                  errorState;

  /*
   * Size of this structure in bytes.
   * NOTE(Isaac): provided for inbuilt types, calculated for composite types by `CalculateTypeSizes`.
   */
  unsigned int                size;
};

struct TypeRef
{
  TypeRef();
  ~TypeRef();

  std::string name;
  TypeDef*    resolvedType;        // NOTE(Isaac): for empty array `initialiser-list`s, this may be nullptr
  bool        isResolved;
  bool        isMutable;           // NOTE(Isaac): for references, this describes the mutability of the reference
  bool        isReference;
  bool        isReferenceMutable;  // NOTE(Isaac): describes the mutability of the reference's contents

  bool        isArray;
  bool        isArraySizeResolved;
  union
  {
    ASTNode*      arraySizeExpression;
    unsigned int  arraySize;
  };
};

struct VariableDef
{
  VariableDef(char* name, const TypeRef& typeRef, ASTNode* initialValue);
  ~VariableDef();

  char*     name;
  TypeRef   type;
  ASTNode*  initialValue;
  Slot*     slot;

  /*
   * NOTE(Isaac): this can be used to represent multiple things:
   *    * Offset inside a parent structure
   */
  unsigned int  offset;
};

struct AttribSet
{
  AttribSet();

  bool isEntry      : 1;
  bool isPrototype  : 1;
  bool isInline     : 1;
  bool isNoInline   : 1;
};

struct ThingOfCode
{
  enum Type
  {
    FUNCTION,
    OPERATOR
  };

  ThingOfCode(Type type, ...);
  ~ThingOfCode();

  Type                        type;
  union
  {
    char*                     name;
    token_type                op;
  };

  char*                       mangledName;
  std::vector<VariableDef*>   params;
  std::vector<VariableDef*>   locals;
  bool                        shouldAutoReturn;
  AttribSet                   attribs;
  TypeRef*                    returnType;       // NOTE(Isaac): `nullptr` if it doesn't return anything

  ErrorState                  errorState;
  std::vector<ThingOfCode*>   calledThings;

  // AST representation
  ASTNode*                    ast;

  // AIR representation
  std::vector<Slot*>          slots;
  unsigned int                stackFrameSize;
  AirInstruction*             airHead;
  AirInstruction*             airTail;
  unsigned int                numTemporaries;
  unsigned int                numReturnResults;
  elf_symbol*                 symbol;
};

TypeDef* GetTypeByName(ParseResult& parse, const char* typeName);
char* TypeRefToString(TypeRef* type);
bool AreTypeRefsCompatible(TypeRef* a, TypeRef* b, bool careAboutMutability = true);
unsigned int GetSizeOfTypeRef(TypeRef& type);
char* MangleName(ThingOfCode* thing);
void CompleteIR(ParseResult& parse);
