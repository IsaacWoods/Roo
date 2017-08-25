/*
 * Copyright (C) 2017, Isaac Woods.
 * See LICENCE.md
 */

#include <target.hpp>
#include <codegen.hpp>
#include <elf/elf.hpp>

BaseRegisterDef::BaseRegisterDef(Usage usage, const std::string& name)
  :usage(usage)
  ,name(name)
{
}

TargetMachine::TargetMachine(const std::string& name, unsigned int numRegisters,
                                                      unsigned int numGeneralRegisters,
                                                      unsigned int generalRegisterSize,
                                                      unsigned int numIntParamColors,
                                                      unsigned int functionReturnColor)
  :name(name)
  ,numRegisters(numRegisters)
  ,registerSet(new BaseRegisterDef*[numRegisters])
  ,numGeneralRegisters(numGeneralRegisters)
  ,generalRegisterSize(generalRegisterSize)
  ,numIntParamColors(numIntParamColors)
  ,intParamColors(new unsigned int[numIntParamColors])
  ,functionReturnColor(functionReturnColor)
{
}

TargetMachine::~TargetMachine()
{
  for (unsigned int i = 0u;
       i < numRegisters;
       i++)
  {
    delete registerSet[i];
  }

  delete[] registerSet;
  delete[] intParamColors;
}
