/*
 * Copyright (C) 2016, Isaac Woods. All rights reserved.
 */

#pragma once

enum reg
{
  RAX,
  RBX,
  RCX,
  RDX,
  
  RSI,
  RDI,
  RBP,
  RSP,

  R8,
  R9,
  R10,
  R11,
  R12,
  R13,
  R14,
  R15,

  NUM_REGISTERS
};

const char* GetRegisterName(reg r);

/*
 * The state of a register at a particular point in the program.
 */
struct register_state
{
  enum register_usage
  {
    FREE,
    IN_USE,
    UNUSABLE
  } usage;

  /*
   * The variable occupying this register if it's in use.
   * Null otherwise
   */
  variable_def* variable;
};

struct register_state_set
{
  const char*     tag;
  register_state  registers[NUM_REGISTERS];

  register_state& operator[](reg r)
  {
    return registers[r];
  }
};

void InitRegisterStateSet(register_state_set& set, const char* tag = nullptr);
void PrintRegisterStateSet(register_state_set& set);

struct register_usage_node
{
  enum user_type
  {
    PARAMETER,
    LOCAL
  }               type;

  variable_def*   variable;
  reg             color;
};

struct register_graph_interference
{
  register_usage_node* a;
  register_usage_node* b;

  register_graph_interference* next;
};

struct register_allocation
{
  register_usage_node* userList;
  register_graph_interference* firstInterference;
};

void AllocateRegisters(register_allocation& allocation, function_def* function);
