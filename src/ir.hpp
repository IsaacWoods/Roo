/*
 * Copyright (C) 2017, Isaac Woods.
 * See LICENCE.md
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <common.hpp>
#include <parser.hpp>
#include <error.hpp>

struct Slot;
struct AirInstruction;

struct DependencyDef;
struct CodeThing;
struct MemberDef;
struct VariableDef;
struct TypeDef;
struct StringConstant;
struct ElfSymbol;

struct TargetMachine;

/*
 * This records the type of intrinsic operations, which are handled directly by the compiler, instead of being done
 * as function calls.
 */
enum class IntrinsicOpType
{
  UNKNOWN,

  UNSIGNED_INT,
  SIGNED_INT,
  FLOAT,
};

struct ParseResult
{
  ParseResult();

  bool                          isModule;
  std::string                   name;
  std::string                   targetArch;     // `nullptr` leaves the compiler to assume the correct target arch
  std::vector<DependencyDef*>   dependencies;
  std::vector<CodeThing*>       codeThings;
  std::vector<TypeDef*>         types;
  std::vector<StringConstant*>  strings;
  std::vector<std::string>      filesToLink;
};

struct DependencyDef
{
  enum Type
  {
    LOCAL,
    REMOTE
  };

  DependencyDef(Type type, const std::string& path);

  Type type;
  /*
   * LOCAL:   the name of a local package
   * REMOTE:  a URL to a Git repository containing a Roo project
   */
  std::string path;
};

struct StringConstant
{
  StringConstant(ParseResult& parse, const std::string& str);

  unsigned int      handle;
  std::string       str;

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
  TypeDef(const std::string& name);
  ~TypeDef();

  std::string             name;
  std::vector<MemberDef*> members;
  ErrorState*             errorState;

  /*
   * Size of this structure in bytes.
   * NOTE(Isaac): provided for inbuilt types, calculated for composite types by `CalculateTypeSizes`.
   */
  unsigned int            size;
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
  bool        isMutable;           // For references, this describes the mutability of the reference
  bool        isReference;
  bool        isReferenceMutable;  // Describes the mutability of the reference's contents

  bool        isArray;
  bool        isArraySizeResolved;
  union
  {
    ASTNode*      arraySizeExpression;
    unsigned int  arraySize;
  };
};

/*
 * This defines a member of a type. It does not represent a variable of that type's member, just the type's
 * definition of the idea of the member. It is in part a separate type to stop errors occuring when the member
 * definition is accidently referenced.
 */
struct MemberDef
{
  MemberDef(const std::string& name, const TypeRef& type, ASTNode* initExpression, int offset = 0);
  ~MemberDef();

  std::string name;
  TypeRef     type;
  ASTNode*    initExpression;

  /*
   * NOTE(Isaac): this can be used to represent multiple things:
   *    * Offset from the base-pointer into the stack frame (for variables on the stack).
   *      On x64, this offset should be *taken* away from the base-pointer (because the stack grows downwards.
   *    * Offset inside a parent structure (for members of structures)
   */
  int offset;
};

/*
 * This describes the definition of a variable, and can also be used to refer to variables that have already
 * been defined. Its initial value should be assigned to its allocated register/memory when it enters scope.
 */
struct VariableDef
{
  /*
   * This describes where the variable should be located. Usually, it will be in a register unless it is larger
   * than a general register on the target machine.
   */
  enum Storage
  {
    UNDECIDED,    // NOTE(Isaac): This should only be used as a placeholder in the incomplete IR!
    REGISTER,
    STACK
  };

  VariableDef(const std::string& name, const TypeRef& typeRef, ASTNode* initExpression, int offset = 0);
  ~VariableDef();

  /*
   * This returns a character representing the storage of this variable. This is used to pretty-print slots etc.
   * in other parts of the compiler.
   */
  char GetStorageChar();

  std::string               name;
  TypeRef                   type;
  ASTNode*                  initExpression;
  Storage                   storage;
  Slot*                     slot;

  /*
   * If this variable's type has accessible members, we need to manage them separately.
   * They should be in the same order as the member definitions in the type.
   */
  std::vector<VariableDef*> members;

  /*
   * NOTE(Isaac): this can be used to represent multiple things:
   *    * Offset from the base-pointer into the stack frame (for variables on the stack).
   *      On x64, this offset should be *taken* away from the base-pointer (because the stack grows downwards.
   *    * Offset inside a parent structure (for members of structures)
   */
  int                       offset;
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
 * This describes a scope inside a code block. A scope contains local variables that can only be accessed from
 * within it, and can be inside another scope (it's 'parent'). Code inside a scope can also access variables within
 * its scope's parent.
 */
struct ScopeDef
{
  ScopeDef(CodeThing* thing, ScopeDef* parent);
  ~ScopeDef() { }
 
  /*
   * This gets a list of all variables reachable from this scope.
   * (Basically variables in this scope and its parent's scopes (recursively))
   */
  std::vector<VariableDef*> GetReachableVariables();

  ScopeDef*                 parent;
  std::vector<VariableDef*> locals;
};

/*
 * This describes a function (either parsed as an actual function or as an operator) that takes inputs (parameters)
 * does stuff to them (described by the AST and then the AIR code), and produces an output (the return value).
 * Functions supply a name in the `name` field, whereas operators supply the type of the token used to denote the
 * operation (addition uses TOKEN_PLUS, multiplication TOKEN_ASTERIX etc.) in the `op` field.
 */
struct CodeThing
{
  enum Type
  {
    FUNCTION,
    OPERATOR
  };

  CodeThing(Type type);
  virtual ~CodeThing();

  Type                      type;
  std::string               mangledName;
  std::vector<VariableDef*> params;
  std::vector<ScopeDef*>    scopes;
  bool                      shouldAutoReturn;
  AttribSet                 attribs;
  TypeRef*                  returnType;       // `nullptr` if it doesn't return anything

  ErrorState*               errorState;
  std::vector<CodeThing*>   calledThings;

  // AST representation
  ASTNode*                  ast;

  // AIR representation
  std::vector<Slot*>        slots;
  unsigned int              stackFrameSize;
  AirInstruction*           airHead;
  AirInstruction*           airTail;
  unsigned int              numTemporaries;
  unsigned int              numReturnResults;
  unsigned int              neededStackSpace;   // This is the size (in bytes) that we need to grow the stack frame by to fit local variables etc.

  // Final executable stuff
  ElfSymbol*                symbol;
};

struct FunctionThing : CodeThing
{
  FunctionThing(const std::string& name);

  std::string name;
};

struct OperatorThing : CodeThing
{
  OperatorThing(TokenType token);

  TokenType token;
};

TypeDef* GetTypeByName(ParseResult& parse, const std::string& name);
bool AreTypeRefsCompatible(TypeRef* a, TypeRef* b, bool careAboutMutability = true);
void CompleteIR(ParseResult& parse, TargetMachine* target);
