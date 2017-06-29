/*
 * Copyright (C) 2017, Isaac woods.
 * See LICENCE.md
 */

#include <module.hpp>
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
char* Read<char*>(FILE* f)
{
  // NOTE(Isaac): The length includes the null-terminator
  uint8_t length = Read<uint8_t>(f);
  char* str = static_cast<char*>(malloc(sizeof(char) * length));

  for (size_t i = 0u;
       i < length;
       i++)
  {
    str[i] = fgetc(f);
  }

  return str;
}

template<>
VariableDef* Read<VariableDef*>(FILE* f)
{
  TypeRef type;
  char* name = Read<char*>(f);
  type.name = Read<char*>(f);
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
TypeDef* Read<TypeDef*>(FILE* f)
{
  char* name = Read<char*>(f);
  TypeDef* type = new TypeDef(name);
  type->members = Read<std::vector<VariableDef*>>(f);
  type->size = static_cast<unsigned int>(Read<uint32_t>(f));

  return type;
}

template<>
ThingOfCode* Read<ThingOfCode*>(FILE* f)
{
  ThingOfCode* thing;

  switch (Read<uint8_t>(f))
  {
    case 0u:
    {
      thing = new ThingOfCode(ThingOfCode::Type::FUNCTION, Read<char*>(f));
    } break;

    case 1u:
    {
      thing = new ThingOfCode(ThingOfCode::Type::OPERATOR, static_cast<token_type>(Read<uint32_t>(f)));
    } break;

    default:
    {
      // TODO: get the module info path from somewhere
      RaiseError(ERROR_MALFORMED_MODULE_INFO, "ModuleFileIdk", "ThingOfCode type encoding should be 0 or 1");
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

ErrorState ImportModule(const char* modulePath, ParseResult& parse)
{
  ErrorState errorState(ErrorState::Type::GENERAL_STUFF);
  FILE* f = fopen(modulePath, "rb");

  if (!f)
  {
    RaiseError(errorState, ERROR_MALFORMED_MODULE_INFO, modulePath, "Couldn't open file");
    return errorState;
  }

  #define CONSUME(byte)\
    if (fgetc(f) != byte)\
    {\
      RaiseError(errorState, ERROR_MALFORMED_MODULE_INFO, modulePath, "Format not followed");\
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
    RaiseError(errorState, ERROR_MALFORMED_MODULE_INFO, modulePath, "Unsupported version");
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
    parse.codeThings.push_back(Read<ThingOfCode*>(f));
  }

  fclose(f);
  return errorState;
}

template<typename T>
void Emit(FILE* f, T value, ErrorState& /*errorState*/)
{
  fwrite(&value, sizeof(T), 1, f);
}

template<>
void Emit<const char*>(FILE* f, const char* value, ErrorState& errorState)
{
  uint8_t length = static_cast<uint8_t>(strlen(value));
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
void Emit<VariableDef*>(FILE* f, VariableDef* value, ErrorState& errorState)
{
  Assert(value->type.isResolved, "Tried to emit module info for unresolved type of a VariableDef");
  Emit<const char*>(f, value->name, errorState);
  Emit<const char*>(f, (value->type.isResolved ? value->type.resolvedType->name : value->type.name.c_str()), errorState);
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
void Emit<std::vector<VariableDef*>>(FILE* f, std::vector<VariableDef*> value, ErrorState& errorState)
{
  Emit<uint8_t>(f, value.size(), errorState);

  for (VariableDef* variable : value)
  {
    Emit<VariableDef*>(f, variable, errorState);
  }
}

template<>
void Emit<TypeDef*>(FILE* f, TypeDef* value, ErrorState& errorState)
{
  Emit<const char*>(f, value->name, errorState);
  Emit<std::vector<VariableDef*>>(f, value->members, errorState);
  Emit<uint32_t>(f, static_cast<uint32_t>(value->size), errorState);
}

template<>
void Emit<ThingOfCode*>(FILE* f, ThingOfCode* thing, ErrorState& errorState)
{
  switch (thing->type)
  {
    case ThingOfCode::Type::FUNCTION:
    {
      Emit<uint8_t>(f, 0u, errorState);
      Emit<const char*>(f, thing->name, errorState);
      Emit<std::vector<VariableDef*>>(f, thing->params, errorState);
    } break;

    case ThingOfCode::Type::OPERATOR:
    {
      Emit<uint8_t>(f, 1u, errorState);
      Emit<uint32_t>(f, static_cast<uint32_t>(thing->op), errorState);
      Emit<std::vector<VariableDef*>>(f, thing->params, errorState);
    } break;
  }
}

ErrorState ExportModule(const char* outputPath, ParseResult& parse)
{
  ErrorState errorState(ErrorState::Type::GENERAL_STUFF);
  FILE* f = fopen(outputPath, "wb");

  if (!f)
  {
    RaiseError(errorState, ERROR_FAILED_TO_EXPORT_MODULE, outputPath, "Couldn't open file");
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

  for (ThingOfCode* code : parse.codeThings)
  {
    Emit<ThingOfCode*>(f, code, errorState);
  }

  fclose(f);
  return errorState;
}
