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
      i->payload.slot                 = va_arg(args, slot_def*);
    } break;

    case I_JUMP:
    {
      i->payload.jump.cond            = static_cast<jump_i::condition>(va_arg(args, int));
      i->payload.jump.label           = va_arg(args, instruction_label*);
    } break;

    case I_MOV:
    {
      i->payload.mov.dest             = va_arg(args, slot_def*);
      i->payload.mov.src              = va_arg(args, slot_def*);
    } break;

    case I_CMP:
    {
      i->payload.slotTriple.left      = va_arg(args, slot_def*);
      i->payload.slotTriple.right     = va_arg(args, slot_def*);
      i->payload.slotTriple.result    = va_arg(args, slot_def*);
    } break;

    case I_BINARY_OP:
    {
      i->payload.binaryOp.operation   = static_cast<binary_op_i::op>(va_arg(args, int));
      i->payload.binaryOp.left        = va_arg(args, slot_def*);
      i->payload.binaryOp.right       = va_arg(args, slot_def*);
      i->payload.binaryOp.result      = va_arg(args, slot_def*);
    } break;

    case I_CALL:
    {
      i->payload.function             = va_arg(args, function_def*);
    } break;

    case I_LABEL:
    {
      i->payload.label                = va_arg(args, instruction_label*);
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

  if (slot->liveRanges.tail)
  {
    live_range& lastRange = **(slot->liveRanges.tail);
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
 * BINARY_OP_NODE:          `slot_def*
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
      slot_def* left = GenNodeAIR<slot_def*>(target, function, n->payload.binaryOp.left);
      slot_def* right = GenNodeAIR<slot_def*>(target, function, n->payload.binaryOp.right);
      slot_def* result = CreateSlot(function, slot_type::TEMPORARY);
      binary_op_i::op operation;

      switch (n->payload.binaryOp.op)
      {
        case TOKEN_PLUS:    operation = binary_op_i::op::ADD_I; break;
        case TOKEN_MINUS:   operation = binary_op_i::op::SUB_I; break;
        case TOKEN_ASTERIX: operation = binary_op_i::op::MUL_I; break;
        case TOKEN_SLASH:   operation = binary_op_i::op::DIV_I; break;
    
        default:
        {
          fprintf(stderr, "Unhandled AST binary op in GenNodeAIR!\n");
          Crash();
        }
      }
      
      air_instruction* instruction = PushInstruction(function, I_BINARY_OP, operation, left, right, result);
      UseSlot(left, instruction);
      UseSlot(right, instruction);
      ChangeSlotValue(result, instruction);

      return result;
    } break;

    case PREFIX_OP_NODE:
    {
      slot_def* right = GenNodeAIR<slot_def*>(target, function, n->payload.prefixOp.right);
      slot_def* result = CreateSlot(function, slot_type::TEMPORARY);
      air_instruction* instruction = nullptr;

      switch (n->payload.prefixOp.op)
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
      assert(n->payload.variable.isResolved);
      variable_def* variable = n->payload.variable.var.def;

      if (!(variable->slot))
      {
        variable->slot = CreateSlot(function, slot_type::VARIABLE, variable);
      }
      
      return variable->slot;
    } break;

    case NUMBER_CONSTANT_NODE:
    {
      switch (n->payload.numberConstant.type)
      {
        case number_constant_part::constant_type::CONSTANT_TYPE_INT:
        {
          return CreateSlot(function, slot_type::INT_CONSTANT, n->payload.numberConstant.constant.i);
        } break;

        case number_constant_part::constant_type::CONSTANT_TYPE_FLOAT:
        {
          return CreateSlot(function, slot_type::FLOAT_CONSTANT, n->payload.numberConstant.constant.f);
        } break;
      }
    };

    case STRING_CONSTANT_NODE:
    {
      return CreateSlot(function, slot_type::STRING_CONSTANT, n->payload.stringConstant);
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
      slot_def* a = GenNodeAIR<slot_def*>(target, function, n->payload.condition.left);
      slot_def* b = GenNodeAIR<slot_def*>(target, function, n->payload.condition.right);

      air_instruction* instruction = PushInstruction(function, I_CMP, a, b);
      UseSlot(a, instruction);
      UseSlot(a, instruction);

      switch (n->payload.condition.condition)
      {
        case TOKEN_EQUALS_EQUALS:
        {
          return (n->payload.condition.reverseOnJump ?
                  jump_i::condition::IF_NOT_EQUAL :
                  jump_i::condition::IF_EQUAL);
        }

        case TOKEN_BANG_EQUALS:
        {
          return (n->payload.condition.reverseOnJump ?
                  jump_i::condition::IF_EQUAL :
                  jump_i::condition::IF_NOT_EQUAL);
        }

        case TOKEN_GREATER_THAN:
        {
          return (n->payload.condition.reverseOnJump ?
                  jump_i::condition::IF_LESSER_OR_EQUAL :
                  jump_i::condition::IF_GREATER);
        }

        case TOKEN_GREATER_THAN_EQUAL_TO:
        {
          return (n->payload.condition.reverseOnJump ?
                  jump_i::condition::IF_LESSER :
                  jump_i::condition::IF_GREATER_OR_EQUAL);
        }

        case TOKEN_LESS_THAN:
        {
          return (n->payload.condition.reverseOnJump ?
                  jump_i::condition::IF_GREATER_OR_EQUAL :
                  jump_i::condition::IF_LESSER);
        }

        case TOKEN_LESS_THAN_EQUAL_TO:
        {
          return (n->payload.condition.reverseOnJump ?
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

      if (n->payload.expression)
      {
        returnValue = GenNodeAIR<slot_def*>(target, function, n->payload.expression);
      }

      PushInstruction(function, I_LEAVE_STACK_FRAME);
      air_instruction* retInstruction = PushInstruction(function, I_RETURN, returnValue);
      UseSlot(returnValue, retInstruction);
    } break;

    case VARIABLE_ASSIGN_NODE:
    {
      slot_def* variable= GenNodeAIR<slot_def*>(target, function, n->payload.variableAssignment.variable);
      slot_def* newValue= GenNodeAIR<slot_def*>(target, function, n->payload.variableAssignment.newValue);

      air_instruction* instruction = PushInstruction(function, I_MOV, variable, newValue);
      ChangeSlotValue(variable, instruction);
      UseSlot(newValue, instruction);
    } break;

    case FUNCTION_CALL_NODE:
    {
      // TODO: don't assume everything will fit in a general register
/*      unsigned int numGeneralParams = 0u;
      for (auto* paramIt = n->payload.functionCall.params.first;
           paramIt;
           paramIt = paramIt->next)
      {
        slot* param = CreateSlot(function, slot::slot_type::IN_PARAM, target.intParamColors[numGeneralParams++]);
        slot* value = GenNodeAIR<slot*>(target, function, **paramIt);

        air_instruction* iMov = PushInstruction(function, I_MOV, param, value);
        param->range.definer = iMov;
        value->range.lastUser = iMov;
      }

      assert(n->payload.functionCall.isResolved);
      PushInstruction(function, I_CALL, n->payload.functionCall.function.def);*/

      // TODO: do this properly:
      //   * Save and restore caller-saved registers (that are in use)
      //   * Mark registers used by params
    } break;

    case IF_NODE:
    {
      assert(n->payload.ifThing.condition->type == CONDITION_NODE);
      assert(n->payload.ifThing.condition->payload.condition.reverseOnJump);
      jump_i::condition jumpCondition = GenNodeAIR<jump_i::condition>(target, function, n->payload.ifThing.condition);

      instruction_label* elseLabel = nullptr;
      instruction_label* endLabel = CreateInstructionLabel();
      
      if (n->payload.ifThing.elseCode)
      {
        elseLabel = CreateInstructionLabel();
      }

      PushInstruction(function, I_JUMP, jumpCondition, (elseLabel ? elseLabel : endLabel));
      GenNodeAIR(target, function, n->payload.ifThing.thenCode);

      if (n->payload.ifThing.elseCode)
      {
        PushInstruction(function, I_JUMP, jump_i::condition::UNCONDITIONAL, endLabel);

        PushInstruction(function, I_LABEL, elseLabel);
        GenNodeAIR(target, function, n->payload.ifThing.elseCode);
      }

      PushInstruction(function, I_LABEL, endLabel);
    } break;

    case WHILE_NODE:
    {
      assert(n->payload.whileThing.condition->type == CONDITION_NODE);
      assert(!(n->payload.whileThing.condition->payload.condition.reverseOnJump));

      instruction_label* label = CreateInstructionLabel();
      PushInstruction(function, I_LABEL, label);

      GenNodeAIR(target, function, n->payload.whileThing.code);

      jump_i::condition loopCondition = GenNodeAIR<jump_i::condition>(target, function, n->payload.whileThing.condition);
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

static void GenerateInterferences(function_def* function)
{
  for (auto* itA = function->slots.first;
       itA;
       itA = itA->next)
  {
    slot_def* a = **itA;

    if (a->type == slot_type::INT_CONSTANT    ||
        a->type == slot_type::FLOAT_CONSTANT  ||
        a->type == slot_type::STRING_CONSTANT   )
    {
      continue;
    }

    for (auto* itB = itA->next;
         itB;
         itB = itB->next)
    {
      slot_def* b = **itB;

      if (b->type == slot_type::INT_CONSTANT    ||
          b->type == slot_type::FLOAT_CONSTANT  ||
          b->type == slot_type::STRING_CONSTANT   )
      {
        continue;
      }

      for (auto* aRangeIt = a->liveRanges.first;
           aRangeIt;
           aRangeIt = aRangeIt->next)
      {
        live_range& rangeA = **aRangeIt;

        for (auto* bRangeIt = b->liveRanges.first;
             bRangeIt;
             bRangeIt = bRangeIt->next)
        {
          live_range& rangeB = **bRangeIt;
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
      printf("Coloring param slot: %s\n", (**slotIt)->payload.variable->name);
      (**slotIt)->color = target.intParamColors[intParamCounter++];
    }
  }*/

  // --- Color other slots ---
  for (auto* it = function->slots.first;
       it;
       it = it->next)
  {
    slot_def* slot = **it;

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
  for (auto* it = function->slots.first;
       it;
       it = it->next)
  {
    slot_def* slot = **it;

    if (slot->type == slot_type::INT_CONSTANT   ||
        slot->type == slot_type::FLOAT_CONSTANT ||
        slot->type == slot_type::STRING_CONSTANT  )
    {
      continue;
    }

    char* slotString = SlotAsString(slot);
    printf("%-15s ", slotString);
    free(slotString);

    for (auto* rangeIt = slot->liveRanges.first;
         rangeIt;
         rangeIt = rangeIt->next)
    {
      live_range& range = **rangeIt;

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

      if (instruction->payload.slot)
      {
        returnValue = SlotAsString(instruction->payload.slot);
      }

      printf("%u: RETURN %s\n", instruction->index, returnValue);
      free(returnValue);
    } break;

    case I_JUMP:
    {
      switch (instruction->payload.jump.cond)
      {
        case jump_i::condition::UNCONDITIONAL:
        {
          printf("%u: JMP I(0x%lx)\n", instruction->index, instruction->payload.jump.label->offset);
        } break;

        case jump_i::condition::IF_EQUAL:
        {
          printf("%u: JE I(0x%lx)\n", instruction->index, instruction->payload.jump.label->offset);
        } break;

        case jump_i::condition::IF_NOT_EQUAL:
        {
          printf("%u: JNE I(0x%lx)\n", instruction->index, instruction->payload.jump.label->offset);
        } break;

        case jump_i::condition::IF_OVERFLOW:
        {
          printf("%u: JO I(0x%lx)\n", instruction->index, instruction->payload.jump.label->offset);
        } break;

        case jump_i::condition::IF_NOT_OVERFLOW:
        {
          printf("%u: JNO I(0x%lx)\n", instruction->index, instruction->payload.jump.label->offset);
        } break;

        case jump_i::condition::IF_SIGN:
        {
          printf("%u: JS I(0x%lx)\n", instruction->index, instruction->payload.jump.label->offset);
        } break;

        case jump_i::condition::IF_NOT_SIGN:
        {
          printf("%u: JNS I(0x%lx)\n", instruction->index, instruction->payload.jump.label->offset);
        } break;

        case jump_i::condition::IF_GREATER:
        {
          printf("%u: JG I(0x%lx)\n", instruction->index, instruction->payload.jump.label->offset);
        } break;

        case jump_i::condition::IF_GREATER_OR_EQUAL:
        {
          printf("%u: JGE I(0x%lx)\n", instruction->index, instruction->payload.jump.label->offset);
        } break;

        case jump_i::condition::IF_LESSER:
        {
          printf("%u: JL I(0x%lx)\n", instruction->index, instruction->payload.jump.label->offset);
        } break;

        case jump_i::condition::IF_LESSER_OR_EQUAL:
        {
          printf("%u: JLE I(0x%lx)\n", instruction->index, instruction->payload.jump.label->offset);
        } break;

        case jump_i::condition::IF_PARITY_EVEN:
        {
          printf("%u: JPE I(0x%lx)\n", instruction->index, instruction->payload.jump.label->offset);
        } break;

        case jump_i::condition::IF_PARITY_ODD:
        {
          printf("%u: JPO I(0x%lx)\n", instruction->index, instruction->payload.jump.label->offset);
        } break;
      }
    } break;

    case I_MOV:
    {
      char* srcSlot = SlotAsString(instruction->payload.mov.src);
      char* destSlot = SlotAsString(instruction->payload.mov.dest);
      printf("%u: %s -> %s\n", instruction->index, srcSlot, destSlot);
      free(srcSlot);
      free(destSlot);
    } break;

    case I_CMP:
    {
      char* leftSlot = SlotAsString(instruction->payload.slotPair.left);
      char* rightSlot = SlotAsString(instruction->payload.slotPair.right);
      printf("%u: CMP %s, %s\n", instruction->index, leftSlot, rightSlot);
      free(leftSlot);
      free(rightSlot);
    } break;

    case I_BINARY_OP:
    {
      const char* opString;

      switch (instruction->payload.binaryOp.operation)
      {
        case binary_op_i::op::ADD_I: opString = "+"; break;
        case binary_op_i::op::SUB_I: opString = "-"; break;
        case binary_op_i::op::MUL_I: opString = "*"; break;
        case binary_op_i::op::DIV_I: opString = "/"; break;
      }

      char* leftSlot = SlotAsString(instruction->payload.binaryOp.left);
      char* rightSlot = SlotAsString(instruction->payload.binaryOp.right);
      char* resultSlot = SlotAsString(instruction->payload.binaryOp.result);
      printf("%u: %s := %s %s %s\n", instruction->index, resultSlot, leftSlot, opString, rightSlot);
      free(leftSlot);
      free(rightSlot);
      free(resultSlot);
    } break;

    case I_CALL:
    {
      printf("%u: CALL %s\n", instruction->index, instruction->payload.function->name);
    } break;

    case I_LABEL:
    {
      printf("LABEL\n");
      // TODO: be more helpful to the poor sod debugginh here
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
      switch (instruction->payload.binaryOp.operation)
      {
        case binary_op_i::op::ADD_I: return "ADD_I";
        case binary_op_i::op::SUB_I: return "SUB_I";
        case binary_op_i::op::MUL_I: return "MUL_I";
        case binary_op_i::op::DIV_I: return "DIV_I";
      }
    }
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

  for (auto* it = function->slots.first;
       it;
       it = it->next)
  {
    slot_def* slot = **it;
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
        PRINT_SLOT("%s : VAR", slot->payload.variable->name);
      } break;

      case slot_type::TEMPORARY:
      {
        PRINT_SLOT("t%u : TMP", slot->payload.tag);
      } break;

      case slot_type::INT_CONSTANT:
      {
        PRINT_SLOT("%d : INT", slot->payload.i);
      } break;

      case slot_type::FLOAT_CONSTANT:
      {
        PRINT_SLOT("%f : FLOAT", slot->payload.f);
      } break;

      case slot_type::STRING_CONSTANT:
      {
        PRINT_SLOT("\\\"%s\\\" : STRING", slot->payload.string->string);
      } break;
    }

    slot->dotTag = i;
    i++;
  }

  // Emit the interferences between them
  for (auto* it = function->slots.first;
       it;
       it = it->next)
  {
    slot_def* slot = **it;

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
