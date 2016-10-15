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
  I_NEGATE,

  I_NUM_INSTRUCTIONS
};

const char* GetInstructionName(instruction_type type);

struct instruction_label
{
  
};

struct slot
{
  enum slot_type
  {
    PARAM,
    LOCAL,
    INT_CONSTANT,
    FLOAT_CONSTANT
  } type;

  union
  {
    const variable_def* variableDef;
    int                 i;
    float               f;
  } payload;

  /*
   * -1 : signifies this slot holds a constant
   */
  signed int tag;
};

struct slot_link
{
  slot*       s;
  slot_link*  next;
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

struct mov_instruction
{
  slot* destination;
  slot* source;
};

struct slot_pair
{
  slot* right;
  slot* result;
};

struct slot_triple
{
  slot* left;
  slot* right;
  slot* result;
};

struct air_instruction
{
  air_instruction*      next;

  instruction_type      type;
  instruction_label*    label;    // NOTE(Isaac): `nullptr` for unlabelled instructions

  union instruction_payload
  {
    jump_instruction      jump;
    mov_instruction       mov;
    slot_pair             slotPair;
    slot_triple           slotTriple;
  } payload;
};

struct air_function
{
  air_instruction*  code;
  slot_link*        firstSlot;
};

void GenFunctionAIR(function_def* function);
void FreeAIRFunction(air_function* function);
void PrintInstruction(air_instruction* instruction);
