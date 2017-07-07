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
struct ElfSymbol;

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

/*
 * This describes the definition of a type. The error state should be used to report errors with the definition
 * (and not the usage) of this type.
 */
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

/*
 * This describes the *usage* of a type, unlike `TypeDef`. For example, a `TypeRef` should be used to represent
 * the type of a variable, and stores extra information such as whether it is a reference or mutable.
 */
struct TypeRef
{
  TypeRef();
  ~TypeRef();

  std::string AsString();
  unsigned int GetSize();

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

/*
 * This describes the definition of a variable, and can also be used to refer to variables that have already
 * been defined. Its initial value should be assigned to its allocated register/memory when it enters scope.
 */
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
  unsigned int offset;
};

/*
 * This bitfield describes the attributes of a type, function or entire program. Attributes are used to provide
 * extra information to the compiler about how the thing they have been applied to should be handled.
 */
struct AttribSet
{
  AttribSet();

  bool isEntry      : 1;        // (Function) - Defines a function as the entry point of the program
  bool isPrototype  : 1;        // (Function) - Defines a prototype function (one not implemented in Roo)
  bool isInline     : 1;        // (Function) - Will always inline the function
  bool isNoInline   : 1;        // (Function) - Will never inline the function
};

/*
 * This describes a function (either parsed as an actual function or as an operator) that takes inputs (parameters)
 * does stuff to them (described by the AST and then the AIR code), and produces an output (the return value).
 * Functions supply a name in the `name` field, whereas operators supply the type of the token used to denote the
 * operation (addition uses TOKEN_PLUS, multiplication TOKEN_ASTERIX etc.) in the `op` field.
 */
struct ThingOfCode
{
  enum Type
  {
    FUNCTION,
    OPERATOR
  };

  ThingOfCode(Type type, ...);
  ~ThingOfCode();

  Type                      type;
  union
  {
    char*                   name;
    TokenType               op;
  };

  char*                     mangledName;
  std::vector<VariableDef*> params;
  std::vector<VariableDef*> locals;
  bool                      shouldAutoReturn;
  AttribSet                 attribs;
  TypeRef*                  returnType;       // NOTE(Isaac): `nullptr` if it doesn't return anything

  ErrorState                errorState;
  std::vector<ThingOfCode*> calledThings;

  // AST representation
  ASTNode*                  ast;

  // AIR representation
  std::vector<Slot*>        slots;
  unsigned int              stackFrameSize;
  AirInstruction*           airHead;
  AirInstruction*           airTail;
  unsigned int              numTemporaries;
  unsigned int              numReturnResults;
  ElfSymbol*                symbol;
};

TypeDef* GetTypeByName(ParseResult& parse, const char* typeName);
bool AreTypeRefsCompatible(TypeRef* a, TypeRef* b, bool careAboutMutability = true);
void CompleteIR(ParseResult& parse);
