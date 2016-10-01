/*
 * Copyright (C) 2016, Isaac Woods. All rights reserved.
 */

#include <air.hpp>
#include <cstdio>
#include <cstdlib>
#include <cassert>

static air_instruction* CreateInstruction(instruction_type type, ...)
{
  air_instruction* i = static_cast<air_instruction*>(malloc(sizeof(air_instruction)));
  i->type = type;
  i->next = nullptr;

  va_list args;
  va_start(args, type);

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
    } break;

    case I_JUMP:
    {
      i->payload.jump.cond                    = static_cast<jump_instruction_part::condition(va_arg(args, int));
      i->payload.jump.labelInstruction        = va_arg(args, air_instruction*);
    } break;

    default:
    {
      fprintf(stderr, "Unhandled AIR instruction type in CreateInstruction!\n");
      exit(1);
    }
  }

  va_end(args);
  return i;
}

#define PUSH(instruction) \
  tail->next = instruction; \
  tail = tail->next;

static void GenNodeAIR(air_instruction* tail, node* n)
{
  assert(n != nullptr);

  switch (n->type)
  {
    case BREAK_NODE:
    {
      instruction_label* label = static_cast<instruction_label*>(malloc(sizeof(instruction_label)));
      // TODO: find the end of the loop and attach the label

      PUSH(CreateInstruction(I_JUMP, UNCONDITIONAL, label));
    } break;

    case RETURN_NODE:
    {
      PUSH(CreateInstruction(I_LEAVE_STACK_FRAME));
      PUSH(CreateInstruction(I_RETURN));
    } break;

    case BINARY_OP_NODE:
    {

    } break;

    case PREFIX_OP_NODE:
    {

    } break;

    case VARIABLE_NODE:
    {

    };

    case CONDITION_NODE:
    {
    } break;

    case IF_NODE:
    {

    } break;

    case NUMBER_CONSTANT_NODE:
    {

    } break;

    case STRING_CONSTANT_NODE:
    {

    } break;

    case FUNCTION_CALL_NODE:
    {

    } break;

    case FUNCTION_CALL_NODE:
    {

    } break;

    case VARIABLE_ASSIGN_NODE:
    {
      slot* variableSlot = static_cast<slot*>(malloc(sizeof(slot)));
      // TODO

      slot* expressionSlot = static_cast<slot*>(malloc(sizeof(slot)));
      // TODO

      PUSH(CreateInstruction(I_MOV, variableSlot, expressionSlot));
    } break;

    default:
    {
      fprintf("Unhandled AST node type in GenNodeAIR!\n");
      exit(1);
    }
  }
}

void GenFunctionAIR(function_def* function)
{
  assert(function->code == nullptr);

  function->code = CreateInstruction(I_ENTER_STACK_FRAME);
  air_instruction* tail = function->code;

  for (node* n = function->ast;
       n;
       n = n->next)
  {
    GenNodeAIR(tail, n);
  }

  PUSH(CreateInstruction(I_LEAVE_STACK_FRAME));
}

void PrintInstruction(air_instruction* instruction)
{
  if (instruction->label)
  {
    // TODO(Isaac): print more information about the label
    printf("[LABEL]: ");
  }

  switch (instruction->type)
  {
    case I_ENTER_STACK_FRAME:
    {
      printf("ENTER_STACK_FRAME\n");
    } break;

    case I_LEAVE_STACK_FRAME:
    {
      printf("LEAVE_STACK_FRAME\n");
    } break;

    default:
    {
      fprintf(stderr, "Unhandled AIR instruction type in PrintInstruction!\n");
      exit(1);
    }
  }
}

static void FreeInstructionLabel(instruction_label* label)
{
  free(label);
}

void FreeInstruction(air_instruction* instruction)
{
  if (instruction->label)
  {
    FreeInstructionLabel(instruction->label);
  }

  switch (instruction->type)
  {
    case I_ENTER_STACK_FRAME:
    {
    } break;

    case I_LEAVE_STACK_FRAME:
    {
    } break;

    case I_RETURN:
    {
    } break;

    case I_JUMP:
    {
    } break;

    default:
    {
      fprintf(stderr, "Unhandled AIR instruction type in FreeInstruction!\n");
      exit(1);
    }
  }

  free(instruction);
}
