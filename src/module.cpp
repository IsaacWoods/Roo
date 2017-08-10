/*
 * Copyright (C) 2017, Isaac woods.
 * See LICENCE.md
 */

#include <module.hpp>
#include <string>
#include <cstring>

#define ROO_MOD_VERSION 0u

/*
 * XXX(Isaac): Resources allocated by these methods are expected to be managed by the caller
 */

template<typename T>
inline T Read(FILE* f)
{
  T retValue;
  fread(&retValue, sizeof(T), 1, f);
  return retValue;
}

template<>
std::string Read<std::string>(FILE* f)
{
  // NOTE(Isaac): The length includes the null-terminator
  uint8_t length = Read<uint8_t>(f);
  char* strBuffer = new char[length];

  for (size_t i = 0u;
       i < length;
       i++)
  {
    strBuffer[i] = fgetc(f);
  }

  std::string str(strBuffer);
  delete[] strBuffer;
  return str;
}

template<>
VariableDef* Read<VariableDef*>(FILE* f)
{
  TypeRef type;
  std::string name = Read<std::string>(f);
  type.name = Read<std::string>(f);
  type.isResolved = false;
  type.isMutable = static_cast<bool>(Read<uint8_t>(f));
  type.isReference = static_cast<bool>(Read<uint8_t>(f));
  type.isReferenceMutable = static_cast<bool>(Read<uint8_t>(f));
  type.arraySize = Read<uint32_t>(f);
  type.isArray = (type.arraySize > 0u);
  type.isArraySizeResolved = true;

  return new VariableDef(name, type, nullptr);
}

template<>
std::vector<VariableDef*> Read<std::vector<VariableDef*>>(FILE* f)
{
  std::vector<VariableDef*> vec;
  uint8_t numElements = Read<uint8_t>(f);

  for (size_t i = 0u;
       i < numElements;
       i++)
  {
    vec.push_back(Read<VariableDef*>(f));
  }

  return vec;
}

template<>
MemberDef* Read<MemberDef*>(FILE* f)
{
  TypeRef type;
  std::string name = Read<std::string>(f);
  type.name = Read<std::string>(f);
  type.isResolved = false;
  type.isMutable = static_cast<bool>(Read<uint8_t>(f));
  type.isReference = static_cast<bool>(Read<uint8_t>(f));
  type.isReferenceMutable = static_cast<bool>(Read<uint8_t>(f));
  type.arraySize = Read<uint32_t>(f);
  type.isArray = (type.arraySize > 0u);
  type.isArraySizeResolved = true;

  return new MemberDef(name, type, nullptr);
}

template<>
std::vector<MemberDef*> Read<std::vector<MemberDef*>>(FILE* f)
{
  std::vector<MemberDef*> vec;
  uint8_t numElements = Read<uint8_t>(f);

  for (size_t i = 0u;
       i < numElements;
       i++)
  {
    vec.push_back(Read<MemberDef*>(f));
  }

  return vec;
}

template<>
TypeDef* Read<TypeDef*>(FILE* f)
{
  TypeDef* type = new TypeDef(Read<std::string>(f));
  type->members = Read<std::vector<MemberDef*>>(f);
  type->size = static_cast<unsigned int>(Read<uint32_t>(f));

  return type;
}

template<>
CodeThing* Read<CodeThing*>(FILE* f)
{
  CodeThing* thing;

  switch (Read<uint8_t>(f))
  {
    case 0u:
    {
      thing = new FunctionThing(Read<std::string>(f));
    } break;

    case 1u:
    {
      thing = new OperatorThing(static_cast<TokenType>(Read<uint32_t>(f)));
    } break;

    default:
    {
      // TODO: get the module info path from somewhere
      RaiseError(ERROR_MALFORMED_MODULE_INFO, "ModuleFileIdk", "CodeThing type encoding should be 0 or 1");
      return nullptr;
    } break;
  }

  thing->params = Read<std::vector<VariableDef*>>(f);

  /*
   * Even if it is defined in Roo in the other module, we're linking against it here and so it should be considered
   * a prototype function, as if it were defined in C or assembly or whatever.
   */
  thing->attribs.isPrototype = true;

  return thing;
}

ErrorState* ImportModule(const std::string& modulePath, ParseResult& parse)
{
  ErrorState* errorState = new ErrorState();
  FILE* f = fopen(modulePath.c_str(), "rb");

  if (!f)
  {
    RaiseError(errorState, ERROR_MALFORMED_MODULE_INFO, modulePath.c_str(), "Couldn't open file");
    return errorState;
  }

  #define CONSUME(byte)\
    if (fgetc(f) != byte)\
    {\
      RaiseError(errorState, ERROR_MALFORMED_MODULE_INFO, modulePath.c_str(), "Format not followed");\
      return errorState;\
    }

  CONSUME(0x7F);
  CONSUME('R');
  CONSUME('O');
  CONSUME('O');

  uint8_t   version         = Read<uint8_t>(f);
  uint32_t  typeCount       = Read<uint32_t>(f);
  uint32_t  codeThingCount  = Read<uint32_t>(f);

  if (version != ROO_MOD_VERSION)
  {
    RaiseError(errorState, ERROR_MALFORMED_MODULE_INFO, modulePath.c_str(), "Unsupported version");
    return errorState;
  }

  for (size_t i = 0u;
       i < typeCount;
       i++)
  {
    parse.types.push_back(Read<TypeDef*>(f));
  }

  for (size_t i = 0u;
       i < codeThingCount;
       i++)
  {
    parse.codeThings.push_back(Read<CodeThing*>(f));
  }

  fclose(f);
  return errorState;
}

template<typename T>
void Emit(FILE* f, const T& value, ErrorState* /*errorState*/)
{
  fwrite(&value, sizeof(T), 1, f);
}

template<>
void Emit<std::string>(FILE* f, const std::string& value, ErrorState* errorState)
{
  uint8_t length = value.size();
  Emit<uint8_t>(f, length+1u, errorState);  // We also need to include a null-terminator in the length

  for (size_t i = 0u;
       i < length;
       i++)
  {
    Emit<uint8_t>(f, static_cast<char>(value[i]), errorState);
  }
  Emit<uint8_t>(f, '\0', errorState);
}

template<>
void Emit<VariableDef*>(FILE* f, VariableDef* const& value, ErrorState* errorState)
{
  Assert(value->type.isResolved, "Tried to emit module info for unresolved type of a VariableDef");
  Emit<std::string>(f, value->name, errorState);
  Emit<std::string>(f, (value->type.isResolved ? value->type.resolvedType->name : value->type.name.c_str()), errorState);
  Emit<uint8_t>(f, static_cast<uint8_t>(value->type.isMutable), errorState);
  Emit<uint8_t>(f, static_cast<uint8_t>(value->type.isReference), errorState);
  Emit<uint8_t>(f, static_cast<uint8_t>(value->type.isReferenceMutable), errorState);

  if (value->type.isArray)
  {
    Assert(value->type.isArraySizeResolved, "Tried to emit module info for unresolved array size");
    Emit<uint32_t>(f, static_cast<uint32_t>(value->type.arraySize), errorState);
  }
  else
  {
    Emit<uint32_t>(f, 0u, errorState);
  }
}

template<>
void Emit<std::vector<VariableDef*>>(FILE* f, const std::vector<VariableDef*>& value, ErrorState* errorState)
{
  Emit<uint8_t>(f, value.size(), errorState);

  for (VariableDef* variable : value)
  {
    Emit<VariableDef*>(f, variable, errorState);
  }
}

template<>
void Emit<MemberDef*>(FILE* f, MemberDef* const& value, ErrorState* errorState)
{
  Assert(value->type.isResolved, "Tried to emit module info for unresolved type of a MemberDef");
  Emit<std::string>(f, value->name, errorState);
  Emit<std::string>(f, (value->type.isResolved ? value->type.resolvedType->name : value->type.name.c_str()), errorState);
  Emit<uint8_t>(f, static_cast<uint8_t>(value->type.isMutable), errorState);
  Emit<uint8_t>(f, static_cast<uint8_t>(value->type.isReference), errorState);
  Emit<uint8_t>(f, static_cast<uint8_t>(value->type.isReferenceMutable), errorState);

  if (value->type.isArray)
  {
    Assert(value->type.isArraySizeResolved, "Tried to emit module info for unresolved array size");
    Emit<uint32_t>(f, static_cast<uint32_t>(value->type.arraySize), errorState);
  }
  else
  {
    Emit<uint32_t>(f, 0u, errorState);
  }
}

template<>
void Emit<std::vector<MemberDef*>>(FILE* f, const std::vector<MemberDef*>& value, ErrorState* errorState)
{
  Emit<uint8_t>(f, value.size(), errorState);

  for (MemberDef* variable : value)
  {
    Emit<MemberDef*>(f, variable, errorState);
  }
}

template<>
void Emit<TypeDef*>(FILE* f, TypeDef* const& value, ErrorState* errorState)
{
  Emit<std::string>(f, value->name, errorState);
  Emit<std::vector<MemberDef*>>(f, value->members, errorState);
  Emit<uint32_t>(f, static_cast<uint32_t>(value->size), errorState);
}

template<>
void Emit<CodeThing*>(FILE* f, CodeThing* const& thing, ErrorState* errorState)
{
  switch (thing->type)
  {
    case CodeThing::Type::FUNCTION:
    {
      Emit<uint8_t>(f, 0u, errorState);
      Emit<std::string>(f, dynamic_cast<const FunctionThing*>(thing)->name, errorState);
      Emit<std::vector<VariableDef*>>(f, thing->params, errorState);
    } break;

    case CodeThing::Type::OPERATOR:
    {
      Emit<uint8_t>(f, 1u, errorState);
      Emit<uint32_t>(f, static_cast<uint32_t>(dynamic_cast<const OperatorThing*>(thing)->token), errorState);
      Emit<std::vector<VariableDef*>>(f, thing->params, errorState);
    } break;
  }
}

ErrorState* ExportModule(const std::string& outputPath, ParseResult& parse)
{
  ErrorState* errorState = new ErrorState();
  FILE* f = fopen(outputPath.c_str(), "wb");

  if (!f)
  {
    RaiseError(errorState, ERROR_FAILED_TO_EXPORT_MODULE, outputPath.c_str(), "Couldn't open file");
  }

  #define EMIT(byte) fputc(byte, f);

  EMIT(0x7F);
  EMIT('R');
  EMIT('O');
  EMIT('O');
  EMIT(ROO_MOD_VERSION);

  uint32_t typeCount      = static_cast<uint32_t>(parse.types.size());
  uint32_t codeThingCount = static_cast<uint32_t>(parse.codeThings.size());

  fwrite(&typeCount,      sizeof(uint32_t), 1, f);
  fwrite(&codeThingCount, sizeof(uint32_t), 1, f);

  for (TypeDef* type : parse.types)
  {
    Emit<TypeDef*>(f, type, errorState);
  }

  for (CodeThing* code : parse.codeThings)
  {
    Emit<CodeThing*>(f, code, errorState);
  }

  fclose(f);
  return errorState;
}
