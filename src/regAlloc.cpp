/*
 * Copyright (C) 2016, Isaac Woods. All rights reserved.
 */

#include <regAlloc.hpp>
#include <cstdio>
#include <cstdlib>

const char* GetRegisterName(reg r)
{
  switch (r)
  {
    case RAX:
      return "rax";
    case RBX:
      return "rbx";
    case RCX:
      return "rcx";
    case RDX:
      return "rdx";
    case RSI:
      return "rsi";
    case RDI:
      return "rdi";
    case RBP:
      return "rbp";
    case RSP:
      return "rsp";
    case R8:
      return "r8";
    case R9:
      return "r9";
    case R10:
      return "r10";
    case R11:
      return "r11";
    case R12:
      return "r12";
    case R13:
      return "r13";
    case R14:
      return "r14";
    case R15:
      return "r15";
    default:
      return "[INVALID REGISTER]";
  }

  fprintf(stderr, "Unhandled register in GetRegisterName!\n");
  exit(1);
}

void InitRegisterStateSet(register_state_set& set, const char* tag)
{
  set.tag = tag;

  for (unsigned int i = 0u;
       i < NUM_REGISTERS;
       i++)
  {
    register_state& r = set[static_cast<reg>(i)];
    r.usage = register_state::register_usage::FREE;
    r.variable = nullptr;
  }

  set[RBP].usage           = register_state::register_usage::UNUSABLE;
  set[RSP].usage           = register_state::register_usage::UNUSABLE;
}

void PrintRegisterStateSet(register_state_set& set)
{
  printf("/ %20s \\\n", (set.tag ? set.tag : "UNTAGGED"));
  printf("|----------------------|\n");
  
  for (unsigned int i = 0u;
       i < NUM_REGISTERS;
       i++)
  {
    register_state& r = set[static_cast<reg>(i)];
    printf("| %3s     - %10s |\n", GetRegisterName(static_cast<reg>(i)),
        ((r.usage == register_state::register_usage::FREE) ? "FREE" :
        ((r.usage == register_state::register_usage::IN_USE) ? "IN USE" : "UNUSABLE")));
  }

  printf("\\----------------------/\n");
}

void AllocateRegisters(register_allocation& allocation, function_def* function)
{
  unsigned int numRegisterUsers = 0u;

  for (variable_def* param = function->firstParam;
       param;
       param = param->next)
  {
    numRegisterUsers++;
  }

  for (variable_def* local = function->firstLocal;
       local;
       local = local->next)
  {
    numRegisterUsers++;
  }

  printf("Register users: %u\n", numRegisterUsers);

  allocation.userList = static_cast<register_usage_node*>(malloc(sizeof(register_usage_node) * numRegisterUsers));

}
