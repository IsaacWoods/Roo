/*
 * Copyright (C) 2017, Isaac Woods.
 * See LICENCE.md
 */

#pragma once

#include <common.hpp>
#include <ir.hpp>

enum instruction_type
{
  I_RETURN,
  I_JUMP,
  I_MOV,
  I_CMP,
  I_BINARY_OP,
  I_INC,
  I_DEC,
  I_CALL,

  /*
   * NOTE(Isaac): This doesn't correspond to an actual instruction, but marks positions we can jump to in the code
   */
  I_LABEL,

  I_NUM_INSTRUCTIONS
};

struct instruction_label
{
  /*
   * Initially 0x00.
   * Labels are assumed to be local to the function, so this is an offset from the symbol of the current function.
   */
  uint64_t offset;
};

struct jump_i
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

struct mov_i
{
  slot_def* dest;
  slot_def* src;
};

struct binary_op_i
{
  enum op
  {
    ADD_I,
    SUB_I,
    MUL_I,
    DIV_I
  } operation;

  slot_def* left;
  slot_def* right;
  slot_def* result;
};

struct slot_pair
{
  slot_def* left;
  slot_def* right;
};

struct slot_triple
{
  slot_def* left;
  slot_def* right;
  slot_def* result;
};

struct air_instruction
{
  unsigned int          index;
  air_instruction*      next;
  instruction_type      type;

  union
  {
    jump_i              jump;
    mov_i               mov;
    binary_op_i         binaryOp;
    slot_def*           slot;
    slot_pair           slotPair;
    slot_triple         slotTriple;
    thing_of_code*      call;
    instruction_label*  label;
  };
};

#ifdef OUTPUT_DOT
void OutputInterferenceDOT(thing_of_code* code);
#endif

unsigned int GetInstructionCost(air_instruction* instruction);
unsigned int GetCodeCost(thing_of_code* code);
bool IsColorInUseAtPoint(thing_of_code* code, air_instruction* instruction, signed int color);
void GenerateAIR(codegen_target& target, thing_of_code* code);
const char* GetInstructionName(air_instruction* instruction);
void PrintInstruction(air_instruction* instruction);
