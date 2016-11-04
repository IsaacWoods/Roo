/*
 * Copyright (C) 2016, Isaac Woods. All rights reserved.
 */

#pragma once

#include <common.hpp>
#include <ir.hpp>

// NOTE(Isaac): Could be considered bad practice, but stops us needing a gazillion dynamic arrays everywhere??
#define MAX_INTERFERENCES 32u

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
  I_CALL,

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
    PARAM,            // NOTE(Isaac): `variable` field of `payload` is valid
    LOCAL,            // NOTE(Isaac): `variable` field of `payload` is valid
    INTERMEDIATE,     // NOTE(Isaac): `payload` is undefined
    INT_CONSTANT,     // NOTE(Isaac): `i` field of `payload` is valid
    FLOAT_CONSTANT,   // NOTE(Isaac): `f` field of `payload` is valid
  } type;

  union
  {
    variable_def* variable;
    int           i;
    float         f;
  } payload;

  /*
   * -1 : signifies this slot holds a constant
   */
  signed int tag;

  unsigned int numInterferences;
  slot* interferences[MAX_INTERFERENCES];

  /*
   * -1: signifies that this slot has not been colored
   */
  bool shouldBeColored;
  signed int color;

#if 1
  unsigned int dotTag;
#endif
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
  slot* dest;
  slot* src;
};

struct slot_pair
{
  slot* left;
  slot* right;
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
    slot*                 s;
    slot_pair             slotPair;
    slot_triple           slotTriple;
    function_def*         function;
  } payload;
};

struct air_function
{
  air_instruction*    code;
  air_instruction*    tail;
  linked_list<slot*>  slots;
  int                 numIntermediates;
};

void GenFunctionAIR(codegen_target& target, function_def* function);
void PrintInstruction(air_instruction* instruction);
void CreateInterferenceDOT(air_function* function, const char* functionName);
