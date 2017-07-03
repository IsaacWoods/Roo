/*
 * Copyright (C) 2017, Isaac Woods.
 * See LICENCE.md
 */

#pragma once

#include <cstdint>
#include <vector>
#include <ast.hpp>
#include <ir.hpp>

struct AirInstruction;
struct CodegenTarget;

struct LiveRange
{
  LiveRange(AirInstruction* definition, AirInstruction* lastUse);
  ~LiveRange() { }

  /*
   * If `definition` is nullptr, the slot has a value on entry into the scope.
   */
  AirInstruction* definition;
  AirInstruction* lastUse;
};

/*
 * XXX: This feels a bit messy, but we need a way to tell what type of slot we're dealing with and dynamically
 * dispatching all the time is a bit of a pain in the ass.
 */
enum class SlotType
{
  VARIABLE,
  PARAMETER,
  MEMBER,
  TEMPORARY,
  RETURN_RESULT,
  UNSIGNED_INT_CONSTANT,
  INT_CONSTANT,
  FLOAT_CONSTANT,
  STRING_CONSTANT
};

struct Slot
{
  enum Storage
  {
    REGISTER,
    STACK
  };

  Slot(ThingOfCode* code);
  virtual ~Slot() = default;

  Storage                 storage;
  signed int              color;          // -1 means it hasn't been colored
  std::vector<Slot*>      interferences;
  std::vector<LiveRange>  liveRanges;
#ifdef OUTPUT_DOT
  unsigned int dotTag;
#endif

  bool IsColored()
  {
    return (color != -1);
  }

  virtual SlotType GetType() = 0;
  virtual bool IsConstant() = 0;
  virtual bool ShouldColor() = 0;
  virtual void Use(AirInstruction* instruction) = 0;
  virtual void ChangeValue(AirInstruction* instruction) = 0;
  virtual std::string AsString() = 0;
};

struct VariableSlot : Slot
{
  VariableSlot(ThingOfCode* code, VariableDef* variable);
  ~VariableSlot() { }

  VariableDef* variable;

  SlotType GetType()  { return SlotType::VARIABLE;  }
  bool IsConstant()   { return false;               }
  bool ShouldColor()  { return true;                }
  void Use(AirInstruction* instruction);
  void ChangeValue(AirInstruction* instruction);
  std::string AsString();
};

struct ParameterSlot : Slot
{
  ParameterSlot(ThingOfCode* code, VariableDef* parameter);
  ~ParameterSlot() { }

  VariableDef* parameter;

  SlotType GetType()  { return SlotType::PARAMETER; }
  bool IsConstant()   { return false;               }
  bool ShouldColor()  { return false;               }
  void Use(AirInstruction* instruction);
  void ChangeValue(AirInstruction* instruction);
  std::string AsString();
};

struct MemberSlot : Slot
{
  MemberSlot(ThingOfCode* code, Slot* parent, VariableDef* member);
  ~MemberSlot() { }

  Slot*         parent;
  VariableDef*  member;

  SlotType GetType()  { return SlotType::MEMBER;  }
  bool IsConstant()   { return false;             }
  bool ShouldColor()  { return false;             }
  void Use(AirInstruction* instruction);
  void ChangeValue(AirInstruction* instruction);
  std::string AsString();
};

struct TemporarySlot : Slot
{
  TemporarySlot(ThingOfCode* code);

  unsigned int tag;

  SlotType GetType()  { return SlotType::TEMPORARY; }
  bool IsConstant()   { return false;               }
  bool ShouldColor()  { return true;                }
  void Use(AirInstruction* instruction);
  void ChangeValue(AirInstruction* instruction);
  std::string AsString();
};

struct ReturnResultSlot : Slot
{
  ReturnResultSlot(ThingOfCode* code);

  unsigned int tag;

  SlotType GetType()  { return SlotType::RETURN_RESULT; }
  bool IsConstant()   { return false;                   }
  bool ShouldColor()  { return false;                   }
  void Use(AirInstruction* instruction);
  void ChangeValue(AirInstruction* instruction);
  std::string AsString();
};

template<typename T>
struct ConstantSlot : Slot
{
  ConstantSlot(ThingOfCode* code, T value)
    :Slot(code)
    ,value(value)
  {
    static_assert(std::is_same<T, unsigned int>::value  ||
                  std::is_same<T, int>::value           ||
                  std::is_same<T, float>::value         ||
                  std::is_same<T, StringConstant*>::value,
                  "ConstantSlot can only be: unsigned int, int, float, StringConstant*");
  }

  T value;

  SlotType GetType()
  {
    if (std::is_same<T, unsigned int>::value)
    {
      return SlotType::UNSIGNED_INT_CONSTANT;
    }
    else if (std::is_same<T, int>::value)
    {
      return SlotType::INT_CONSTANT;
    }
    else if (std::is_same<T, float>::value)
    {
      return SlotType::FLOAT_CONSTANT;
    }
    else
    {
      return SlotType::STRING_CONSTANT;
    }
  }

  bool IsConstant()   { return true;  }
  bool ShouldColor()  { return false; }
  void Use(AirInstruction* /*instruction*/) { }
  void ChangeValue(AirInstruction* /*instruction*/) { }
  std::string AsString();
};

struct AirInstruction
{
  AirInstruction();
  virtual ~AirInstruction();

  virtual std::string AsString() = 0;

  signed int      index;    // NOTE(Isaac): value of -1 used to detect instructions that haven't been pushed yet
  AirInstruction* next;
};

/*
 * This really an actual instruction, but more a placeholder to jump to
 * within the instruction stream.
 */
struct LabelInstruction : AirInstruction
{
  LabelInstruction();

  std::string AsString();

  /*
   * This is the offset from the symbol of the containing function.
   */
  uint64_t offset;
};

struct ReturnInstruction : AirInstruction
{
  ReturnInstruction(Slot* returnValue);
  ~ReturnInstruction() { }

  std::string AsString();

  Slot* returnValue;
};

struct JumpInstruction : AirInstruction
{
  enum Condition
  {
    UNCONDITIONAL,
    IF_EQUAL,
    IF_NOT_EQUAL,
    IF_OVERFLOW,
    IF_NOT_OVERFLOW,
    IF_SIGN,
    IF_NOT_SIGN,
    IF_GREATER,
    IF_GREATER_OR_EQUAL,
    IF_LESSER,
    IF_LESSER_OR_EQUAL,
    IF_PARITY_EVEN,
    IF_PARITY_ODD
  };

  JumpInstruction(Condition condition, LabelInstruction* label);
  ~JumpInstruction() { }

  std::string AsString();

  Condition         condition;
  LabelInstruction* label;
};

struct MovInstruction : AirInstruction
{
  MovInstruction(Slot* src, Slot* dest);
  ~MovInstruction() { }

  std::string AsString();

  Slot* src;
  Slot* dest;
};

struct CmpInstruction : AirInstruction
{
  CmpInstruction(Slot* a, Slot* b);
  ~CmpInstruction() { }

  std::string AsString();

  Slot* a;
  Slot* b;
};

struct UnaryOpInstruction : AirInstruction
{
  enum Operation
  {
    INCREMENT,
    DECREMENT,
    NEGATE,
    LOGICAL_NOT,
  };

  UnaryOpInstruction(Operation op, Slot* result, Slot* operand);
  ~UnaryOpInstruction() { }

  std::string AsString();

  Operation op;
  Slot*     result;
  Slot*     operand;
};

struct BinaryOpInstruction : AirInstruction
{
  enum Operation
  {
    ADD,
    SUBTRACT,
    MULTIPLY,
    DIVIDE
  };

  BinaryOpInstruction(Operation op, Slot* result, Slot* left, Slot* right);
  ~BinaryOpInstruction() { }

  std::string AsString();

  Operation op;
  Slot*     result;
  Slot*     left;
  Slot*     right;
};

struct CallInstruction : AirInstruction
{
  CallInstruction(ThingOfCode* thing);
  ~CallInstruction() { }

  std::string AsString();

  ThingOfCode* thing;
};

/*
 * This traverses the AST and builds an AIR instruction sequence from it.
 * Most of these obviously actually generate AIR instructions, but they don't have to (for example, constants
 * simply return a Slot with the correct constant value).
 */
struct AirGenerator : ASTPass<Slot*, ThingOfCode>
{
  AirGenerator(CodegenTarget& target)
    :ASTPass(true)
    ,target(target)
  {
  }

  CodegenTarget& target;

  void Apply(ParseResult& parse);

  Slot* VisitNode(BreakNode* node                 , ThingOfCode* code);
  Slot* VisitNode(ReturnNode* node                , ThingOfCode* code);
  Slot* VisitNode(UnaryOpNode* node               , ThingOfCode* code);
  Slot* VisitNode(BinaryOpNode* node              , ThingOfCode* code);
  Slot* VisitNode(VariableNode* node              , ThingOfCode* code);
  Slot* VisitNode(ConditionNode* node             , ThingOfCode* code);
  Slot* VisitNode(BranchNode* node                , ThingOfCode* code);
  Slot* VisitNode(WhileNode* node                 , ThingOfCode* code);
  Slot* VisitNode(NumberNode<unsigned int>* node  , ThingOfCode* code);
  Slot* VisitNode(NumberNode<int>* node           , ThingOfCode* code);
  Slot* VisitNode(NumberNode<float>* node         , ThingOfCode* code);
  Slot* VisitNode(StringNode* node                , ThingOfCode* code);
  Slot* VisitNode(CallNode* node                  , ThingOfCode* code);
  Slot* VisitNode(VariableAssignmentNode* node    , ThingOfCode* code);
  Slot* VisitNode(MemberAccessNode* node          , ThingOfCode* code);
  Slot* VisitNode(ArrayInitNode* node             , ThingOfCode* code);
};

bool IsColorInUseAtPoint(ThingOfCode* code, AirInstruction* instruction, signed int color);

/*
 * This allows dynamic dispatch based upon the type of an AIR instruction.
 */
template<typename T=void>
struct AirPass
{
  bool errorOnNonexistantPass;

  AirPass(bool errorOnNonexistantPass)
    :errorOnNonexistantPass(errorOnNonexistantPass)
  {
  }

  #define BASE_CASE(nodeType)\
  {\
    if (errorOnNonexistantPass)\
    {\
      RaiseError(ICE_NONEXISTANT_AIR_PASSLET, nodeType);\
    }\
  }

  virtual void Visit(LabelInstruction*,     T* = nullptr) BASE_CASE("LabelInstruction");
  virtual void Visit(ReturnInstruction*,    T* = nullptr) BASE_CASE("ReturnInstruction");
  virtual void Visit(JumpInstruction*,      T* = nullptr) BASE_CASE("JumpInstruction");
  virtual void Visit(MovInstruction*,       T* = nullptr) BASE_CASE("MovInstruction");
  virtual void Visit(CmpInstruction*,       T* = nullptr) BASE_CASE("CmpInstruction");
  virtual void Visit(UnaryOpInstruction*,   T* = nullptr) BASE_CASE("UnaryOpInstruction");
  virtual void Visit(BinaryOpInstruction*,  T* = nullptr) BASE_CASE("BinaryOpInstruction");
  virtual void Visit(CallInstruction*,      T* = nullptr) BASE_CASE("CallInstruction");
  #undef BASE_CASE

  /*
   * This uses the same bodged dynamic dispatch approach as the AST pass system.
   * There is more justification for it there.
   */
  void Dispatch(AirInstruction* instruction, T* state = nullptr)
  {
    Assert(instruction, "Tried to dispatch on a nullptr instruction");

    // XXX: Again, if this is too slow, we could instead compare an id returned from the node virtually
    // (but this is more fragile because we have to manange the ids ourselves)
    #define DISPATCH(instructionType)\
      if (strcmp(typeid(*instruction).name(), typeid(instructionType).name()) == 0)\
      {\
        Visit(reinterpret_cast<instructionType*>(instruction), state);\
        return;\
      }

         DISPATCH(LabelInstruction)
    else DISPATCH(ReturnInstruction)
    else DISPATCH(JumpInstruction)
    else DISPATCH(MovInstruction)
    else DISPATCH(CmpInstruction)
    else DISPATCH(UnaryOpInstruction)
    else DISPATCH(BinaryOpInstruction)
    else DISPATCH(CallInstruction)
    else
    {
      RaiseError(ICE_UNHANDLED_INSTRUCTION_TYPE, "Dispatch(AirPass)", typeid(*instruction).name());
    }
    #undef DISPATCH

    __builtin_unreachable();
  }
};
