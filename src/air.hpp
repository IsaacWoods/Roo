/*
 * Copyright (C) 2016, Isaac Woods. All rights reserved.
 */

#pragma once

#include <common.hpp>

enum instruction_type
{
  I_ENTER_STACK_FRAME,
  I_LEAVE_STACK_FRAME,
  I_RETURN,
  I_JUMP,
  I_MOV,
  I_CMP,
  I_ADD,
  I_SUB,
  I_MUL,
  I_DIV,

  NUM_INSTRUCTIONS
};

struct instruction_label
{
  
};

struct slot
{
  enum slot_type
  {
    PARAM,
    LOCAL,
  } type;

  const variable_def* variableDef;
  unsigned int  tag;
};

struct jump_instruction
{
  enum condition
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
  } cond;

  const instruction_label* label;
};

struct slot_pair_instruction
{
  slot* a;
  slot* b;
};

struct air_instruction
{
  air_instruction*      next;

  instruction_type      type;
  instruction_label*    label;    // NOTE(Isaac): `nullptr` for unlabelled instructions

  union
  {
    jump_instruction        jump;
    slot_pair_instruction   slotPair;
  } payload;
};

void GenFunctionAIR(function_def* function);
void PrintInstruction(air_instruction* instruction);
void FreeInstruction(air_instruction* instruction);
