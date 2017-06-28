/*
 * Copyright (C) 2017, Isaac Woods.
 * See LICENCE.md
 */

#include <air.hpp>
#include <climits>
#include <codegen.hpp>

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

std::string VariableSlot::AsString()
{
  return FormatString("%s(V)-%c", variable->name, (storage == Slot::Storage::REGISTER ? 'R' : 'S'));
}

ParameterSlot::ParameterSlot(VariableDef* parameter)
  :Slot()
  ,parameter(parameter)
{
}

std::string ParameterSlot::AsString()
{
  return FormatString("%s(P)-%c", parameter->name, (storage == Slot::Storage::REGISTER ? 'R' : 'S'));
}

MemberSlot::MemberSlot(Slot* parent, VariableDef* member)
  :Slot()
  ,parent(parent)
  ,member(member)
{
}

std::string MemberSlot::AsString()
{
  return FormatString("%s(M)-%c", member->name, (storage == Slot::Storage::REGISTER ? 'R' : 'S'));
}

TemporarySlot::TemporarySlot(unsigned int tag)
  :Slot()
  ,tag(tag)
{
}

std::string TemporarySlot::AsString()
{
  return FormatString("t%u", tag);
}

ReturnResultSlot::ReturnResultSlot(unsigned int tag)
  :Slot()
  ,tag(tag)
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
std::string ConstantSlot<StringConstant*>::AsString()
{
  return FormatString("\"%s\"", value->string);
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

CallInstruction::CallInstruction(ThingOfCode* thing)
  :AirInstruction()
  ,thing(thing)
{
}

std::string CallInstruction::AsString()
{
  return FormatString("%u: CALL %u", index, thing->mangledName);
}

// AirGenerator
static void PushInstruction(ThingOfCode* code, AirInstruction* instruction)
{
  printf("Pushing instruction: %s\n", instruction->AsString().c_str());
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
  if (node->next) (void)Dispatch(node->next, code);
  return nullptr;
}

Slot* AirGenerator::VisitNode(ReturnNode* node, ThingOfCode* code)
{
  Slot* returnValue = (node->returnValue ? Dispatch(node->returnValue, code) : nullptr);
  PushInstruction(code, new ReturnInstruction(returnValue));
  if (node->next) (void)Dispatch(node->next, code);
  return nullptr;
}

Slot* AirGenerator::VisitNode(UnaryOpNode* node, ThingOfCode* code)
{
  Slot* operand = Dispatch(node->operand, code);
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
      operand->Use(neg);
      result->ChangeValue(neg);
    } break;

    case UnaryOpNode::Operator::LOGICAL_NOT:
    {
      AirInstruction* notI = new UnaryOpInstruction(UnaryOpInstruction::Operation::LOGICAL_NOT, result, operand);
      PushInstruction(code, notI);
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
      PushInstruction(code, inc);
      operand->Use(inc);
      result->ChangeValue(inc);
    } break;

    case UnaryOpNode::Operator::POST_INCREMENT:
    {
      AirInstruction* mov = new MovInstruction(operand, result);
      AirInstruction* inc = new UnaryOpInstruction(UnaryOpInstruction::Operation::INCREMENT, operand, operand);

      operand->Use(mov);
      operand->Use(inc);
      result->ChangeValue(mov);
      operand->ChangeValue(inc);

      PushInstruction(code, mov);
      PushInstruction(code, inc);
    } break;

    case UnaryOpNode::Operator::PRE_DECREMENT:
    {
      AirInstruction* dec = new UnaryOpInstruction(UnaryOpInstruction::Operation::DECREMENT, result, operand);
      PushInstruction(code, dec);
      operand->Use(dec);
      result->ChangeValue(dec);
    } break;

    case UnaryOpNode::Operator::POST_DECREMENT:
    {
      AirInstruction* mov = new MovInstruction(operand, result);
      AirInstruction* dec = new UnaryOpInstruction(UnaryOpInstruction::Operation::DECREMENT, operand, operand);

      operand->Use(mov);
      operand->Use(dec);
      result->ChangeValue(mov);
      result->ChangeValue(dec);

      PushInstruction(code, mov);
      PushInstruction(code, dec);
    } break;
  }

  if (node->next) (void)Dispatch(node->next, code);
  return result;
}

Slot* AirGenerator::VisitNode(BinaryOpNode* node, ThingOfCode* code)
{
  // TODO
  if (node->next) (void)Dispatch(node->next, code);
  return nullptr;
}

Slot* AirGenerator::VisitNode(VariableNode* node, ThingOfCode* code)
{
  Assert(node->isResolved, "Tried to generate AIR for unresolved variable");

  if (node->next) (void)Dispatch(node->next, code);
  return node->var->slot;
}

Slot* AirGenerator::VisitNode(ConditionNode* node, ThingOfCode* code)
{
  Slot* a = Dispatch(node->left, code);
  Slot* b = Dispatch(node->right, code);
  CmpInstruction* cmp = new CmpInstruction(a, b);
  a->Use(cmp);
  b->Use(cmp);
  PushInstruction(code, cmp);

  if (node->next) (void)Dispatch(node->next, code);
  return nullptr;
}

Slot* AirGenerator::VisitNode(BranchNode* node, ThingOfCode* code)
{
  Dispatch(node->condition, code);

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
  Dispatch(node->thenCode, code);
  if (elseLabel)
  {
    PushInstruction(code, elseLabel);
    Dispatch(node->elseCode, code);
  }
  PushInstruction(code, endLabel);

  if (node->next) (void)Dispatch(node->next, code);
  return nullptr;
}

Slot* AirGenerator::VisitNode(WhileNode* node, ThingOfCode* code)
{
  LabelInstruction* label = new LabelInstruction();
  PushInstruction(code, label);

  Dispatch(node->loopBody, code);

  Dispatch(node->condition, code);
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

  if (node->next) (void)Dispatch(node->next, code);
  return nullptr;
}

Slot* AirGenerator::VisitNode(NumberNode<unsigned int>* node, ThingOfCode* code)
{
  Slot* slot = new ConstantSlot<unsigned int>(node->value);
  code->slots.push_back(slot);

  if (node->next) (void)Dispatch(node->next, code);
  return slot;
}

Slot* AirGenerator::VisitNode(NumberNode<int>* node, ThingOfCode* code)
{
  Slot* slot = new ConstantSlot<int>(node->value);
  code->slots.push_back(slot);

  if (node->next) (void)Dispatch(node->next, code);
  return slot;
}

Slot* AirGenerator::VisitNode(NumberNode<float>* node, ThingOfCode* code)
{
  Slot* slot = new ConstantSlot<float>(node->value);
  code->slots.push_back(slot);

  if (node->next) (void)Dispatch(node->next, code);
  return slot;
}

Slot* AirGenerator::VisitNode(StringNode* node, ThingOfCode* code)
{
  Slot* slot = new ConstantSlot<StringConstant*>(node->string);
  code->slots.push_back(slot);

  if (node->next) (void)Dispatch(node->next, code);
  return slot;
}

Slot* AirGenerator::VisitNode(CallNode* node, ThingOfCode* code)
{
  // TODO: parameterise the parameters into the correct places, issue the call instruction, and return the
  // result in a ReturnResultSlot correctly

  if (node->next) (void)Dispatch(node->next, code);
  return nullptr;
}

Slot* AirGenerator::VisitNode(VariableAssignmentNode* node, ThingOfCode* code)
{
  Slot* variable = Dispatch(node->variable, code);
  Slot* newValue = Dispatch(node->newValue, code);
  AirInstruction* mov = new MovInstruction(newValue, variable);
  variable->ChangeValue(mov);
  newValue->Use(mov);

  PushInstruction(code, mov);

  if (node->next) (void)Dispatch(node->next, code);
  return variable;
}

Slot* AirGenerator::VisitNode(MemberAccessNode* node, ThingOfCode* code)
{
  // TODO
  if (node->next) (void)Dispatch(node->next, code);
  return nullptr;
}

Slot* AirGenerator::VisitNode(ArrayInitNode* node, ThingOfCode* code)
{
  // TODO
  if (node->next) (void)Dispatch(node->next, code);
  return nullptr;
}

static bool DoRangesIntersect(LiveRange& a, LiveRange& b)
{
  unsigned int definitionA = (a.definition ? a.definition->index : 0u);
  unsigned int definitionB = (b.definition ? b.definition->index : 0u);
  // TODO: should this default to their definitions instead of UINT_MAX??
  // (Because we don't care what happens to unused slots?)
  // Maybe remove unused slots before creating the interference graph?
  unsigned int useA = (a.lastUse ? a.lastUse->index : UINT_MAX);
  unsigned int useB = (b.lastUse ? b.lastUse->index : UINT_MAX);

  return ((definitionA <= useB) && (definitionB <= useA));
}

static void GenerateInterferenceGraph(ThingOfCode* code)
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
static void ColorSlots(CodegenTarget& target, ThingOfCode* code)
{
  for (Slot* slot : code->slots)
  {
    // Skip slots that are uncolorable or already colored
    if ((slot->color != -1) || !slot->ShouldColor())
    {
      continue;
    }

    // Find colors already used by interfering slots
    bool usedColors[target.numGeneralRegisters] = {};
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
      if (!usedColors[i])
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

void AirGenerator::Apply(ParseResult& parse)
{
  for (ThingOfCode* code : parse.codeThings)
  {
    if (code->attribs.isPrototype)
    {
      continue;
    }

    Assert(!(code->airHead), "Tried to generate AIR for ThingOfCode already with generated code");
    unsigned int numParams = 0u;

    // Generate slots for the parameters
    for (VariableDef* param : code->params)
    {
      param->slot = new ParameterSlot(param);
      param->slot->color = target.intParamColors[numParams++];

      for (VariableDef* member : param->type.resolvedType->members)
      {
        member->slot = new MemberSlot(param->slot, member);
      }
    }

    // Generate slots for the locals
    for (VariableDef* local : code->locals)
    {
      local->slot = new VariableSlot(local);
      Assert(local->type.isResolved, "Tried to generate AIR without type information");

      for (VariableDef* member : local->type.resolvedType->members)
      {
        member->slot = new MemberSlot(local->slot, member);
      }
    }

    if (!(code->ast))
    {
      return;
    }

    Dispatch(code->ast, code);

    // Allow the code generator to precolor the interference graph
    InstructionPrecolorer precolorer;
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
    printf("Instruction listing for %s:\n", code->mangledName);
    for (AirInstruction* instruction = code->airHead;
         instruction;
         instruction = instruction->next)
    {
      printf("%s\n", instruction->AsString().c_str());
    }

    printf("Slots for %s:\n", code->mangledName);
    for (Slot* slot : code->slots)
    {
      printf("%s\n", slot->AsString().c_str());
    }
#endif

#ifdef OUTPUT_DOT
    // TODO: Output interference graph as a DOT
#endif
  }
}

bool IsColorInUseAtPoint(ThingOfCode* code, AirInstruction* instruction, signed int color)
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

      unsigned int definitionIndex = (range.definition ? range.definition->index : 0u);
      if ((instruction->index >= definitionIndex) && (instruction->index <= range.lastUse->index))
      {
        return true;
      }
    }
  }

  return false;
}
