/*
 * Copyright (C) 2017, Isaac Woods.
 * See LICENCE.md
 */

#include <air.hpp>

static void UseSlot(Slot* slot, AirInstruction* instruction)
{
  if (slot->liveRanges.size() >= 1u)
  {
    LiveRange& lastRange = slot->liveRanges.back();

    if (lastRange.definition)
    {
      Assert(lastRange.definition->index < instruction->index, "Slot used before being defined");
      lastRange.lastUse = instruction;
    }
  }
  else
  {
    // TODO: get a proper string representation of the slot
    RaiseError(ERROR_BIND_USED_BEFORE_INIT, "SomeSlot");
  }
}

void VariableSlot     ::Use(AirInstruction* instruction) { UseSlot(this, instruction); }
void ParameterSlot    ::Use(AirInstruction* instruction) { UseSlot(this, instruction); }
void MemberSlot       ::Use(AirInstruction* instruction) { UseSlot(this, instruction); }
void TemporarySlot    ::Use(AirInstruction* instruction) { UseSlot(this, instruction); }
void ReturnResultSlot ::Use(AirInstruction* instruction) { UseSlot(this, instruction); }

static void ChangeSlotValue(Slot* slot, AirInstruction* instruction)
{
  slot->liveRanges.push_back(LiveRange(instruction, nullptr));
}

void VariableSlot     ::ChangeValue(AirInstruction* instruction) { ChangeSlotValue(this, instruction); }
void ParameterSlot    ::ChangeValue(AirInstruction* instruction) { ChangeSlotValue(this, instruction); }
void MemberSlot       ::ChangeValue(AirInstruction* instruction) { ChangeSlotValue(this, instruction); }
void TemporarySlot    ::ChangeValue(AirInstruction* instruction) { ChangeSlotValue(this, instruction); }
void ReturnResultSlot ::ChangeValue(AirInstruction* instruction) { ChangeSlotValue(this, instruction); }

LiveRange::LiveRange(AirInstruction* definition, AirInstruction* lastUse)
  :definition(definition)
  ,lastUse(lastUse)
{
}

VariableSlot::VariableSlot(VariableDef* variable)
  :Slot()
  ,variable(variable)
{
}

ParameterSlot::ParameterSlot(VariableDef* parameter)
  :Slot()
  ,parameter(parameter)
{
}

MemberSlot::MemberSlot(Slot* parent, VariableDef* member)
  :Slot()
  ,parent(parent)
  ,member(member)
{
}

TemporarySlot::TemporarySlot(unsigned int tag)
  :Slot()
  ,tag(tag)
{
}

ReturnResultSlot::ReturnResultSlot(unsigned int tag)
  :Slot()
  ,tag(tag)
{
}

AirInstruction::AirInstruction()
  :index(0u)
  ,next(nullptr)
{
}

AirInstruction::~AirInstruction()
{
  delete next;
}

LabelInstruction::LabelInstruction()
  :AirInstruction()
  ,offset(0u)
{
}

ReturnInstruction::ReturnInstruction(Slot* returnValue)
  :AirInstruction()
  ,returnValue(returnValue)
{
}

JumpInstruction::JumpInstruction(JumpInstruction::Condition condition, LabelInstruction* label)
  :AirInstruction()
  ,condition(condition)
  ,label(label)
{
}

MovInstruction::MovInstruction(Slot* src, Slot* dest)
  :AirInstruction()
  ,src(src)
  ,dest(dest)
{
}

CmpInstruction::CmpInstruction(Slot* a, Slot* b)
  :AirInstruction()
  ,a(a)
  ,b(b)
{
}

UnaryOpInstruction::UnaryOpInstruction(UnaryOpInstruction::Operation op, Slot* result, Slot* operand)
  :AirInstruction()
  ,op(op)
  ,result(result)
  ,operand(operand)
{
}

BinaryOpInstruction::BinaryOpInstruction(BinaryOpInstruction::Operation op, Slot* result, Slot* left, Slot* right)
  :AirInstruction()
  ,op(op)
  ,result(result)
  ,left(left)
  ,right(right)
{
}

CallInstruction::CallInstruction(ThingOfCode* thing)
  :AirInstruction()
  ,thing(thing)
{
}

// AirGenerator
static void PushInstruction(ThingOfCode* code, AirInstruction* instruction)
{
  if (code->airHead)
  {
    instruction->index = code->airTail->index + 1u;
    code->airTail->next = instruction;
    code->airTail = instruction;
  }
  else
  {
    instruction->index = 0u;
    code->airHead = instruction;
    code->airTail = instruction;
  }
}

Slot* AirGenerator::VisitNode(BreakNode* node, ThingOfCode* code)
{
  // TODO: we need to somehow link to the end of the loop (maybe we should be marking the AST with this info
  // beforehand?)
}

Slot* AirGenerator::VisitNode(ReturnNode* node, ThingOfCode* code)
{
  Slot* returnValue = (node->returnValue ? DispatchNode(node->returnValue, code) : nullptr);
  PushInstruction(code, new ReturnInstruction(returnValue));
  return nullptr;
}

Slot* AirGenerator::VisitNode(UnaryOpNode* node, ThingOfCode* code)
{
  Slot* operand = DispatchNode(node->operand, code);
  Slot* result = new TemporarySlot(code->numTemporaries++);

  switch (node->op)
  {
    case UnaryOpNode::Operator::POSITIVE:
    {
      // TODO
    } break;

    case UnaryOpNode::Operator::NEGATIVE:
    {
      // TODO
    } break;

    case UnaryOpNode::Operator::NEGATE:
    {
      AirInstruction* neg = new UnaryOpInstruction(UnaryOpInstruction::Operation::NEGATE, result, operand);
      PushInstruction(code, neg);
      UseSlot(operand, neg);
      ChangeSlotValue(result, neg);
    } break;

    case UnaryOpNode::Operator::LOGICAL_NOT:
    {
      AirInstruction* notI = new UnaryOpInstruction(UnaryOpInstruction::Operation::LOGICAL_NOT, result, operand);
      PushInstruction(code, notI);
      UseSlot(operand, notI);
      ChangeSlotValue(result, notI);
    } break;

    case UnaryOpNode::Operator::TAKE_REFERENCE:
    {
      // TODO
    } break;

    case UnaryOpNode::Operator::PRE_INCREMENT:
    {
      AirInstruction* inc = new UnaryOpInstruction(UnaryOpInstruction::Operation::INCREMENT, result, operand);
      PushInstruction(code, inc);
      UseSlot(operand, inc);
      ChangeSlotValue(result, inc);
    } break;

    case UnaryOpNode::Operator::POST_INCREMENT:
    {
      AirInstruction* mov = new MovInstruction(operand, result);
      AirInstruction* inc = new UnaryOpInstruction(UnaryOpInstruction::Operation::INCREMENT, operand, operand);

      UseSlot(operand, mov);
      UseSlot(operand, inc);
      ChangeSlotValue(result, mov);
      ChangeSlotValue(operand, inc);

      PushInstruction(code, mov);
      PushInstruction(code, inc);
    } break;

    case UnaryOpNode::Operator::PRE_DECREMENT:
    {
      AirInstruction* dec = new UnaryOpInstruction(UnaryOpInstruction::Operation::DECREMENT, result, operand);
      PushInstruction(code, dec);
      UseSlot(operand, dec);
      ChangeSlotValue(result, dec);
    } break;

    case UnaryOpNode::Operator::POST_DECREMENT:
    {
      AirInstruction* mov = new MovInstruction(operand, result);
      AirInstruction* dec = new UnaryOpInstruction(UnaryOpInstruction::Operation::DECREMENT, operand, operand);

      UseSlot(operand, mov);
      UseSlot(operand, dec);
      ChangeSlotValue(result, mov);
      ChangeSlotValue(operand, dec);

      PushInstruction(code, mov);
      PushInstruction(code, dec);
    } break;
  }

  return result;
}

Slot* AirGenerator::VisitNode(BinaryOpNode* node, ThingOfCode* code)
{
  // TODO
}

Slot* AirGenerator::VisitNode(VariableNode* node, ThingOfCode* code)
{
  Assert(node->isResolved, "Tried to generate AIR for unresolved variable");
  return node->var->slot;
}

Slot* AirGenerator::VisitNode(ConditionNode* node, ThingOfCode* code)
{
  Slot* a = DispatchNode(node->left, code);
  Slot* b = DispatchNode(node->right, code);
  PushInstruction(code, new CmpInstruction(a, b));
  return nullptr;
}

Slot* AirGenerator::VisitNode(BranchNode* node, ThingOfCode* code)
{
  DispatchNode(node->condition, code);

  LabelInstruction* elseLabel = (node->elseCode ? new LabelInstruction() : nullptr);
  LabelInstruction* endLabel = new LabelInstruction();

  JumpInstruction::Condition condition;
  switch (node->condition->condition)
  {
    /*
     * We reverse the jump condition here, because we want to jump (and skip the 'then' branch) if the
     * condition is *not* true.
     */
    case ConditionNode::Condition::EQUAL:                   condition = JumpInstruction::Condition::IF_NOT_EQUAL;         break;
    case ConditionNode::Condition::NOT_EQUAL:               condition = JumpInstruction::Condition::IF_EQUAL;             break;
    case ConditionNode::Condition::LESS_THAN:               condition = JumpInstruction::Condition::IF_GREATER_OR_EQUAL;  break;
    case ConditionNode::Condition::LESS_THAN_OR_EQUAL:      condition = JumpInstruction::Condition::IF_GREATER;           break;
    case ConditionNode::Condition::GREATER_THAN:            condition = JumpInstruction::Condition::IF_LESSER_OR_EQUAL;   break;
    case ConditionNode::Condition::GREATER_THAN_OR_EQUAL:   condition = JumpInstruction::Condition::IF_LESSER;            break;
  }

  PushInstruction(code, new JumpInstruction(condition, (elseLabel ? elseLabel : endLabel)));
  DispatchNode(node->thenCode, code);
  if (elseLabel)
  {
    PushInstruction(code, elseLabel);
    DispatchNode(node->elseCode, code);
  }
  PushInstruction(code, endLabel);
  return nullptr;
}

Slot* AirGenerator::VisitNode(WhileNode* node, ThingOfCode* code)
{
  LabelInstruction* label = new LabelInstruction();
  PushInstruction(code, label);

  DispatchNode(node->loopBody, code);

  DispatchNode(node->condition, code);
  JumpInstruction::Condition condition;
  switch (node->condition->condition)
  {
    case ConditionNode::Condition::EQUAL:                   condition = JumpInstruction::Condition::IF_EQUAL;             break;
    case ConditionNode::Condition::NOT_EQUAL:               condition = JumpInstruction::Condition::IF_NOT_EQUAL;         break;
    case ConditionNode::Condition::LESS_THAN:               condition = JumpInstruction::Condition::IF_LESSER;            break;
    case ConditionNode::Condition::LESS_THAN_OR_EQUAL:      condition = JumpInstruction::Condition::IF_LESSER_OR_EQUAL;   break;
    case ConditionNode::Condition::GREATER_THAN:            condition = JumpInstruction::Condition::IF_GREATER;           break;
    case ConditionNode::Condition::GREATER_THAN_OR_EQUAL:   condition = JumpInstruction::Condition::IF_GREATER_OR_EQUAL;  break;
  }

  PushInstruction(code, new JumpInstruction(condition, label));
  return nullptr;
}

Slot* AirGenerator::VisitNode(NumberNode<unsigned int>* node, ThingOfCode* code)
{
  Slot* slot = new ConstantSlot<unsigned int>(node->value);
  code->slots.push_back(slot);
  return slot;
}

Slot* AirGenerator::VisitNode(NumberNode<int>* node, ThingOfCode* code)
{
  Slot* slot = new ConstantSlot<int>(node->value);
  code->slots.push_back(slot);
  return slot;
}

Slot* AirGenerator::VisitNode(NumberNode<float>* node, ThingOfCode* code)
{
  Slot* slot = new ConstantSlot<float>(node->value);
  code->slots.push_back(slot);
  return slot;
}

Slot* AirGenerator::VisitNode(StringNode* node, ThingOfCode* code)
{
  Slot* slot = new ConstantSlot<StringConstant*>(node->string);
  code->slots.push_back(slot);
  return slot;
}

Slot* AirGenerator::VisitNode(CallNode* node, ThingOfCode* code)
{
}

Slot* AirGenerator::VisitNode(VariableAssignmentNode* node, ThingOfCode* code)
{
  Slot* variable = DispatchNode(node->variable, code);
  Slot* newValue = DispatchNode(node->newValue, code);
  AirInstruction* mov = new MovInstruction(newValue, variable);
  ChangeSlotValue(variable, mov);
  UseSlot(newValue, mov);

  PushInstruction(code, mov);
  return variable;
}

Slot* AirGenerator::VisitNode(MemberAccessNode* node, ThingOfCode* code)
{
}

Slot* AirGenerator::VisitNode(ArrayInitNode* node, ThingOfCode* code)
{
}

static void GenerateInterferenceGraph(ThingOfCode* code)
{
}

static void ColorSlots(CodegenTarget& target, ThingOfCode* code)
{
}

void GenerateAIR(CodegenTarget& target, ThingOfCode* code)
{
  Assert(!(code->airHead), "Tried to generate AIR for ThingOfCode already with generated code");
  unsigned int numParams = 0u;

  for (VariableDef* param : code->params)
  {
    param->slot = new ParameterSlot(param);
    param->slot->color = target.intParamColors[numParams++];

    for (VariableDef* member : param->type.def->members)
    {
      member->slot = new MemberSlot(param->slot, member);
    }
  }

  for (VariableDef* local : code->locals)
  {
    local->slot = new VariableSlot(local);

    for (VariableDef* member : local->type.def->members)
    {
      member->slot = new MemberSlot(local->slot, member);
    }
  }

  if (!(code->ast))
  {
    return;
  }

  AirGenerator airGenerator;
  airGenerator.DispatchNode(code->ast, code);

  // Allow the code generator to precolor the interference graph
  // TODO
  
  // Color the interference graph
  GenerateInterferenceGraph(code);
  ColorSlots(target, code);

  // TODO: print the instruction and slot listings
}
