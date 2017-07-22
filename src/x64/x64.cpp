/*
 * Copyright (C) 2017, Isaac Woods.
 * See LICENCE.md
 */

#include <x64/x64.hpp>

TargetMachine::TargetMachine()
  :name("x64_elf")
  ,numRegisters(16u)
  ,registerSet(new RegisterDef[numRegisters])
  ,numGeneralRegisters(14u)
  ,generalRegisterSize(8u)
  ,numIntParamColors(6u)
  ,intParamColors(new unsigned int[numIntParamColors])
  ,functionReturnColor(RAX)
{
  intParamColors[0u] = RDI;
  intParamColors[1u] = RSI;
  intParamColors[2u] = RDX;
  intParamColors[3u] = RCX;
  intParamColors[4u] = R8;
  intParamColors[5u] = R9;

#define REGISTER(index, name, usage, modRMOffset)\
    registerSet[index] = RegisterDef{usage, name, static_cast<RegisterPimpl*>(malloc(sizeof(RegisterPimpl)))};\
    registerSet[index].pimpl->opcodeOffset = modRMOffset;

  REGISTER(RAX, "RAX", RegisterDef::Usage::GENERAL, 0u);
  REGISTER(RBX, "RBX", RegisterDef::Usage::GENERAL, 3u);
  REGISTER(RCX, "RCX", RegisterDef::Usage::GENERAL, 1u);
  REGISTER(RDX, "RDX", RegisterDef::Usage::GENERAL, 2u);
  REGISTER(RSP, "RSP", RegisterDef::Usage::SPECIAL, 4u);
  REGISTER(RBP, "RBP", RegisterDef::Usage::SPECIAL, 5u);
  REGISTER(RSI, "RSI", RegisterDef::Usage::GENERAL, 6u);
  REGISTER(RDI, "RDI", RegisterDef::Usage::GENERAL, 7u);
  REGISTER(R8 , "R8" , RegisterDef::Usage::GENERAL, 8u);
  REGISTER(R9 , "R9" , RegisterDef::Usage::GENERAL, 9u);
  REGISTER(R10, "R10", RegisterDef::Usage::GENERAL, 10u);
  REGISTER(R11, "R11", RegisterDef::Usage::GENERAL, 11u);
  REGISTER(R12, "R12", RegisterDef::Usage::GENERAL, 12u);
  REGISTER(R13, "R13", RegisterDef::Usage::GENERAL, 13u);
  REGISTER(R14, "R14", RegisterDef::Usage::GENERAL, 14u);
  REGISTER(R15, "R15", RegisterDef::Usage::GENERAL, 15u);
}

TargetMachine::~TargetMachine()
{
  for (unsigned int i = 0u;
       i < numRegisters;
       i++)
  {
    delete registerSet[i].pimpl;
  }

  delete[] registerSet;
  delete[] intParamColors;
}
