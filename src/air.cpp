/*
 * Copyright (C) 2016, Isaac Woods. All rights reserved.
 */

#include <air.hpp>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cassert>
#include <cstdarg>
#include <climits>
#include <common.hpp>
#include <ast.hpp>
#include <ir.hpp>

template<>
void Free<air_instruction*>(air_instruction*& instruction)
{
  free(instruction);
}

instruction_label* CreateInstructionLabel()
{
  instruction_label* label = static_cast<instruction_label*>(malloc(sizeof(instruction_label)));
  label->offset = 0u;
  
  return label;
}

static air_instruction* PushInstruction(thing_of_code& code, instruction_type type, ...)
{
  va_list args;
  va_start(args, type);

  air_instruction* i = static_cast<air_instruction*>(malloc(sizeof(air_instruction)));
  i->type = type;
  i->next = nullptr;

  switch (type)
  {
    case I_RETURN:
    {
      i->slot                 = va_arg(args, slot_def*);
    } break;

    case I_JUMP:
    {
      i->jump.cond            = static_cast<jump_i::condition>(va_arg(args, int));
      i->jump.label           = va_arg(args, instruction_label*);
    } break;

    case I_MOV:
    {
      i->mov.dest             = va_arg(args, slot_def*);
      i->mov.src              = va_arg(args, slot_def*);
    } break;

    case I_CMP:
    {
      i->slotTriple.left      = va_arg(args, slot_def*);
      i->slotTriple.right     = va_arg(args, slot_def*);
      i->slotTriple.result    = va_arg(args, slot_def*);
    } break;

    case I_BINARY_OP:
    {
      i->binaryOp.operation   = static_cast<binary_op_i::op>(va_arg(args, int));
      i->binaryOp.left        = va_arg(args, slot_def*);
      i->binaryOp.right       = va_arg(args, slot_def*);
      i->binaryOp.result      = va_arg(args, slot_def*);
    } break;

    case I_INC:
    case I_DEC:
    {
      i->slot                 = va_arg(args, slot_def*);
    } break;

    case I_CALL:
    {
      i->call                 = va_arg(args, thing_of_code*);
    } break;

    case I_LABEL:
    {
      i->label                = va_arg(args, instruction_label*);
    } break;

    case I_NUM_INSTRUCTIONS:
    {
      fprintf(stderr, "Tried to push AIR instruction of type I_NUM_INSTRUCTIONS!\n");
      assert(false);
    }
  }

  if (code.airHead)
  {
    i->index = code.airTail->index + 1u;
    code.airTail->next = i;
    code.airTail = i;
  }
  else
  {
    i->index = 0u;
    code.airHead = i;
    code.airTail = i;
  }

  va_end(args);
  return i;
}

static void UseSlot(slot_def* slot, air_instruction* user)
{
  if (slot->type == slot_type::INT_CONSTANT    ||
      slot->type == slot_type::FLOAT_CONSTANT  ||
      slot->type == slot_type::STRING_CONSTANT   )
  {
    return;
  }

  if (slot->liveRanges.size >= 1u)
  {
    live_range& lastRange = slot->liveRanges[slot->liveRanges.size - 1u];
    assert(lastRange.definition->index < user->index);
    lastRange.lastUse = user;
  }
  else
  {
    char* slotString = SlotAsString(slot);
    fprintf(stderr, "FATAL: Tried to use slot before defining it (slot=%s)\n", slotString);
    free(slotString);
    exit(1);
  }
}

static void ChangeSlotValue(slot_def* slot, air_instruction* changer)
{
  if (slot->type == slot_type::INT_CONSTANT    ||
      slot->type == slot_type::FLOAT_CONSTANT  ||
      slot->type == slot_type::STRING_CONSTANT   )
  {
    return;
  }

  live_range range;
  range.definition = changer;
  range.lastUse = nullptr;

  Add<live_range>(slot->liveRanges, range);
}

static slot_def* GenCall(codegen_target& target, thing_of_code& code, node* n);

/*
 * BREAK_NODE:              `
 * RETURN_NODE:             `Nothing
 * BINARY_OP_NODE:          `slot_def*  `Nothing
 * PREFIX_OP_NODE:          `slot_def*
 * VARIABLE_NAME:           `slot_def*
 * CONDITION_NODE:          `jump_instruction::condition
 * IF_NODE:                 `Nothing
 * NUMBER_NODE:             `slot_def*
 * STRING_CONSTANT_NODE:    `
 * FUNCTION_CALL_NODE:      `slot_def*  `Nothing
 * VARIABLE_ASSIGN_NODE:    `Nothing
 */
template<typename T = void>
T GenNodeAIR(codegen_target& target, thing_of_code& code, node* n);

template<>
slot_def* GenNodeAIR<slot_def*>(codegen_target& target, thing_of_code& code, node* n)
{
  assert(n);

  switch (n->type)
  {
    case BINARY_OP_NODE:
    {
      slot_def* left = GenNodeAIR<slot_def*>(target, code, n->binaryOp.left);
      slot_def* right = GenNodeAIR<slot_def*>(target, code, n->binaryOp.right);
      slot_def* result = CreateSlot(code, slot_type::TEMPORARY);
      air_instruction* instruction;

      switch (n->binaryOp.op)
      {
        case TOKEN_PLUS:
        {
          instruction = PushInstruction(code, I_BINARY_OP, binary_op_i::op::ADD_I, left, right, result);
        } break;

        case TOKEN_MINUS:
        {
          instruction = PushInstruction(code, I_BINARY_OP, binary_op_i::op::SUB_I, left, right, result);
        } break;

        case TOKEN_ASTERIX:
        {
          instruction = PushInstruction(code, I_BINARY_OP, binary_op_i::op::MUL_I, left, right, result);
        } break;

        case TOKEN_SLASH:
        {
          instruction = PushInstruction(code, I_BINARY_OP, binary_op_i::op::DIV_I, left, right, result);
        } break;

        case TOKEN_DOUBLE_PLUS:
        {
          instruction = PushInstruction(code, I_INC, left);
        } break;

        case TOKEN_DOUBLE_MINUS:
        {
          instruction = PushInstruction(code, I_DEC, left);
        } break;

        default:
        {
          fprintf(stderr, "Unhandled AST binary op in GenNodeAIR<slot_def*>!\n");
          Crash();
        }
      }

      if (right)
      {
        UseSlot(left, instruction);
        UseSlot(right, instruction);
        ChangeSlotValue(result, instruction);
        return result;
      }
      else
      {
        // NOTE(Isaac): order of these is important, we want to update the old live-range, then create a new one
        UseSlot(left, instruction);
        ChangeSlotValue(left, instruction);
        return left;
      }
    } break;

    case PREFIX_OP_NODE:
    {
      slot_def* right = GenNodeAIR<slot_def*>(target, code, n->prefixOp.right);
      slot_def* result = CreateSlot(code, slot_type::TEMPORARY);
      air_instruction* instruction = nullptr;

      switch (n->prefixOp.op)
      {
        case TOKEN_PLUS:
        {
          // TODO
        } break;

        case TOKEN_MINUS:
        {
          // TODO
        } break;

        case TOKEN_BANG:
        {
          // TODO
        } break;

        case TOKEN_TILDE:
        {
          // TODO
        } break;

        default:
        {
          fprintf(stderr, "Unhandled AST prefix op in GenNodeAIR!\n");
          Crash();
        }
      }

      UseSlot(right, instruction);
      ChangeSlotValue(result, instruction);

      return result;
    } break;

    case CALL_NODE:
    {
      return GenCall(target, code, n);
    } break;

    case VARIABLE_NODE:
    {
      assert(n->variable.isResolved);
      variable_def* variable = n->variable.var;

      if (!(variable->slot))
      {
        variable->slot = CreateSlot(code, slot_type::VARIABLE, variable);
      }
      
      return variable->slot;
    } break;

    case NUMBER_CONSTANT_NODE:
    {
      switch (n->numberConstant.type)
      {
        case number_constant_part::constant_type::CONSTANT_TYPE_INT:
        {
          return CreateSlot(code, slot_type::INT_CONSTANT, n->numberConstant.asInt);
        } break;

        case number_constant_part::constant_type::CONSTANT_TYPE_FLOAT:
        {
          return CreateSlot(code, slot_type::FLOAT_CONSTANT, n->numberConstant.asFloat);
        } break;
      }
    };

    case STRING_CONSTANT_NODE:
    {
      return CreateSlot(code, slot_type::STRING_CONSTANT, n->stringConstant);
    } break;

    default:
    {
      fprintf(stderr, "Unhandled node for returning a `slot_def*` in GenNodeAIR: %s\n", GetNodeName(n->type));
      Crash();
    }
  }
}

template<>
jump_i::condition GenNodeAIR<jump_i::condition>(codegen_target& target, thing_of_code& code, node* n)
{
  assert(n);

  switch (n->type)
  {
    case CONDITION_NODE:
    {
      slot_def* a = GenNodeAIR<slot_def*>(target, code, n->condition.left);
      slot_def* b = GenNodeAIR<slot_def*>(target, code, n->condition.right);

      air_instruction* instruction = PushInstruction(code, I_CMP, a, b);
      UseSlot(a, instruction);
      UseSlot(a, instruction);

      switch (n->condition.condition)
      {
        case TOKEN_EQUALS_EQUALS:
        {
          return (n->condition.reverseOnJump ?
                  jump_i::condition::IF_NOT_EQUAL :
                  jump_i::condition::IF_EQUAL);
        }

        case TOKEN_BANG_EQUALS:
        {
          return (n->condition.reverseOnJump ?
                  jump_i::condition::IF_EQUAL :
                  jump_i::condition::IF_NOT_EQUAL);
        }

        case TOKEN_GREATER_THAN:
        {
          return (n->condition.reverseOnJump ?
                  jump_i::condition::IF_LESSER_OR_EQUAL :
                  jump_i::condition::IF_GREATER);
        }

        case TOKEN_GREATER_THAN_EQUAL_TO:
        {
          return (n->condition.reverseOnJump ?
                  jump_i::condition::IF_LESSER :
                  jump_i::condition::IF_GREATER_OR_EQUAL);
        }

        case TOKEN_LESS_THAN:
        {
          return (n->condition.reverseOnJump ?
                  jump_i::condition::IF_GREATER_OR_EQUAL :
                  jump_i::condition::IF_LESSER);
        }

        case TOKEN_LESS_THAN_EQUAL_TO:
        {
          return (n->condition.reverseOnJump ?
                  jump_i::condition::IF_GREATER :
                  jump_i::condition::IF_LESSER_OR_EQUAL);
        }

        default:
        {
          fprintf(stderr, "Unhandled AST conditional in GenNodeAIR!\n");
          Crash();
        }
      }
    } break;

    default:
    {
      fprintf(stderr, "Unhandled node type for returning a `jump_instruction::condition` in GenNodeAIR: %s\n", GetNodeName(n->type));
      Crash();
    }
  }
}

template<>
void GenNodeAIR<void>(codegen_target& target, thing_of_code& code, node* n)
{
  assert(n);

  switch (n->type)
  {
    case RETURN_NODE:
    {
      slot_def* returnValue = nullptr;

      if (n->expression)
      {
        returnValue = GenNodeAIR<slot_def*>(target, code, n->expression);
      }

      air_instruction* retInstruction = PushInstruction(code, I_RETURN, returnValue);
      UseSlot(returnValue, retInstruction);
    } break;

    case VARIABLE_ASSIGN_NODE:
    {
      slot_def* variable= GenNodeAIR<slot_def*>(target, code, n->variableAssignment.variable);
      slot_def* newValue= GenNodeAIR<slot_def*>(target, code, n->variableAssignment.newValue);

      air_instruction* instruction = PushInstruction(code, I_MOV, variable, newValue);
      ChangeSlotValue(variable, instruction);
      UseSlot(newValue, instruction);
    } break;

    case BINARY_OP_NODE:
    {
      slot_def* left = GenNodeAIR<slot_def*>(target, code, n->binaryOp.left);
      air_instruction* instruction;

      switch (n->binaryOp.op)
      {
        case TOKEN_DOUBLE_PLUS:
        {
          instruction = PushInstruction(code, I_INC, left);
        } break;

        case TOKEN_DOUBLE_MINUS:
        {
          instruction = PushInstruction(code, I_DEC, left);
        } break;

        default:
        {
          fprintf(stderr, "ICE: Unhandled AST binary op in GenNodeAIR<void?!\n");
          Crash();
        }
      }

      UseSlot(left, instruction);
      ChangeSlotValue(left, instruction);
    } break;

    case CALL_NODE:
    {
      // NOTE(Isaac): we ignore the result, because this function shouldn't return anything
      GenCall(target, code, n);
    } break;

    case IF_NODE:
    {
      assert(n->ifThing.condition->type == CONDITION_NODE);
      assert(n->ifThing.condition->condition.reverseOnJump);
      jump_i::condition jumpCondition = GenNodeAIR<jump_i::condition>(target, code, n->ifThing.condition);

      instruction_label* elseLabel = nullptr;
      instruction_label* endLabel = CreateInstructionLabel();
      
      if (n->ifThing.elseCode)
      {
        elseLabel = CreateInstructionLabel();
      }

      PushInstruction(code, I_JUMP, jumpCondition, (elseLabel ? elseLabel : endLabel));
      GenNodeAIR(target, code, n->ifThing.thenCode);

      if (n->ifThing.elseCode)
      {
        PushInstruction(code, I_JUMP, jump_i::condition::UNCONDITIONAL, endLabel);

        PushInstruction(code, I_LABEL, elseLabel);
        GenNodeAIR(target, code, n->ifThing.elseCode);
      }

      PushInstruction(code, I_LABEL, endLabel);
    } break;

    case WHILE_NODE:
    {
      assert(n->whileThing.condition->type == CONDITION_NODE);
      assert(!(n->whileThing.condition->condition.reverseOnJump));

      instruction_label* label = CreateInstructionLabel();
      PushInstruction(code, I_LABEL, label);

      GenNodeAIR(target, code, n->whileThing.code);

      jump_i::condition loopCondition = GenNodeAIR<jump_i::condition>(target, code, n->whileThing.condition);
      PushInstruction(code, I_JUMP, loopCondition, label);
    } break;

    default:
    {
      fprintf(stderr, "Unhandled node type for returning nothing in GenNodeAIR: %s\n", GetNodeName(n->type));
      Crash();
    }
  }

  /*
   * Because these nodes don't produce a result, they're probably statements.
   * They can therefore be in a block and so can have a node following them.
   */
  if (n->next)
  {
    GenNodeAIR(target, code, n->next);
  }
}

template<>
instruction_label* GenNodeAIR<instruction_label*>(codegen_target& /*target*/, thing_of_code& code, node* n)
{
  assert(n);

  switch (n->type)
  {
    case BREAK_NODE:
    {
      instruction_label* label = static_cast<instruction_label*>(malloc(sizeof(instruction_label)));
      PushInstruction(code, I_JUMP, jump_i::condition::UNCONDITIONAL, label);
      // TODO

      return label;
    } break;

    default:
    {
      fprintf(stderr, "Unhandled node type for returning a `instruction_label*` in GenNodeAIR: %s\n", GetNodeName(n->type));
      Crash();
    }
  }

  return nullptr;
}

static unsigned int GetSlotAccessCost(slot_def* slot)
{
  switch (slot->type)
  {
    case VARIABLE:
    {
      // TODO: think about more expensive addressing modes for things not in variables
      return 1u;
    }

    case RETURN_RESULT:
    case TEMPORARY:
    {
      // NOTE(Isaac): these will always be in a register
      return 1u;
    }

    case INT_CONSTANT:
    case FLOAT_CONSTANT:
    case STRING_CONSTANT:
    {
      return 0u;
    }
  }

  return 0u;
}

/*
 * NOTE(Isaac): returns the result of the function call in a *new* slot.
 */
static slot_def* GenCall(codegen_target& target, thing_of_code& code, node* n)
{
  // TODO: don't assume everything will fit in a general register
  unsigned int numGeneralParams = 0u;
  vector<slot_def*> params;
  InitVector<slot_def*>(params);

  for (auto* paramIt = n->call.params.head;
       paramIt < n->call.params.tail;
       paramIt++)
  {
    slot_def* paramSlot = GenNodeAIR<slot_def*>(target, code, *paramIt);

    switch (paramSlot->type)
    {
      case slot_type::VARIABLE:
      case slot_type::TEMPORARY:
      {
        // NOTE(Isaac): this precolors the slot to be in the correct place to begin with
        paramSlot->color = target.intParamColors[numGeneralParams++];
        Add<slot_def*>(params, paramSlot);
      } break;

      case slot_type::RETURN_RESULT:
      case slot_type::INT_CONSTANT:
      case slot_type::FLOAT_CONSTANT:
      case slot_type::STRING_CONSTANT:
      {
        // NOTE(Isaac): we create a temporary to move the constant into, then color that
        slot_def* temporary = CreateSlot(code, slot_type::TEMPORARY);
        temporary->color = target.intParamColors[numGeneralParams++];
        air_instruction* movInstruction = PushInstruction(code, I_MOV, temporary, paramSlot);
        Add<slot_def*>(params, temporary);

        UseSlot(paramSlot, movInstruction);
        ChangeSlotValue(temporary, movInstruction);
      } break;
    }
  }

  assert(n->call.isResolved);
  air_instruction* callInstruction = PushInstruction(code, I_CALL, n->call.code);

  for (auto* paramIt = params.head;
       paramIt < params.tail;
       paramIt++)
  {
    UseSlot(*paramIt, callInstruction);
  }
  DetachVector<slot_def*>(params);

  if (n->call.code->returnType)
  {
    slot_def* resultSlot = CreateSlot(code, RETURN_RESULT);
    ChangeSlotValue(resultSlot, callInstruction);
    resultSlot->color = target.functionReturnColor;
    char* slotStr = SlotAsString(resultSlot);
    printf("Emitting temporary %s for return value of call to: %s\n", slotStr, n->call.code->mangledName);
    free(slotStr);
    return resultSlot;
  }

  return nullptr;
}

// TODO: these are just made-up bullshit values for instruction costs
// A) It depends on the microarchitecture on how much these cost - (how) do we take that into consideration?
// B) There isn't really a good modern model of the x64 to base this off
// C) We should look into how GCC, Clang etc. do this
unsigned int GetInstructionCost(air_instruction* instruction)
{
  switch (instruction->type)
  {
    // NOTE(Isaac): labels aren't emitted and so don't count towards the cost
    case I_LABEL:
    {
      return 0u;
    }

    case I_RETURN:
    {
      if (instruction->slot)
      {
        return GetSlotAccessCost(instruction->slot);
      }

      return 0u;
    }

    case I_JUMP:
    {
      return 2u;
    }

    case I_MOV:
    {
      return  GetSlotAccessCost(instruction->slotPair.left) +
              GetSlotAccessCost(instruction->slotPair.right);
    }

    case I_CMP:
    {
      return  GetSlotAccessCost(instruction->slotPair.left) +
              GetSlotAccessCost(instruction->slotPair.right);
    }

    case I_BINARY_OP:
    {
      //                          ADD_I   SUB_I   MUL_I   DIV_I
      unsigned int baseCost[] = { 1u,     1u,     2u,     4u};
      return  baseCost[instruction->binaryOp.operation]     +
              GetSlotAccessCost(instruction->binaryOp.left) +
              GetSlotAccessCost(instruction->binaryOp.right);
    }
    
    case I_INC:
    case I_DEC:
    {
      return 1u;
    }

    case I_CALL:
    {
      return 2u;
    }

    case I_NUM_INSTRUCTIONS:
    {
      fprintf(stderr, "ICE: Tried to get cost of instruction with type I_NUM_INSTRUCTIONS\n");
      exit(1);
    }
  }

  return 0u;
}

unsigned int GetCodeCost(thing_of_code& code)
{
  unsigned int cost = 0u;

  for (auto* instruction = code.airHead;
       instruction;
       instruction = instruction->next)
  {
    cost += GetInstructionCost(instruction);
  }

  return cost;
}

// TODO: maybe this could take more context into account?
bool ShouldCodeBeInlined(thing_of_code& code)
{
  static const unsigned int INLINE_THRESHOLD = 16u;

  return   ((GetAttrib(code, attrib_type::INLINE)) &&
           !(GetAttrib(code, attrib_type::NO_INLINE) || GetAttrib(code, attrib_type::ENTRY))) ||
            (GetCodeCost(code) < INLINE_THRESHOLD);
}

bool IsColorInUseAtPoint(thing_of_code& code, air_instruction* instruction, signed int color)
{
  for (auto* it = code.slots.head;
       it < code.slots.tail;
       it++)
  {
    slot_def* slot = *it;

    if (slot->type == slot_type::INT_CONSTANT     ||
        slot->type == slot_type::FLOAT_CONSTANT   ||
        slot->type == slot_type::STRING_CONSTANT  ||
        slot->type == slot_type::RETURN_RESULT)
    {
      continue;
    }

    if (slot->color != color)
    {
      continue;
    }

    for (auto* rangeIt = slot->liveRanges.head;
         rangeIt < slot->liveRanges.tail;
         rangeIt++)
    {
      live_range& range = *rangeIt;
      // TODO: should this be UINT_MAX or automatically return false because we don't care about the value
      // of the register??
      unsigned int lastUse = (range.lastUse ? range.lastUse->index : UINT_MAX);

      if ((instruction->index >= range.definition->index) && (instruction->index <= lastUse))
      {
        return true;
      }
    }
  }

  return false;
}

static void GenerateInterferences(thing_of_code& code)
{
  for (auto* itA = code.slots.head;
       itA < code.slots.tail;
       itA++)
  {
    slot_def* a = *itA;

    if (a->type == slot_type::INT_CONSTANT    ||
        a->type == slot_type::FLOAT_CONSTANT  ||
        a->type == slot_type::STRING_CONSTANT   )
    {
      continue;
    }

    for (auto* itB = (itA + 1u);
         itB < code.slots.tail;
         itB++)
    {
      slot_def* b = *itB;

      if (b->type == slot_type::INT_CONSTANT    ||
          b->type == slot_type::FLOAT_CONSTANT  ||
          b->type == slot_type::STRING_CONSTANT   )
      {
        continue;
      }

      for (auto* aRangeIt = a->liveRanges.head;
           aRangeIt < a->liveRanges.tail;
           aRangeIt++)
      {
        live_range& rangeA = *aRangeIt;

        for (auto* bRangeIt = b->liveRanges.head;
             bRangeIt < b->liveRanges.tail;
             bRangeIt++)
        {
          live_range& rangeB = *bRangeIt;
          unsigned int useA = (rangeA.lastUse ? rangeA.lastUse->index : UINT_MAX);
          unsigned int useB = (rangeB.lastUse ? rangeB.lastUse->index : UINT_MAX);

          // NOTE(Isaac): this checks if the live-ranges intersect
          if ((rangeA.definition->index <= useB) && (rangeB.definition->index <= useA))
          {
            a->interferences[a->numInterferences++] = b;
            b->interferences[b->numInterferences++] = a;
            goto FoundInterference;
          }
        }
      }

      /*
       * NOTE(Isaac): We need the `continue` here even though it's at the end of the loop because
       * C++ makes us have a statement after a label...
       */
FoundInterference:
      continue;
    }
  }
}

/*
 * Used by the AIR generator to color the interference graph to allocate slots to registers.
 */
static void ColorSlots(codegen_target& /*target*/, thing_of_code& code)
{
  const unsigned int numGeneralRegisters = 14u;

  for (auto* it = code.slots.head;
       it < code.slots.tail;
       it++)
  {
    slot_def* slot = *it;

    // Skip if uncolorable or already colored
    if ((slot->type != slot_type::VARIABLE && slot->type != slot_type::TEMPORARY) ||
        (slot->color != -1))
    {
      continue;
    }

    // Find colors already used by interferring nodes
    bool usedColors[numGeneralRegisters] = {0};
    for (unsigned int i = 0u;
         i < slot->numInterferences;
         i++)
    {
      signed int interference = slot->interferences[i]->color;

      if (interference != -1)
      {
        usedColors[interference] = true;
      }
    }

    // Choose a free color
    for (unsigned int i = 0u;
         i < numGeneralRegisters;
         i++)
    {
      if (!usedColors[i])
      {
        slot->color = static_cast<signed int>(i);
        break;
      }
    }

    if (slot->color == -1)
    {
      // TODO: spill something instead of crashing
      fprintf(stderr, "FATAL: failed to find valid k-coloring of interference graph!\n");
      Crash();
    }
  }
}

void GenerateAIR(codegen_target& target, thing_of_code& code)
{
  assert(!(code.airHead));

  // NOTE(Isaac): this prints all the nodes in the function's outermost scope
  GenNodeAIR(target, code, code.ast);

  // Color the interference graph
  GenerateInterferences(code);
  ColorSlots(target, code);

#if 1
  printf("--- AIR instruction listing for function: %s ---\n", code.mangledName);
  for (air_instruction* i = code.airHead;
       i;
       i = i->next)
  {
    PrintInstruction(i);
  }

  printf("\n--- Slot listing for function: %s ---\n", code.mangledName);
  for (auto* it = code.slots.head;
       it < code.slots.tail;
       it++)
  {
    slot_def* slot = *it;

    if (slot->type == slot_type::INT_CONSTANT   ||
        slot->type == slot_type::FLOAT_CONSTANT ||
        slot->type == slot_type::STRING_CONSTANT  )
    {
      continue;
    }

    char* slotString = SlotAsString(slot);
    printf("%-15s ", slotString);
    free(slotString);

    for (auto* rangeIt = slot->liveRanges.head;
         rangeIt < slot->liveRanges.tail;
         rangeIt++)
    {
      live_range& range = *rangeIt;

      if (range.lastUse)
      {
        printf("(%u..%u) ", range.definition->index, range.lastUse->index);
      }
      else
      {
        // NOTE(Isaac): `??)` forms a trigraph so escape one of the question marks
        printf("(%u..?\?) ", range.definition->index);
      }
    }
    printf("\n");
  }
  printf("\n");
#endif
}

void PrintInstruction(air_instruction* instruction)
{
  switch (instruction->type)
  {
    case I_RETURN:
    {
      char* returnValue = nullptr;

      if (instruction->slot)
      {
        returnValue = SlotAsString(instruction->slot);
      }

      printf("%u: RETURN %s\n", instruction->index, returnValue);
      free(returnValue);
    } break;

    case I_JUMP:
    {
      switch (instruction->jump.cond)
      {
        case jump_i::condition::UNCONDITIONAL:
        {
          printf("%u: JMP I(0x%lx)\n", instruction->index, instruction->jump.label->offset);
        } break;

        case jump_i::condition::IF_EQUAL:
        {
          printf("%u: JE I(0x%lx)\n", instruction->index, instruction->jump.label->offset);
        } break;

        case jump_i::condition::IF_NOT_EQUAL:
        {
          printf("%u: JNE I(0x%lx)\n", instruction->index, instruction->jump.label->offset);
        } break;

        case jump_i::condition::IF_OVERFLOW:
        {
          printf("%u: JO I(0x%lx)\n", instruction->index, instruction->jump.label->offset);
        } break;

        case jump_i::condition::IF_NOT_OVERFLOW:
        {
          printf("%u: JNO I(0x%lx)\n", instruction->index, instruction->jump.label->offset);
        } break;

        case jump_i::condition::IF_SIGN:
        {
          printf("%u: JS I(0x%lx)\n", instruction->index, instruction->jump.label->offset);
        } break;

        case jump_i::condition::IF_NOT_SIGN:
        {
          printf("%u: JNS I(0x%lx)\n", instruction->index, instruction->jump.label->offset);
        } break;

        case jump_i::condition::IF_GREATER:
        {
          printf("%u: JG I(0x%lx)\n", instruction->index, instruction->jump.label->offset);
        } break;

        case jump_i::condition::IF_GREATER_OR_EQUAL:
        {
          printf("%u: JGE I(0x%lx)\n", instruction->index, instruction->jump.label->offset);
        } break;

        case jump_i::condition::IF_LESSER:
        {
          printf("%u: JL I(0x%lx)\n", instruction->index, instruction->jump.label->offset);
        } break;

        case jump_i::condition::IF_LESSER_OR_EQUAL:
        {
          printf("%u: JLE I(0x%lx)\n", instruction->index, instruction->jump.label->offset);
        } break;

        case jump_i::condition::IF_PARITY_EVEN:
        {
          printf("%u: JPE I(0x%lx)\n", instruction->index, instruction->jump.label->offset);
        } break;

        case jump_i::condition::IF_PARITY_ODD:
        {
          printf("%u: JPO I(0x%lx)\n", instruction->index, instruction->jump.label->offset);
        } break;
      }
    } break;

    case I_MOV:
    {
      char* srcSlot = SlotAsString(instruction->mov.src);
      char* destSlot = SlotAsString(instruction->mov.dest);
      printf("%u: %s -> %s\n", instruction->index, srcSlot, destSlot);
      free(srcSlot);
      free(destSlot);
    } break;

    case I_CMP:
    {
      char* leftSlot = SlotAsString(instruction->slotPair.left);
      char* rightSlot = SlotAsString(instruction->slotPair.right);
      printf("%u: CMP %s, %s\n", instruction->index, leftSlot, rightSlot);
      free(leftSlot);
      free(rightSlot);
    } break;

    case I_BINARY_OP:
    {
      const char* opString;

      switch (instruction->binaryOp.operation)
      {
        case binary_op_i::op::ADD_I: opString = "+"; break;
        case binary_op_i::op::SUB_I: opString = "-"; break;
        case binary_op_i::op::MUL_I: opString = "*"; break;
        case binary_op_i::op::DIV_I: opString = "/"; break;
      }

      char* leftSlot = SlotAsString(instruction->binaryOp.left);
      char* rightSlot = SlotAsString(instruction->binaryOp.right);
      char* resultSlot = SlotAsString(instruction->binaryOp.result);
      printf("%u: %s := %s %s %s\n", instruction->index, resultSlot, leftSlot, opString, rightSlot);
      free(leftSlot);
      free(rightSlot);
      free(resultSlot);
    } break;

    case I_INC:
    {
      char* slot = SlotAsString(instruction->slot);
      printf("%u: INC %s\n", instruction->index, slot);
      free(slot);
    } break;

    case I_DEC:
    {
      char* slot = SlotAsString(instruction->slot);
      printf("%u: DEC %s\n", instruction->index, slot);
      free(slot);
    } break;

    case I_CALL:
    {
      printf("%u: CALL %s\n", instruction->index, instruction->call->mangledName);
    } break;

    case I_LABEL:
    {
      printf("LABEL\n");
      // TODO: be more helpful to the poor sod debugging here
    } break;

    case I_NUM_INSTRUCTIONS:
    {
      printf("!!!I_NUM_INSTRUCTIONS!!!\n");
    } break;
  }
}

const char* GetInstructionName(air_instruction* instruction)
{
  switch (instruction->type)
  {
    case I_RETURN:
      return "RETURN";
    case I_JUMP:
      return "JUMP";
    case I_MOV:
      return "MOV";
    case I_CMP:
      return "CMP";
    case I_BINARY_OP:
    {
      switch (instruction->binaryOp.operation)
      {
        case binary_op_i::op::ADD_I: return "ADD_I";
        case binary_op_i::op::SUB_I: return "SUB_I";
        case binary_op_i::op::MUL_I: return "MUL_I";
        case binary_op_i::op::DIV_I: return "DIV_I";
      }
    }
    case I_INC:
      return "INC";
    case I_DEC:
      return "DEC";
    case I_CALL:
      return "CALL";
    case I_LABEL:
      return "LABEL";
    case I_NUM_INSTRUCTIONS:
      return "!!!I_NUM_INSTRUCTIONS!!!";
  }

  return nullptr;
}

#ifdef OUTPUT_DOT
void OutputInterferenceDOT(thing_of_code& code, const char* name)
{
  if (code.airHead == nullptr)
  {
    return;
  }

  char fileName[128u] = {0};
  strcpy(fileName, name);
  strcat(fileName, "_interference.dot");
  FILE* f = fopen(fileName, "w");

  if (!f)
  {
    fprintf(stderr, "Failed to open DOT file: %s!\n", fileName);
    Crash();
  }

  fprintf(f, "digraph G\n{\n");
  unsigned int i = 0u;

  const char* snazzyColors[] =
  {
    "cyan2", "deeppink", "darkgoldenrod2", "mediumpurple2", "slategray", "goldenrod", "darkorchid1", "plum",
    "green3", "lightblue2", "mediumspringgreeen", "orange1", "mistyrose3", "maroon2", "dodgerblue4",
    "steelblue2", "blue", "lightseagreen"
  };

  for (auto* it = code.slots.head;
       it < code.slots.tail;
       it++)
  {
    slot_def* slot = *it;
    const char* color = "black";

    if (slot->type != slot_type::VARIABLE && slot->type != slot_type::TEMPORARY)
    {
      continue;
    }

    if (slot->color < 0)
    {
      fprintf(stderr, "WARNING: found uncolored node! Using red!\n");
      color = "red";
    }
    else
    {
      color = snazzyColors[slot->color];
    }

    /*
     * NOTE(Isaac): this is a slightly hacky way of concatenating strings using GCC varadic macros,
     * since apparently the ## preprocessing token is next to bloody useless...
     */
    #define CAT(A, B, C) A B C
    #define PRINT_SLOT(label, ...) \
        fprintf(f, CAT("\ts%u[label=\"", label, "\" color=\"%s\" fontcolor=\"%s\"];\n"), \
            i, __VA_ARGS__, color, color);

    switch (slot->type)
    {
      case slot_type::VARIABLE:
      {
        PRINT_SLOT("%s : VAR", slot->variable->name);
      } break;

      case slot_type::TEMPORARY:
      {
        PRINT_SLOT("t%u : TMP", slot->tag);
      } break;

      case slot_type::RETURN_RESULT:
      {
        PRINT_SLOT("r%u : RES", slot->tag);
      } break;

      case slot_type::INT_CONSTANT:
      {
        PRINT_SLOT("%d : INT", slot->i);
      } break;

      case slot_type::FLOAT_CONSTANT:
      {
        PRINT_SLOT("%f : FLOAT", slot->f);
      } break;

      case slot_type::STRING_CONSTANT:
      {
        PRINT_SLOT("\\\"%s\\\" : STRING", slot->string->string);
      } break;
    }

    slot->dotTag = i;
    i++;
  }

  // Emit the interferences between them
  for (auto* it = code.slots.head;
       it < code.slots.tail;
       it++)
  {
    slot_def* slot = *it;

    for (unsigned int i = 0u;
         i < slot->numInterferences;
         i++)
    {
      // NOTE(Isaac): this is a slightly tenuous way to avoid emitting duplicates
      if (slot->dotTag < slot->interferences[i]->dotTag)
      {
        fprintf(f, "\ts%u -> s%u[dir=none];\n", slot->dotTag, slot->interferences[i]->dotTag);
      }
    }
  }

  fprintf(f, "}\n");
  fclose(f);
}
#endif
