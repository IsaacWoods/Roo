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

TargetMachine::TargetMachine(const std::string& name, ParseResult& parse,
                                                      unsigned int numRegisters,
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
  ,intrinsicTypes{}
{
  intrinsicTypes[UNSIGNED_INT_INTRINSIC] = new TypeRef(GetTypeByName(parse, "uint"));
  intrinsicTypes[SIGNED_INT_INTRINSIC]   = new TypeRef(GetTypeByName(parse, "int"));
  intrinsicTypes[FLOAT_INTRINSIC]        = new TypeRef(GetTypeByName(parse, "float"));
  intrinsicTypes[BOOL_INTRINSIC]         = new TypeRef(GetTypeByName(parse, "bool"));
  intrinsicTypes[STRING_INTRINSIC]       = new TypeRef(GetTypeByName(parse, "string"));
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

  for (unsigned int i = 0u;
       i < NUM_INTRINSIC_OP_TYPES;
       i++)
  {
    delete intrinsicTypes[i];
  }
}
