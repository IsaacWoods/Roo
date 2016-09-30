/*
 * Copyright (C) 2016, Isaac Woods. All rights reserved.
 */

#pragma once

#include <common.hpp>

enum instruction_type
{
  I_ENTER_STACK_FRAME,
  I_LEAVE_STACK_FRAME,

  NUM_INSTRUCTIONS
};

struct air_instruction
{
  instruction_type  type;

  air_instruction*  next;
};

void GenerateAIRForFunction(function_def* function);
void PrintInstruction(air_instruction* instruction);
void FreeInstruction(air_instruction* instruction);
