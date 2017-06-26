/*
 * Copyright (C) 2017, Isaac Woods.
 * See LICENCE.md
 */

#pragma once

#include <cstdint>
#include <vector>
#include <ast.hpp>
#include <ir.hpp>
#include <codegen.hpp>

struct AirInstruction;
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

struct Slot
{
  enum Storage
  {
    REGISTER,
    STACK
  };

  Slot() = default;

  Storage                 storage;
  signed int              color;          // -1 means it hasn't been colored
  std::vector<Slot*>      interferences;
  std::vector<LiveRange>  liveRanges;

  virtual void Use(AirInstruction* instruction) = 0;
  virtual void ChangeValue(AirInstruction* instruction) = 0;
};

struct VariableSlot : Slot
{
  VariableSlot(VariableDef* variable);
  ~VariableSlot() { }

  VariableDef* variable;

  void Use(AirInstruction* instruction);
  void ChangeValue(AirInstruction* instruction);
};

struct ParameterSlot : Slot
{
  ParameterSlot(VariableDef* parameter);
  ~ParameterSlot() { }

  VariableDef* parameter;

  void Use(AirInstruction* instruction);
  void ChangeValue(AirInstruction* instruction);
};

struct MemberSlot : Slot
{
  MemberSlot(Slot* parent, VariableDef* member);
  ~MemberSlot() { }

  Slot*         parent;
  VariableDef* member;

  void Use(AirInstruction* instruction);
  void ChangeValue(AirInstruction* instruction);
};

struct TemporarySlot : Slot
{
  TemporarySlot(unsigned int tag);

  unsigned int tag;

  void Use(AirInstruction* instruction);
  void ChangeValue(AirInstruction* instruction);
};

struct ReturnResultSlot : Slot
{
  ReturnResultSlot(unsigned int tag);

  unsigned int tag;

  void Use(AirInstruction* instruction);
  void ChangeValue(AirInstruction* instruction);
};

template<typename T>
struct ConstantSlot : Slot
{
  ConstantSlot(T value)
    :Slot()
    ,value(value)
  {
    static_assert(std::is_same<T, unsigned int>::value  ||
                  std::is_same<T, int>::value           ||
                  std::is_same<T, float>::value         ||
                  std::is_same<T, StringConstant*>::value,
                  "ConstantSlot can only be: unsigned int, int, float, StringConstant*");
  }

  T value;

  void Use(AirInstruction* /*instruction*/) { }
  void ChangeValue(AirInstruction* /*instruction*/) { }
};

struct AirInstruction
{
  AirInstruction();
  ~AirInstruction();

  unsigned int    index;
  AirInstruction* next;

  void Use(AirInstruction* instruction);
};

/*
 * This really an actual instruction, but more a placeholder to jump to
 * within the instruction stream.
 */
struct LabelInstruction : AirInstruction
{
  LabelInstruction();

  /*
   * This is the offset from the symbol of the containing function.
   */
  uint64_t offset;
};

struct ReturnInstruction : AirInstruction
{
  ReturnInstruction(Slot* returnValue);
  ~ReturnInstruction() { }

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

  Condition         condition;
  LabelInstruction* label;
};

struct MovInstruction : AirInstruction
{
  MovInstruction(Slot* src, Slot* dest);
  ~MovInstruction() { }

  Slot* src;
  Slot* dest;
};

struct CmpInstruction : AirInstruction
{
  CmpInstruction(Slot* a, Slot* b);
  ~CmpInstruction() { }

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

  Operation op;
  Slot*     result;
  Slot*     left;
  Slot*     right;
};

struct CallInstruction : AirInstruction
{
  CallInstruction(ThingOfCode* thing);
  ~CallInstruction() { }

  ThingOfCode* thing;
};

/*
 * This traverses the AST and builds an AIR instruction sequence from it.
 * Most of these obviously actually generate AIR instructions, but they don't have to (for example, constants
 * simply return a Slot with the correct constant value).
 */
struct AirGenerator : ASTPass<Slot*, ThingOfCode>
{
  AirGenerator()
    :ASTPass(true)
  {
  }

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

void GenerateAIR(CodegenTarget& target, ThingOfCode* code);
