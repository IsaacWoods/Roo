/*
 * Copyright (C) 2017, Isaac Woods.
 * See LICENCE.md
 */

#include <air.hpp>
#include <climits>
#include <codegen.hpp>
#include <x64/precolorer.hpp>

static void UseSlot(Slot* slot, AirInstruction* instruction)
{
  Assert(instruction->index != -1, "Instruction must have been pushed");

  if (slot->liveRanges.size() >= 1u)
  {
    LiveRange& lastRange = slot->liveRanges.back();

    if (lastRange.definition)
    {
      Assert(lastRange.definition->index < instruction->index, FormatString("Slot used before being defined: %s", slot->AsString().c_str()).c_str());
      lastRange.lastUse = instruction;
    }
  }
  else
  {
    RaiseError(ERROR_BIND_USED_BEFORE_INIT, slot->AsString().c_str());
  }
}

void VariableSlot     ::Use(AirInstruction* instruction) { UseSlot(this, instruction); }
void ParameterSlot    ::Use(AirInstruction* instruction) { UseSlot(this, instruction); }
void MemberSlot       ::Use(AirInstruction* instruction) { UseSlot(this, instruction); }
void TemporarySlot    ::Use(AirInstruction* instruction) { UseSlot(this, instruction); }
void ReturnResultSlot ::Use(AirInstruction* instruction) { UseSlot(this, instruction); }

static void ChangeSlotValue(Slot* slot, AirInstruction* instruction)
{
  Assert(instruction->index != -1, "Instruction must have been pushed");
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

Slot::Slot(CodeThing* code)
  :color(-1)
  ,interferences()
  ,liveRanges()
#ifdef OUTPUT_DOT
  ,dotTag(0u)
#endif
{
  code->slots.push_back(this);
}

VariableSlot::VariableSlot(CodeThing* code, VariableDef* variable)
  :Slot(code)
  ,variable(variable)
{
}

std::string VariableSlot::AsString()
{
  return FormatString("%s(V)-%c", variable->name.c_str(), variable->GetStorageChar());
}

ParameterSlot::ParameterSlot(CodeThing* code, VariableDef* parameter)
  :Slot(code)
  ,parameter(parameter)
{
}

std::string ParameterSlot::AsString()
{
  return FormatString("%s(P)-%c", parameter->name.c_str(), parameter->GetStorageChar());
}

MemberSlot::MemberSlot(CodeThing* code, Slot* parent, VariableDef* member)
  :Slot(code)
  ,parent(parent)
  ,member(member)
{
}

std::string MemberSlot::AsString()
{
  return FormatString("%s(M)(%d)-%c", member->name.c_str(), member->offset, member->GetStorageChar());
}

int MemberSlot::GetBasePointerOffset()
{
  int parentOffset;
  switch (parent->GetType())
  {
    case SlotType::VARIABLE:
    {
      parentOffset = dynamic_cast<VariableSlot*>(parent)->variable->offset;
    } break;

    case SlotType::PARAMETER:
    {
      parentOffset = dynamic_cast<ParameterSlot*>(parent)->parameter->offset;
    } break;

    case SlotType::MEMBER:
    {
      parentOffset = dynamic_cast<MemberSlot*>(parent)->GetBasePointerOffset();
    } break;

    default:
    {
      RaiseError(ICE_UNHANDLED_SLOT_TYPE, parent->AsString().c_str(), "MemberSlot::GetBasePointerOffset");
      __builtin_unreachable();
    } break;
  }

  /*
   * Some things store positive offsets from stuff, but the stack grows downwards so we want it to be negative
   * regardless.
   */
  parentOffset = -abs(parentOffset);

  /*
   * We then *add* the offset into the structure.
   */
  return parentOffset + member->offset;
}

TemporarySlot::TemporarySlot(CodeThing* code)
  :Slot(code)
  ,tag(code->numTemporaries++)
{
}

std::string TemporarySlot::AsString()
{
  return FormatString("t%u", tag);
}

ReturnResultSlot::ReturnResultSlot(CodeThing* code)
  :Slot(code)
  ,tag(code->numReturnResults++)
{
}

std::string ReturnResultSlot::AsString()
{
  return FormatString("r%u", tag);
}

template<>
std::string ConstantSlot<unsigned int>::AsString()
{
  return FormatString("#%uu", value);
}

template<>
std::string ConstantSlot<int>::AsString()
{
  return FormatString("#%d", value);
}

template<>
std::string ConstantSlot<float>::AsString()
{
  return FormatString("#%f", value);
}

template<>
std::string ConstantSlot<bool>::AsString()
{
  return FormatString("#%s", (value ? "TRUE" : "FALSE"));
}

template<>
std::string ConstantSlot<StringConstant*>::AsString()
{
  return FormatString("\"%s\"", value->str.c_str());
}

AirInstruction::AirInstruction()
  :index(-1)
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

std::string LabelInstruction::AsString()
{
  // TODO: can we be more helpful here?
  return FormatString("%u: LABEL", index);
}

ReturnInstruction::ReturnInstruction(Slot* returnValue)
  :AirInstruction()
  ,returnValue(returnValue)
{
}

std::string ReturnInstruction::AsString()
{
  return FormatString("%u: RETURN %s", index, (returnValue ? returnValue->AsString().c_str() : ""));
}

JumpInstruction::JumpInstruction(JumpInstruction::Condition condition, LabelInstruction* label)
  :AirInstruction()
  ,condition(condition)
  ,label(label)
{
}

std::string JumpInstruction::AsString()
{
  switch (condition)
  {
    case JumpInstruction::Condition::UNCONDITIONAL:         return FormatString("%u: JMP L(%u)", index, label->index);  
    case JumpInstruction::Condition::IF_EQUAL:              return FormatString("%u: JE L(%u)",  index, label->index);
    case JumpInstruction::Condition::IF_NOT_EQUAL:          return FormatString("%u: JNE L(%u)", index, label->index);
    case JumpInstruction::Condition::IF_OVERFLOW:           return FormatString("%u: JO L(%u)",  index, label->index);
    case JumpInstruction::Condition::IF_NOT_OVERFLOW:       return FormatString("%u: JNO L(%u)", index, label->index);
    case JumpInstruction::Condition::IF_SIGN:               return FormatString("%u: JS L(%u)",  index, label->index);
    case JumpInstruction::Condition::IF_NOT_SIGN:           return FormatString("%u: JNS L(%u)", index, label->index);
    case JumpInstruction::Condition::IF_GREATER:            return FormatString("%u: JG L(%u)",  index, label->index);
    case JumpInstruction::Condition::IF_GREATER_OR_EQUAL:   return FormatString("%u: JGE L(%u)", index, label->index);
    case JumpInstruction::Condition::IF_LESSER:             return FormatString("%u: JL L(%u)",  index, label->index);
    case JumpInstruction::Condition::IF_LESSER_OR_EQUAL:    return FormatString("%u: JLE L(%u)", index, label->index);
    case JumpInstruction::Condition::IF_PARITY_EVEN:        return FormatString("%u: JPE L(%u)", index, label->index);
    case JumpInstruction::Condition::IF_PARITY_ODD:         return FormatString("%u: JPO L(%u)", index, label->index);
  }

  __builtin_unreachable();
}

MovInstruction::MovInstruction(Slot* src, Slot* dest)
  :AirInstruction()
  ,src(src)
  ,dest(dest)
{
}

std::string MovInstruction::AsString()
{
  return FormatString("%u: MOV %s, %s", index, dest->AsString().c_str(), src->AsString().c_str());
}

CmpInstruction::CmpInstruction(Slot* a, Slot* b)
  :AirInstruction()
  ,a(a)
  ,b(b)
{
}

std::string CmpInstruction::AsString()
{
  return FormatString("%u: CMP %s, %s", index, a->AsString().c_str(), b->AsString().c_str());
}

UnaryOpInstruction::UnaryOpInstruction(UnaryOpInstruction::Operation op, Slot* result, Slot* operand)
  :AirInstruction()
  ,op(op)
  ,result(result)
  ,operand(operand)
{
}

std::string UnaryOpInstruction::AsString()
{
  switch (op)
  {
    case UnaryOpInstruction::INCREMENT:   return FormatString("%u: %s = INC %s", index, result->AsString().c_str(), operand->AsString().c_str());
    case UnaryOpInstruction::DECREMENT:   return FormatString("%u: %s = DEC %s", index, result->AsString().c_str(), operand->AsString().c_str());
    case UnaryOpInstruction::NEGATE:      return FormatString("%u: %s = NEG %s", index, result->AsString().c_str(), operand->AsString().c_str());
    case UnaryOpInstruction::LOGICAL_NOT: return FormatString("%u: %s = NOT %s", index, result->AsString().c_str(), operand->AsString().c_str());
  }

  __builtin_unreachable();
}

BinaryOpInstruction::BinaryOpInstruction(BinaryOpInstruction::Operation op, Slot* result, Slot* left, Slot* right)
  :AirInstruction()
  ,op(op)
  ,result(result)
  ,left(left)
  ,right(right)
{
}

std::string BinaryOpInstruction::AsString()
{
  switch (op)
  {
    case BinaryOpInstruction::Operation::ADD:       return FormatString("%u: %s = ADD %s, %s", index, result->AsString().c_str(), left->AsString().c_str(), right->AsString().c_str());
    case BinaryOpInstruction::Operation::SUBTRACT:  return FormatString("%u: %s = SUB %s, %s", index, result->AsString().c_str(), left->AsString().c_str(), right->AsString().c_str());
    case BinaryOpInstruction::Operation::MULTIPLY:  return FormatString("%u: %s = MUL %s, %s", index, result->AsString().c_str(), left->AsString().c_str(), right->AsString().c_str());
    case BinaryOpInstruction::Operation::DIVIDE:    return FormatString("%u: %s = DIV %s, %s", index, result->AsString().c_str(), left->AsString().c_str(), right->AsString().c_str());
  }

  __builtin_unreachable();
}

CallInstruction::CallInstruction(CodeThing* thing)
  :AirInstruction()
  ,thing(thing)
{
}

std::string CallInstruction::AsString()
{
  return FormatString("%u: CALL %s", index, thing->mangledName.c_str());
}

// AirGenerator
static JumpInstruction::Condition MapReverseCondition(ConditionNode::Condition condition)
{
  switch (condition)
  {
    case ConditionNode::Condition::EQUAL:                   return JumpInstruction::Condition::IF_NOT_EQUAL;
    case ConditionNode::Condition::NOT_EQUAL:               return JumpInstruction::Condition::IF_EQUAL;
    case ConditionNode::Condition::LESS_THAN:               return JumpInstruction::Condition::IF_GREATER_OR_EQUAL;
    case ConditionNode::Condition::LESS_THAN_OR_EQUAL:      return JumpInstruction::Condition::IF_GREATER;
    case ConditionNode::Condition::GREATER_THAN:            return JumpInstruction::Condition::IF_LESSER_OR_EQUAL;
    case ConditionNode::Condition::GREATER_THAN_OR_EQUAL:   return JumpInstruction::Condition::IF_LESSER;
  }

  __builtin_unreachable();
}

static void PushInstruction(CodeThing* code, AirInstruction* instruction)
{
  Assert(instruction->index == -1, "Instruction has already been pushed");

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

Slot* AirGenerator::VisitNode(BreakNode* node, AirState* state)
{
  Assert(state->breakLabel, "No valid break label to jump to from BreakNode");
  PushInstruction(state->code, new JumpInstruction(JumpInstruction::Condition::UNCONDITIONAL, state->breakLabel));
  
  if (node->next) (void)Dispatch(node->next, state);
  return nullptr;
}

Slot* AirGenerator::VisitNode(ReturnNode* node, AirState* state)
{
  Slot* returnValue = (node->returnValue ? Dispatch(node->returnValue, state) : nullptr);
  ReturnInstruction* ret = new ReturnInstruction(returnValue);
  PushInstruction(state->code, ret);

  if (node->next) (void)Dispatch(node->next, state);
  return nullptr;
}

Slot* AirGenerator::VisitNode(UnaryOpNode* node, AirState* state)
{
  Slot* operand = Dispatch(node->operand, state);
  Slot* result = new TemporarySlot(state->code);

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
      PushInstruction(state->code, neg);
      operand->Use(neg);
      result->ChangeValue(neg);
    } break;

    case UnaryOpNode::Operator::LOGICAL_NOT:
    {
      AirInstruction* notI = new UnaryOpInstruction(UnaryOpInstruction::Operation::LOGICAL_NOT, result, operand);
      PushInstruction(state->code, notI);
      operand->Use(notI);
      result->ChangeValue(notI);
    } break;

    case UnaryOpNode::Operator::TAKE_REFERENCE:
    {
      // TODO
    } break;

    case UnaryOpNode::Operator::PRE_INCREMENT:
    {
      AirInstruction* inc = new UnaryOpInstruction(UnaryOpInstruction::Operation::INCREMENT, result, operand);
      PushInstruction(state->code, inc);
      operand->Use(inc);
      result->ChangeValue(inc);
    } break;

    case UnaryOpNode::Operator::POST_INCREMENT:
    {
      AirInstruction* mov = new MovInstruction(operand, result);
      AirInstruction* inc = new UnaryOpInstruction(UnaryOpInstruction::Operation::INCREMENT, operand, operand);

      PushInstruction(state->code, mov);
      PushInstruction(state->code, inc);

      operand->Use(mov);
      operand->Use(inc);
      result->ChangeValue(mov);
      operand->ChangeValue(inc);
    } break;

    case UnaryOpNode::Operator::PRE_DECREMENT:
    {
      AirInstruction* dec = new UnaryOpInstruction(UnaryOpInstruction::Operation::DECREMENT, result, operand);
      PushInstruction(state->code, dec);
      operand->Use(dec);
      result->ChangeValue(dec);
    } break;

    case UnaryOpNode::Operator::POST_DECREMENT:
    {
      AirInstruction* mov = new MovInstruction(operand, result);
      AirInstruction* dec = new UnaryOpInstruction(UnaryOpInstruction::Operation::DECREMENT, operand, operand);

      PushInstruction(state->code, mov);
      PushInstruction(state->code, dec);

      operand->Use(mov);
      operand->Use(dec);
      result->ChangeValue(mov);
      result->ChangeValue(dec);
    } break;
  }

  if (node->next) (void)Dispatch(node->next, state);
  return result;
}

Slot* AirGenerator::VisitNode(BinaryOpNode* node, AirState* state)
{
  // TODO
  if (node->next) (void)Dispatch(node->next, state);
  return nullptr;
}

Slot* AirGenerator::VisitNode(VariableNode* node, AirState* state)
{
  Assert(node->isResolved, "Tried to generate AIR for unresolved variable");

  if (node->next) (void)Dispatch(node->next, state);
  return node->var->slot;
}

Slot* AirGenerator::VisitNode(ConditionNode* node, AirState* state)
{
  Slot* a = Dispatch(node->left, state);
  Slot* b = Dispatch(node->right, state);

  CmpInstruction* cmp = new CmpInstruction(a, b);
  PushInstruction(state->code, cmp);

  a->Use(cmp);
  b->Use(cmp);

  if (node->next) (void)Dispatch(node->next, state);
  return nullptr;
}

Slot* AirGenerator::VisitNode(BranchNode* node, AirState* state)
{
  Dispatch(node->condition, state);

  LabelInstruction* elseLabel = (node->elseCode ? new LabelInstruction() : nullptr);
  LabelInstruction* endLabel = new LabelInstruction();

  /*
   * We reverse the jump condition here, because we want to jump (and skip the 'then' branch) if the
   * condition is *not* true.
   */
  Assert(IsNodeOfType<ConditionNode>(node->condition), "Complete AST must have `ConditionNode`s as conditions");
  JumpInstruction::Condition condition = MapReverseCondition(dynamic_cast<ConditionNode*>(node->condition)->condition);
  PushInstruction(state->code, new JumpInstruction(condition, (elseLabel ? elseLabel : endLabel)));
  Dispatch(node->thenCode, state);

  if (elseLabel)
  {
    /*
     * We only need to jump to endLabel after the then-branch if there is an else branch, otherwise we're already
     * there.
     */
    PushInstruction(state->code, new JumpInstruction(JumpInstruction::Condition::UNCONDITIONAL, endLabel));

    PushInstruction(state->code, elseLabel);
    Dispatch(node->elseCode, state);
  }

  PushInstruction(state->code, endLabel);

  if (node->next) (void)Dispatch(node->next, state);
  return nullptr;
}

Slot* AirGenerator::VisitNode(WhileNode* node, AirState* state)
{
  state->breakLabel = new LabelInstruction();

  LabelInstruction* startLabel = new LabelInstruction();
  PushInstruction(state->code, startLabel);

  Dispatch(node->condition, state);
  Assert(IsNodeOfType<ConditionNode>(node->condition), "Complete AST must have `ConditionNode`s as conditions");

  JumpInstruction::Condition condition = MapReverseCondition(dynamic_cast<ConditionNode*>(node->condition)->condition);
  PushInstruction(state->code, new JumpInstruction(condition, state->breakLabel));
  Dispatch(node->loopBody, state);
  PushInstruction(state->code, new JumpInstruction(JumpInstruction::Condition::UNCONDITIONAL, startLabel));
  PushInstruction(state->code, state->breakLabel);
  state->breakLabel = nullptr;

  if (node->next) (void)Dispatch(node->next, state);
  return nullptr;
}

Slot* AirGenerator::VisitNode(ConstantNode<unsigned int>* node, AirState* state)
{
  Slot* slot = new ConstantSlot<unsigned int>(state->code, node->value);

  if (node->next) (void)Dispatch(node->next, state);
  return slot;
}

Slot* AirGenerator::VisitNode(ConstantNode<int>* node, AirState* state)
{
  Slot* slot = new ConstantSlot<int>(state->code, node->value);

  if (node->next) (void)Dispatch(node->next, state);
  return slot;
}

Slot* AirGenerator::VisitNode(ConstantNode<float>* node, AirState* state)
{
  Slot* slot = new ConstantSlot<float>(state->code, node->value);

  if (node->next) (void)Dispatch(node->next, state);
  return slot;
}

Slot* AirGenerator::VisitNode(ConstantNode<bool>* node, AirState* state)
{
  Slot* slot = new ConstantSlot<bool>(state->code, node->value);

  if (node->next) (void)Dispatch(node->next, state);
  return slot;
}

Slot* AirGenerator::VisitNode(StringNode* node, AirState* state)
{
  Slot* slot = new ConstantSlot<StringConstant*>(state->code, node->string);

  if (node->next) (void)Dispatch(node->next, state);
  return slot;
}

Slot* AirGenerator::VisitNode(CallNode* node, AirState* state)
{
  // TODO: parameterise the parameters into the correct places, issue the call instruction, and return the
  // result in a ReturnResultSlot correctly
  std::vector<Slot*> paramSlots;
  unsigned int numGeneralParams = 0u;

  for (ASTNode* paramNode : node->params)
  {
    Assert(numGeneralParams < target.numGeneralRegisters, "Filled up general registers");
    Slot* slot = Dispatch(paramNode, state);

    switch (slot->GetType())
    {
      case SlotType::VARIABLE:
      case SlotType::PARAMETER:
      case SlotType::MEMBER:
      case SlotType::TEMPORARY:
      {
        slot->color = target.intParamColors[numGeneralParams++];
        paramSlots.push_back(slot);
      } break;

      case SlotType::RETURN_RESULT:
      case SlotType::INT_CONSTANT:
      case SlotType::UNSIGNED_INT_CONSTANT:
      case SlotType::FLOAT_CONSTANT:
      case SlotType::BOOL_CONSTANT:
      case SlotType::STRING_CONSTANT:
      {
        TemporarySlot* tempSlot = new TemporarySlot(state->code);
        tempSlot->color = target.intParamColors[numGeneralParams++];
        AirInstruction* mov = new MovInstruction(slot, tempSlot);
        PushInstruction(state->code, mov);
        paramSlots.push_back(tempSlot);

        slot->Use(mov);
        tempSlot->ChangeValue(mov);
      } break;
    }
  }

  Assert(node->isResolved, "Tried to emit call to unresolved function");
  AirInstruction* call = new CallInstruction(node->resolvedFunction);
  PushInstruction(state->code, call);

  for (Slot* paramSlot : paramSlots)
  {
    paramSlot->Use(call);
  }

  Slot* returnSlot = nullptr;
  if (node->resolvedFunction->returnType)
  {
    returnSlot = new ReturnResultSlot(state->code);
    returnSlot->ChangeValue(call);
    returnSlot->color = target.functionReturnColor;
  }

  if (node->next) (void)Dispatch(node->next, state);
  return returnSlot;
}

Slot* AirGenerator::VisitNode(VariableAssignmentNode* node, AirState* state)
{
  Slot* variable = Dispatch(node->variable, state);
  Slot* newValue = Dispatch(node->newValue, state);

  AirInstruction* mov = new MovInstruction(newValue, variable);
  PushInstruction(state->code, mov);

  variable->ChangeValue(mov);
  newValue->Use(mov);

  if (node->next) (void)Dispatch(node->next, state);
  return variable;
}

Slot* AirGenerator::VisitNode(MemberAccessNode* node, AirState* state)
{
  /*
   * We always generate a temporary that should be registerized, because we can probably eliminate unnecessary
   * register transfers, but we can't be sure that the target architecture supports an indirect memory address
   * wherever this slot is going to be used (e.g. as the destination of a MOV it's fine, but in a comparison
   * instruction it may not be).
   */
  Assert(node->isResolved, "Tried to generate AIR for unresolved member access");
  Slot* tempSlot = new TemporarySlot(state->code);

  AirInstruction* mov = new MovInstruction(node->member->slot, tempSlot);
  PushInstruction(state->code, mov);

  node->member->slot->Use(mov);
  tempSlot->ChangeValue(mov);

  if (node->next) (void)Dispatch(node->next, state);
  return tempSlot;
}

Slot* AirGenerator::VisitNode(ArrayInitNode* node, AirState* state)
{
  // TODO
  if (node->next) (void)Dispatch(node->next, state);
  return nullptr;
}

Slot* AirGenerator::VisitNode(InfiniteLoopNode* node, AirState* state)
{
  state->breakLabel = new LabelInstruction();

  LabelInstruction* startLabel = new LabelInstruction();
  PushInstruction(state->code, startLabel);
  Dispatch(node->loopBody, state);
  PushInstruction(state->code, new JumpInstruction(JumpInstruction::Condition::UNCONDITIONAL, startLabel));
  PushInstruction(state->code, state->breakLabel);
  state->breakLabel = nullptr;

  if (node->next) (void)Dispatch(node->next, state);
  return nullptr;
}

Slot* AirGenerator::VisitNode(ConstructNode* node, AirState* state)
{
  auto itemIt = node->items.begin();
  std::vector<VariableDef*>* members = nullptr;

  // TODO: use enum system when done
  if (IsNodeOfType<VariableNode>(node->variable))
  {
    members= &(dynamic_cast<VariableNode*>(node->variable)->var->members);
  }
  else if (IsNodeOfType<MemberAccessNode>(node->variable))
  {
    MemberAccessNode* memberAccess = dynamic_cast<MemberAccessNode*>(node->variable);
    Assert(memberAccess->isResolved, "Tried to generate AIR from unresolved member access");
    members = &(memberAccess->member->members);
  }
  else
  {
    Crash();
    __builtin_unreachable();
  }

  for (auto memberIt = members->begin();
       itemIt != node->items.end() && memberIt != members->end();
       itemIt++, memberIt++)
  {
    Slot* itemSlot = Dispatch(*itemIt, state);
    Slot* memberSlot = (*memberIt)->slot;

    AirInstruction* mov = new MovInstruction(itemSlot, memberSlot);
    PushInstruction(state->code, mov);
    memberSlot->ChangeValue(mov);
    itemSlot->Use(mov);
  }

  if (node->next) (void)Dispatch(node->next, state);
  return nullptr;
}

static bool DoRangesIntersect(LiveRange& a, LiveRange& b)
{
  unsigned int definitionA = (a.definition ? a.definition->index : 0u);
  unsigned int definitionB = (b.definition ? b.definition->index : 0u);

  /*
   * If we don't use the variable, it doesn't interfere with anything, so we set it's last use to it's definition
   */
  unsigned int useA = (a.lastUse ? a.lastUse->index : a.definition->index);
  unsigned int useB = (b.lastUse ? b.lastUse->index : b.definition->index);

  return ((definitionA <= useB) && (definitionB <= useA));
}

static void GenerateInterferenceGraph(CodeThing* code)
{
  /*
   * We have to use iterators here, because we need to only visit each *pair* of slots once.
   */
  for (auto itA = code->slots.begin();
       itA != code->slots.end();
       itA++)
  {
    Slot* a = *itA;

    if (a->IsConstant())
    {
      continue;
    }

    for (auto itB = std::next(itA);
         itB != code->slots.end();
         itB++)
    {
      Slot* b = *itB;

      for (LiveRange& rangeA : a->liveRanges)
      {
        for (LiveRange& rangeB : b->liveRanges)
        {
          if (DoRangesIntersect(rangeA, rangeB))
          {
            a->interferences.push_back(b);
            b->interferences.push_back(a);
            goto FoundInterference;
          }
        }
      }

FoundInterference:
      continue;
    }
  }
}

/*
 * This assigns a color to each slot (each color then corresponds to a register) so that no edge of the
 * interference graph is between nodes of the same color.
 *
 * XXX: Atm, this is very naive and could do with a lot more improvement to achieve a more efficient k-coloring,
 * especially concerning the register pressure (or lack of it).
 */
static void ColorSlots(TargetMachine& target, CodeThing* code)
{
  for (Slot* slot : code->slots)
  {
    // Skip slots that are uncolorable or already colored
    if ((slot->color != -1) || !slot->ShouldColor())
    {
      continue;
    }

    // Find colors already used by interfering slots
    bool usedColors[target.numGeneralRegisters];
    memset(usedColors, false, sizeof(bool)*target.numGeneralRegisters);
    for (Slot* interferingSlot : slot->interferences)
    {
      if (interferingSlot->color != -1)
      {
        usedColors[interferingSlot->color] = true;
      }
    }

    // Choose a free color
    for (unsigned int i = 0u;
         i < target.numGeneralRegisters;
         i++)
    {
      /*
       * Some registers may be reserved for special purposes - we should not use these for general stuff
       */
      if (!usedColors[i] && target.registerSet[i].usage == RegisterDef::Usage::GENERAL)
      {
        slot->color = static_cast<signed int>(i);
        break;
      }
    }

    // Deal with it if we can't find a free color for this slot
    if (slot->color == -1)
    {
      // TODO: we should do something more helpful here, like spill something or rewrite the AIR
      RaiseError(code->errorState, ICE_GENERIC, "Failed to find a valid k-coloring of the interference graph!");
    }
  }
}

#ifdef OUTPUT_DOT
static void EmitInterferenceGraphDOT(CodeThing* code)
{
  if (!(code->airHead))
  {
    return;
  }

  std::string fileName = std::string(code->mangledName) + "_interference.dot";
  FILE* f = fopen(fileName.c_str(), "w");

  if (!f)
  {
    RaiseError(ERROR_FAILED_TO_OPEN_FILE, fileName.c_str());
  }

  fprintf(f, "digraph G\n{\n");
  unsigned int i = 0u;

  const char* snazzyColors[] =
  {
    "cyan2",      "deeppink",   "darkgoldenrod2",     "mediumpurple2",  "slategray",    "chartreuse3",
    "green3",     "lightblue2", "mediumspringgreeen", "orange1",        "mistyrose3",   "maroon2",
    "steelblue2", "blue",       "lightseagreen",      "plum",           "dodgerblue4",  "darkorchid1",
  };

  // First emit a colored node for each slot
  for (Slot* slot : code->slots)
  {
    if (slot->IsConstant())
    {
      continue;
    }

    const char* color = "black";

    if (slot->color < 0)
    {
      fprintf(stderr, "WARNING: Found uncolored slot! Presenting as red on interference graph!\n");
      color = "red";
    }
    else
    {
      color = snazzyColors[slot->color];
    }

    fprintf(f, "\t%u[label=\"%s\" color=\"%s\" fontcolor=\"%s\"];\n", i, slot->AsString().c_str(), color, color);
    slot->dotTag = i;
    i++;
  }

  // Then emit the interferences between slots
  for (Slot* slot : code->slots)
  {
    for (Slot* interference : slot->interferences)
    {
      // NOTE(Isaac): This stops us from emitting duplicate lines, as this will only be true one way around
      fprintf(f, "\t%u -> %u[dir=none];\n", slot->dotTag, interference->dotTag);
    }
  }

  fprintf(f, "}\n");
  fclose(f);
}
#endif

void AirGenerator::Apply(ParseResult& parse)
{
  for (CodeThing* code : parse.codeThings)
  {
    if (code->attribs.isPrototype)
    {
      continue;
    }

    Assert(!(code->airHead), "Tried to generate AIR for CodeThing already with generated code");
    unsigned int numParams = 0u;

    // Generate slots for the parameters
    for (VariableDef* param : code->params)
    {
      param->slot = new ParameterSlot(code, param);
      param->slot->color = target.intParamColors[numParams++];

      for (VariableDef* member : param->members)
      {
        member->slot = new MemberSlot(code, param->slot, member);
      }
    }

    // Generate slots for the locals
    for (ScopeDef* scope : code->scopes)
    {
      for (VariableDef* local : scope->locals)
      {
        local->slot = new VariableSlot(code, local);
        Assert(local->type.isResolved, "Tried to generate AIR without type information");

        for (VariableDef* member : local->members)
        {
          member->slot = new MemberSlot(code, local->slot, member);
        }
      }
    }

    if (!(code->ast))
    {
      return;
    }

    // Generate AIR from the AST
    AirState state(code);
    Dispatch(code->ast, &state);

    // Allow the code generator to precolor the interference graph
    InstructionPrecolorer_x64 precolorer;
    for (AirInstruction* instruction = code->airHead;
         instruction;
         instruction = instruction->next)
    {
      precolorer.Dispatch(instruction);
    }
    
    // Color the interference graph
    GenerateInterferenceGraph(code);
    ColorSlots(target, code);

    // Print an AIR instruction listing and a slot listing
#if 1
    printf("\nInstruction listing for %s:\n", code->mangledName.c_str());
    for (AirInstruction* instruction = code->airHead;
         instruction;
         instruction = instruction->next)
    {
      printf("%s\n", instruction->AsString().c_str());
    }

    printf("\nSlots for %s:\n", code->mangledName.c_str());
    for (Slot* slot : code->slots)
    {
      printf("%s\n", slot->AsString().c_str());
    }
#endif

#ifdef OUTPUT_DOT
    EmitInterferenceGraphDOT(code);
#endif
  }
}

bool IsColorInUseAtPoint(CodeThing* code, AirInstruction* instruction, signed int color)
{
  for (Slot* slot : code->slots)
  {
    if (slot->IsConstant())
    {
      continue;
    }

    if (slot->color != color)
    {
      continue;
    }

    for (LiveRange& range : slot->liveRanges)
    {
      /*
       * If the slot is never used, it doesn't matter if it's overwritten, so skip it
       */
      if (!(range.lastUse))
      {
        continue;
      }

      signed int definitionIndex = (range.definition ? range.definition->index : 0u);
      if ((instruction->index >= definitionIndex) && (instruction->index <= range.lastUse->index))
      {
        return true;
      }
    }
  }

  return false;
}
