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
  I_BINARY_OP,
  I_CALL,

  /*
   * NOTE(Isaac): This doesn't correspond to an actual instruction, but marks positions we can jump to in the code
   */
  I_LABEL,

  I_NUM_INSTRUCTIONS
};

/*
 * Represents the range a slot is live for, in terms of instruction indices.
 * NOTE(Isaac): `lastUser` should be or should appear after `definer`
 */
struct air_instruction;
struct live_range
{
  air_instruction* definer;
  air_instruction* lastUser;
};

struct slot
{
  enum slot_type
  {
    VARIABLE,         // NOTE(Isaac): either a local or a parameter. `variable` field of `payload` is valid.
    INTERMEDIATE,     // NOTE(Isaac): `payload` is undefined

    /*
     * Used to store the parameters of function calls in the correct registers according to the ABI
     * NOTE(Isaac): comes precolored
     */
    IN_PARAM,

    INT_CONSTANT,     // NOTE(Isaac): `i` field of `payload` is valid
    FLOAT_CONSTANT,   // NOTE(Isaac): `f` field of `payload` is valid
    STRING_CONSTANT,  // NOTE(Isaac): `string` field of `payload` is valid
  } type;

  union
  {
    variable_def*     variable;
    int               i;
    float             f;
    string_constant*  string;
  } payload;

  /*
   * -1 : signifies this slot holds a constant
   */
  signed int tag;
  live_range range;

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

struct instruction_label
{
  /*
   * Initially 0x00.
   * Labels are assumed to be local to the function, so this is an offset from the symbol of the current function.
   */
  uint64_t offset;
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

struct binary_op_instruction
{
  enum op
  {
    ADD,
    SUB,
    MUL,
    DIV
  } operation;

  slot* left;
  slot* right;
  slot* result;
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
  unsigned int          index;
  air_instruction*      next;
  instruction_type      type;

  union instruction_payload
  {
    jump_instruction      jump;
    mov_instruction       mov;
    binary_op_instruction binaryOp;
    slot*                 s;
    slot_pair             slotPair;
    slot_triple           slotTriple;
    function_def*         function;
    instruction_label*    label;
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
const char* GetInstructionName(air_instruction* instruction);
void PrintInstruction(air_instruction* instruction);
void CreateInterferenceDOT(air_function* function, const char* functionName);
