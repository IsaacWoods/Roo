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

instruction_label* CreateInstructionLabel()
{
  instruction_label* label = static_cast<instruction_label*>(malloc(sizeof(instruction_label)));
  label->offset = 0u;
  
  return label;
}

static air_instruction* PushInstruction(function_def* function, instruction_type type, ...)
{
  va_list args;
  va_start(args, type);

  air_instruction* i = static_cast<air_instruction*>(malloc(sizeof(air_instruction)));
  i->type = type;
  i->next = nullptr;

  switch (type)
  {
    case I_ENTER_STACK_FRAME:
    {
    } break;

    case I_LEAVE_STACK_FRAME:
    {
    } break;

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
      i->function             = va_arg(args, function_def*);
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

  if (function->airHead)
  {
    i->index = function->airTail->index + 1u;
    function->airTail->next = i;
    function->airTail = i;
  }
  else
  {
    i->index = 0u;
    function->airHead = i;
    function->airTail = i;
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
T GenNodeAIR(codegen_target& target, function_def* function, node* n);

template<>
slot_def* GenNodeAIR<slot_def*>(codegen_target& target, function_def* function, node* n)
{
  assert(n);

  switch (n->type)
  {
    case BINARY_OP_NODE:
    {
      slot_def* left = GenNodeAIR<slot_def*>(target, function, n->binaryOp.left);
      slot_def* right = GenNodeAIR<slot_def*>(target, function, n->binaryOp.right);
      slot_def* result = CreateSlot(function, slot_type::TEMPORARY);
      air_instruction* instruction;

      switch (n->binaryOp.op)
      {
        case TOKEN_PLUS:
        {
          instruction = PushInstruction(function, I_BINARY_OP, binary_op_i::op::ADD_I, left, right, result);
        } break;

        case TOKEN_MINUS:
        {
          instruction = PushInstruction(function, I_BINARY_OP, binary_op_i::op::SUB_I, left, right, result);
        } break;

        case TOKEN_ASTERIX:
        {
          instruction = PushInstruction(function, I_BINARY_OP, binary_op_i::op::MUL_I, left, right, result);
        } break;

        case TOKEN_SLASH:
        {
          instruction = PushInstruction(function, I_BINARY_OP, binary_op_i::op::DIV_I, left, right, result);
        } break;

        case TOKEN_DOUBLE_PLUS:
        {
          instruction = PushInstruction(function, I_INC, left);
        } break;

        case TOKEN_DOUBLE_MINUS:
        {
          instruction = PushInstruction(function, I_DEC, left);
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
      slot_def* right = GenNodeAIR<slot_def*>(target, function, n->prefixOp.right);
      slot_def* result = CreateSlot(function, slot_type::TEMPORARY);
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

    case VARIABLE_NODE:
    {
      assert(n->variable.isResolved);
      variable_def* variable = n->variable.var;

      if (!(variable->slot))
      {
        variable->slot = CreateSlot(function, slot_type::VARIABLE, variable);
      }
      
      return variable->slot;
    } break;

    case NUMBER_CONSTANT_NODE:
    {
      switch (n->numberConstant.type)
      {
        case number_constant_part::constant_type::CONSTANT_TYPE_INT:
        {
          return CreateSlot(function, slot_type::INT_CONSTANT, n->numberConstant.asInt);
        } break;

        case number_constant_part::constant_type::CONSTANT_TYPE_FLOAT:
        {
          return CreateSlot(function, slot_type::FLOAT_CONSTANT, n->numberConstant.asFloat);
        } break;
      }
    };

    case STRING_CONSTANT_NODE:
    {
      return CreateSlot(function, slot_type::STRING_CONSTANT, n->stringConstant);
    } break;

    default:
    {
      fprintf(stderr, "Unhandled node type for returning a `slot*` in GenNodeAIR: %s\n", GetNodeName(n->type));
      Crash();
    }
  }
}

template<>
jump_i::condition GenNodeAIR<jump_i::condition>(codegen_target& target, function_def* function, node* n)
{
  assert(n);

  switch (n->type)
  {
    case CONDITION_NODE:
    {
      slot_def* a = GenNodeAIR<slot_def*>(target, function, n->condition.left);
      slot_def* b = GenNodeAIR<slot_def*>(target, function, n->condition.right);

      air_instruction* instruction = PushInstruction(function, I_CMP, a, b);
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
void GenNodeAIR<void>(codegen_target& target, function_def* function, node* n)
{
  assert(n);

  switch (n->type)
  {
    case RETURN_NODE:
    {
      slot_def* returnValue = nullptr;

      if (n->expression)
      {
        returnValue = GenNodeAIR<slot_def*>(target, function, n->expression);
      }

      PushInstruction(function, I_LEAVE_STACK_FRAME);
      air_instruction* retInstruction = PushInstruction(function, I_RETURN, returnValue);
      UseSlot(returnValue, retInstruction);
    } break;

    case VARIABLE_ASSIGN_NODE:
    {
      slot_def* variable= GenNodeAIR<slot_def*>(target, function, n->variableAssignment.variable);
      slot_def* newValue= GenNodeAIR<slot_def*>(target, function, n->variableAssignment.newValue);

      air_instruction* instruction = PushInstruction(function, I_MOV, variable, newValue);
      ChangeSlotValue(variable, instruction);
      UseSlot(newValue, instruction);
    } break;

    case BINARY_OP_NODE:
    {
      slot_def* left = GenNodeAIR<slot_def*>(target, function, n->binaryOp.left);
      air_instruction* instruction;

      switch (n->binaryOp.op)
      {
        case TOKEN_DOUBLE_PLUS:
        {
          instruction = PushInstruction(function, I_INC, left);
        } break;

        case TOKEN_DOUBLE_MINUS:
        {
          instruction = PushInstruction(function, I_DEC, left);
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

    case FUNCTION_CALL_NODE:
    {
      // TODO: don't assume everything will fit in a general register
      unsigned int numGeneralParams = 0u;
      vector<slot_def*> params;
      InitVector<slot_def*>(params);

      for (auto* paramIt = n->functionCall.params.head;
           paramIt < n->functionCall.params.tail;
           paramIt++)
      {
        slot_def* paramSlot = GenNodeAIR<slot_def*>(target, function, *paramIt);

        switch (paramSlot->type)
        {
          case slot_type::VARIABLE:
          case slot_type::TEMPORARY:
          {
            // NOTE(Isaac): this precolors the slot to be in the correct place to begin with
            paramSlot->color = target.intParamColors[numGeneralParams++];
            Add<slot_def*>(params, paramSlot);
          } break;

          case slot_type::INT_CONSTANT:
          case slot_type::FLOAT_CONSTANT:
          case slot_type::STRING_CONSTANT:
          {
            // NOTE(Isaac): we create a temporary to move the constant into, then color that
            slot_def* temporary = CreateSlot(function, slot_type::TEMPORARY);
            temporary->color = target.intParamColors[numGeneralParams++];
            air_instruction* movInstruction = PushInstruction(function, I_MOV, temporary, paramSlot);
            Add<slot_def*>(params, temporary);

            UseSlot(paramSlot, movInstruction);
            ChangeSlotValue(temporary, movInstruction);
          } break;
        }
      }

      assert(n->functionCall.isResolved);
      air_instruction* callInstruction = PushInstruction(function, I_CALL, n->functionCall.function);

      for (auto* paramIt = params.head;
           paramIt < params.tail;
           paramIt++)
      {
        UseSlot(*paramIt, callInstruction);
      }
      DetachVector<slot_def*>(params);
    } break;

    case IF_NODE:
    {
      assert(n->ifThing.condition->type == CONDITION_NODE);
      assert(n->ifThing.condition->condition.reverseOnJump);
      jump_i::condition jumpCondition = GenNodeAIR<jump_i::condition>(target, function, n->ifThing.condition);

      instruction_label* elseLabel = nullptr;
      instruction_label* endLabel = CreateInstructionLabel();
      
      if (n->ifThing.elseCode)
      {
        elseLabel = CreateInstructionLabel();
      }

      PushInstruction(function, I_JUMP, jumpCondition, (elseLabel ? elseLabel : endLabel));
      GenNodeAIR(target, function, n->ifThing.thenCode);

      if (n->ifThing.elseCode)
      {
        PushInstruction(function, I_JUMP, jump_i::condition::UNCONDITIONAL, endLabel);

        PushInstruction(function, I_LABEL, elseLabel);
        GenNodeAIR(target, function, n->ifThing.elseCode);
      }

      PushInstruction(function, I_LABEL, endLabel);
    } break;

    case WHILE_NODE:
    {
      assert(n->whileThing.condition->type == CONDITION_NODE);
      assert(!(n->whileThing.condition->condition.reverseOnJump));

      instruction_label* label = CreateInstructionLabel();
      PushInstruction(function, I_LABEL, label);

      GenNodeAIR(target, function, n->whileThing.code);

      jump_i::condition loopCondition = GenNodeAIR<jump_i::condition>(target, function, n->whileThing.condition);
      PushInstruction(function, I_JUMP, loopCondition, label);
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
    GenNodeAIR(target, function, n->next);
  }
}

template<>
instruction_label* GenNodeAIR<instruction_label*>(codegen_target& /*target*/, function_def* function, node* n)
{
  assert(n);

  switch (n->type)
  {
    case BREAK_NODE:
    {
      instruction_label* label = static_cast<instruction_label*>(malloc(sizeof(instruction_label)));
      PushInstruction(function, I_JUMP, jump_i::condition::UNCONDITIONAL, label);
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

bool IsColorInUseAtPoint(function_def* function, air_instruction* instruction, signed int color)
{
  for (auto* it = function->slots.head;
       it < function->slots.tail;
       it++)
  {
    slot_def* slot = *it;

    if (slot->color != color)
    {
      continue;
    }

    for (auto* rangeIt = slot->liveRanges.head;
         rangeIt < slot->liveRanges.tail;
         rangeIt++)
    {
      live_range& range = *rangeIt;

      if ((instruction->index >= range.definition->index) && (instruction->index <= range.lastUse->index))
      {
        return true;
      }
    }
  }

  return false;
}

static void GenerateInterferences(function_def* function)
{
  for (auto* itA = function->slots.head;
       itA < function->slots.tail;
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
         itB < function->slots.tail;
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
static void ColorSlots(codegen_target& /*target*/, function_def* function)
{
  const unsigned int numGeneralRegisters = 14u;

  // --- Color params ---
  // TODO: color incoming parameters to this function
/*  unsigned int intParamCounter = 0u;

  for (auto* slotIt = function->slots.first;
       slotIt;
       slotIt = slotIt->next)
  {
    if ((**slotIt)->type == slot::slot_type::PARAM)
    {
      printf("Coloring param slot: %s\n", (**slotIt)->variable->name);
      (**slotIt)->color = target.intParamColors[intParamCounter++];
    }
  }*/

  // --- Color other slots ---
  for (auto* it = function->slots.head;
       it < function->slots.tail;
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

void GenFunctionAIR(codegen_target& target, function_def* function)
{
  assert(!(function->airHead));
  PushInstruction(function, I_ENTER_STACK_FRAME);

  // NOTE(Isaac): this prints all the nodes in the function's outermost scope
  GenNodeAIR(target, function, function->ast);

  if (function->scope.shouldAutoReturn)
  {
    PushInstruction(function, I_LEAVE_STACK_FRAME);
    PushInstruction(function, I_RETURN, nullptr);
  }

  // Color the interference graph
  GenerateInterferences(function);
  ColorSlots(target, function);

#ifdef OUTPUT_DOT
  CreateInterferenceDOT(function);
#endif

#if 1
  printf("--- AIR instruction listing for function: %s\n", function->name);
  for (air_instruction* i = function->airHead;
       i;
       i = i->next)
  {
    PrintInstruction(i);
  }

  printf("\n--- Slot listing for function: %s ---\n", function->name);
  for (auto* it = function->slots.head;
       it < function->slots.tail;
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
    case I_ENTER_STACK_FRAME:
    case I_LEAVE_STACK_FRAME:
    {
      printf("%u: %s\n", instruction->index, GetInstructionName(instruction));
    } break;

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
      printf("%u: CALL %s\n", instruction->index, instruction->function->name);
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
    case I_ENTER_STACK_FRAME:
      return "ENTER_STACK_FRAME";
    case I_LEAVE_STACK_FRAME:
      return "LEAVE_STACK_FRAME";
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
void CreateInterferenceDOT(function_def* function)
{
  // Check if the function's empty
  if (function->airHead == nullptr)
  {
    return;
  }

  char fileName[128u] = {0};
  strcpy(fileName, function->name);
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

  for (auto* it = function->slots.head;
       it < function->slots.tail;
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
  for (auto* it = function->slots.head;
       it < function->slots.tail;
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

template<>
void Free<air_instruction*>(air_instruction*& instruction)
{
  free(instruction);
}
