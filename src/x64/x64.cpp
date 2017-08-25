/*
 * Copyright (C) 2017, Isaac Woods.
 * See LICENCE.md
 */

#include <x64/x64.hpp>

RegisterDef_x64::RegisterDef_x64(BaseRegisterDef::Usage usage, const std::string& name, uint8_t opcodeOffset)
  :BaseRegisterDef(usage, name)
  ,opcodeOffset(opcodeOffset)
{
}

TargetMachine_x64::TargetMachine_x64(ParseResult& parse)
  :TargetMachine("x64_elf", parse,
                            16u /* numRegisters        */,
                            14u /* numGeneralRegisters */,
                            8u  /* generalRegisterSize */,
                            6u  /* numIntParamColors   */,
                            RAX /* functionReturnColor */)
{
  intParamColors[0u] = RDI;
  intParamColors[1u] = RSI;
  intParamColors[2u] = RDX;
  intParamColors[3u] = RCX;
  intParamColors[4u] = R8;
  intParamColors[5u] = R9;

  registerSet[RAX] = new RegisterDef_x64(BaseRegisterDef::Usage::GENERAL, "RAX", 0u);
  registerSet[RBX] = new RegisterDef_x64(BaseRegisterDef::Usage::GENERAL, "RBX", 3u);
  registerSet[RCX] = new RegisterDef_x64(BaseRegisterDef::Usage::GENERAL, "RCX", 1u);
  registerSet[RDX] = new RegisterDef_x64(BaseRegisterDef::Usage::GENERAL, "RDX", 2u);
  registerSet[RSP] = new RegisterDef_x64(BaseRegisterDef::Usage::SPECIAL, "RSP", 4u);
  registerSet[RBP] = new RegisterDef_x64(BaseRegisterDef::Usage::SPECIAL, "RBP", 5u);
  registerSet[RSI] = new RegisterDef_x64(BaseRegisterDef::Usage::GENERAL, "RSI", 6u);
  registerSet[RDI] = new RegisterDef_x64(BaseRegisterDef::Usage::GENERAL, "RDI", 7u);
  registerSet[R8 ] = new RegisterDef_x64(BaseRegisterDef::Usage::GENERAL, "R8" , 8u);
  registerSet[R9 ] = new RegisterDef_x64(BaseRegisterDef::Usage::GENERAL, "R9" , 9u);
  registerSet[R10] = new RegisterDef_x64(BaseRegisterDef::Usage::GENERAL, "R10", 10u);
  registerSet[R11] = new RegisterDef_x64(BaseRegisterDef::Usage::GENERAL, "R11", 11u);
  registerSet[R12] = new RegisterDef_x64(BaseRegisterDef::Usage::GENERAL, "R12", 12u);
  registerSet[R13] = new RegisterDef_x64(BaseRegisterDef::Usage::GENERAL, "R13", 13u);
  registerSet[R14] = new RegisterDef_x64(BaseRegisterDef::Usage::GENERAL, "R14", 14u);
  registerSet[R15] = new RegisterDef_x64(BaseRegisterDef::Usage::GENERAL, "R15", 15u);
}

InstructionPrecolorer* TargetMachine_x64::CreateInstructionPrecolorer()
{
  return new InstructionPrecolorer_x64();
}

CodeGenerator* TargetMachine_x64::CreateCodeGenerator(ElfFile& file)
{
  return new CodeGenerator_x64(this, file);
}
