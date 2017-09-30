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
struct TargetMachine;

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
  BOOL_CONSTANT,
  STRING_CONSTANT
};

struct Slot
{
  Slot(CodeThing* code);
  virtual ~Slot() = default;

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
  VariableSlot(CodeThing* code, VariableDef* variable);
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
  ParameterSlot(CodeThing* code, VariableDef* parameter);
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
  MemberSlot(CodeThing* code, Slot* parent, VariableDef* member);
  ~MemberSlot() { }

  Slot*         parent;
  VariableDef*  member;

  int GetBasePointerOffset();

  SlotType GetType()  { return SlotType::MEMBER;  }
  bool IsConstant()   { return false;             }
  bool ShouldColor()  { return false;             }
  void Use(AirInstruction* instruction);
  void ChangeValue(AirInstruction* instruction);
  std::string AsString();
};

struct TemporarySlot : Slot
{
  TemporarySlot(CodeThing* code);

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
  ReturnResultSlot(CodeThing* code);

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
  ConstantSlot(CodeThing* code, T value)
    :Slot(code)
    ,value(value)
  {
    static_assert(std::is_same<T, unsigned int>::value    ||
                  std::is_same<T, int>::value             ||
                  std::is_same<T, float>::value           ||
                  std::is_same<T, bool>::value            ||
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
    else if (std::is_same<T, bool>::value)
    {
      return SlotType::BOOL_CONSTANT;
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

  UnaryOpInstruction(Operation op, IntrinsicOpType type, Slot* result, Slot* operand);
  ~UnaryOpInstruction() { }

  std::string AsString();

  Operation       op;
  IntrinsicOpType type;
  Slot*           result;
  Slot*           operand;
};

/*
 * These represent intrinsic binary operations. Not all `BinaryOpNode`s yield one of these (nor should they)!
 */
struct BinaryOpInstruction : AirInstruction
{
  enum Operation
  {
    ADD,
    SUBTRACT,
    MULTIPLY,
    DIVIDE,
  };

  BinaryOpInstruction(Operation op, IntrinsicOpType type, Slot* result, Slot* left, Slot* right);
  ~BinaryOpInstruction() { }

  std::string AsString();

  Operation       op;
  IntrinsicOpType type;
  Slot*           result;
  Slot*           left;
  Slot*           right;
};

struct CallInstruction : AirInstruction
{
  CallInstruction(CodeThing* thing);
  ~CallInstruction() { }

  std::string AsString();

  CodeThing* thing;
};

/*
 * This traverses the AST and builds an AIR instruction sequence from it.
 * Most of these obviously actually generate AIR instructions, but they don't have to (for example, constants
 * simply return a Slot with the correct constant value).
 */
struct AirState
{
  AirState(TargetMachine* target, CodeThing* code)
    :target(target)
    ,code(code)
    ,breakLabel(nullptr)
  {
  }
  ~AirState() { }

  TargetMachine* target;
  CodeThing* code;

  /*
   * If we're inside a loop, we set this to the label that should be jumped to upon a `break`
   */
  LabelInstruction* breakLabel;
};

struct AirGenerator : ASTPass<Slot*, AirState>
{
  AirGenerator()
    :ASTPass()
  { }

  void Apply(ParseResult& parse, TargetMachine* target);

  Slot* VisitNode(BreakNode* node                   , AirState* state);
  Slot* VisitNode(ReturnNode* node                  , AirState* state);
  Slot* VisitNode(UnaryOpNode* node                 , AirState* state);
  Slot* VisitNode(BinaryOpNode* node                , AirState* state);
  Slot* VisitNode(VariableNode* node                , AirState* state);
  Slot* VisitNode(ConditionNode* node               , AirState* state);
  Slot* VisitNode(CompositeConditionNode* node      , AirState* state);
  Slot* VisitNode(BranchNode* node                  , AirState* state);
  Slot* VisitNode(WhileNode* node                   , AirState* state);
  Slot* VisitNode(ConstantNode<unsigned int>* node  , AirState* state);
  Slot* VisitNode(ConstantNode<int>* node           , AirState* state);
  Slot* VisitNode(ConstantNode<float>* node         , AirState* state);
  Slot* VisitNode(ConstantNode<bool>* node          , AirState* state);
  Slot* VisitNode(StringNode* node                  , AirState* state);
  Slot* VisitNode(CallNode* node                    , AirState* state);
  Slot* VisitNode(VariableAssignmentNode* node      , AirState* state);
  Slot* VisitNode(MemberAccessNode* node            , AirState* state);
  Slot* VisitNode(ArrayInitNode* node               , AirState* state);
  Slot* VisitNode(InfiniteLoopNode* node            , AirState* state);
  Slot* VisitNode(ConstructNode* node               , AirState* state);
};

bool IsColorInUseAtPoint(CodeThing* code, AirInstruction* instruction, signed int color);

/*
 * This allows dynamic dispatch based upon the type of an AIR instruction.
 */
template<typename T=void>
struct AirPass
{
  AirPass() = default;

  virtual void Visit(LabelInstruction*,     T* = nullptr) = 0;
  virtual void Visit(ReturnInstruction*,    T* = nullptr) = 0;
  virtual void Visit(JumpInstruction*,      T* = nullptr) = 0;
  virtual void Visit(MovInstruction*,       T* = nullptr) = 0;
  virtual void Visit(CmpInstruction*,       T* = nullptr) = 0;
  virtual void Visit(UnaryOpInstruction*,   T* = nullptr) = 0;
  virtual void Visit(BinaryOpInstruction*,  T* = nullptr) = 0;
  virtual void Visit(CallInstruction*,      T* = nullptr) = 0;

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
