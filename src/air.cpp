/*
 * Copyright (C) 2016, Isaac Woods. All rights reserved.
 */

#include <air.hpp>
#include <cstdio>
#include <cstdlib>

air_instruction* CreateInstruction(instruction_type type, ...)
{
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

    default:
    {
      fprintf(stderr, "Unhandled AIR instruction type in CreateInstruction!\n");
      exit(1);
    }
  }

  return i;
}

void PrintInstruction(air_instruction* instruction)
{
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

void FreeInstruction(air_instruction* instruction)
{
  switch (instruction->type)
  {
    case I_ENTER_STACK_FRAME:
    {
    } break;

    case I_LEAVE_STACK_FRAME:
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
