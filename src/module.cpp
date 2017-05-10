/*
 * Copyright (C) 2017, Isaac woods.
 * See LICENCE.md
 */

#include <module.hpp>

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
variable_def* Read<variable_def*>(FILE* f)
{
  variable_def* var = static_cast<variable_def*>(malloc(sizeof(variable_def)));

  var->name = Read<char*>(f);
  var->type.name = Read<char*>(f);
  var->type.isResolved = true;
  var->type.isMutable = static_cast<bool>(Read<uint8_t>(f));
  var->type.isReference = static_cast<bool>(Read<uint8_t>(f));
  var->type.isReferenceMutable = static_cast<bool>(Read<uint8_t>(f));

  uint32_t arraySize = Read<uint32_t>(f);
  var->type.isArray = (arraySize > 0u);
  var->type.isArraySizeResolved = true;
  var->type.arraySize = static_cast<unsigned int>(arraySize);

  return var;
}

template<>
vector<variable_def*> Read<vector<variable_def*>>(FILE* f)
{
  uint8_t numElements = Read<uint8_t>(f);
  vector<variable_def*> v;
  InitVector<variable_def*>(v, numElements);

  for (size_t i = 0u;
       i < numElements;
       i++)
  {
    Add<variable_def*>(v, Read<variable_def*>(f));
  }

  return v;
}

template<>
type_def* Read<type_def*>(FILE* f)
{
  type_def* type = static_cast<type_def*>(malloc(sizeof(type_def)));
  type->name = Read<char*>(f);
  type->members = Read<vector<variable_def*>>(f);
  type->errorState = CreateErrorState(TYPE_FILLING_IN, type);
  type->size = static_cast<unsigned int>(Read<uint32_t>(f));

  printf("Type name: %s\n", type->name);
  return type;
}

error_state ImportModule(const char* modulePath, parse_result& parse)
{
  error_state errorState = CreateErrorState(GENERAL_STUFF);
  FILE* f = fopen(modulePath, "rb");

  if (!f)
  {
    RaiseError(errorState, ERROR_MALFORMED_MODULE_INFO, modulePath, "Couldn't open file");
  }

  #define CONSUME(byte)\
    if (fgetc(f) != byte)\
      RaiseError(errorState, ERROR_MALFORMED_MODULE_INFO, modulePath, "Format not followed");

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
  }

  printf("Type count: %u\n", typeCount);
  printf("Code Thing count: %u\n", codeThingCount);

  for (size_t i = 0u;
       i < typeCount;
       i++)
  {
    Add<type_def*>(parse.types, Read<type_def*>(f));
  }

  for (size_t i = 0u;
       i < codeThingCount;
       i++)
  {
    Add<thing_of_code*>(parse.codeThings, Read<thing_of_code*>(f));
  }

  fclose(f);
  return errorState;
}

template<typename T>
void Emit(FILE* f, T value, error_state& /*errorState*/)
{
  fwrite(&value, sizeof(T), 1, f);
}

template<>
void Emit<char*>(FILE* f, char* value, error_state& errorState)
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
void Emit<variable_def*>(FILE* f, variable_def* value, error_state& errorState)
{
  assert(value->type.isResolved);
  Emit<char*>(f, value->name, errorState);
  Emit<char*>(f, value->type.name, errorState);
  Emit<uint8_t>(f, static_cast<uint8_t>(value->type.isMutable), errorState);
  Emit<uint8_t>(f, static_cast<uint8_t>(value->type.isReference), errorState);
  Emit<uint8_t>(f, static_cast<uint8_t>(value->type.isReferenceMutable), errorState);

  if (value->type.isArray)
  {
    assert(value->type.isArraySizeResolved);
    Emit<uint32_t>(f, static_cast<uint32_t>(value->type.arraySize), errorState);
  }
  else
  {
    Emit<uint32_t>(f, 0u, errorState);
  }
}

template<>
void Emit<vector<variable_def*>>(FILE* f, vector<variable_def*> value, error_state& errorState)
{
  Emit<uint8_t>(f, value.size, errorState);

  for (auto* it = value.head;
       it < value.tail;
       it++)
  {
    Emit<variable_def*>(f, *it, errorState);
  }
}

template<>
void Emit<type_def*>(FILE* f, type_def* value, error_state& errorState)
{
  Emit<char*>(f, value->name, errorState);
  Emit<vector<variable_def*>>(f, value->members, errorState);
  Emit<uint32_t>(f, static_cast<uint32_t>(value->size), errorState);
}

error_state ExportModule(const char* outputPath, parse_result& parse)
{
  error_state errorState = CreateErrorState(GENERAL_STUFF);
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

  uint32_t typeCount      = static_cast<uint32_t>(parse.types.size);
  uint32_t codeThingCount = static_cast<uint32_t>(parse.codeThings.size);

  fwrite(&typeCount,      sizeof(uint32_t), 1, f);
  fwrite(&codeThingCount, sizeof(uint32_t), 1, f);

  for (auto* it = parse.types.head;
       it < parse.types.tail;
       it++)
  {
    Emit<type_def*>(f, *it, errorState);
  }

  for (auto* it = parse.codeThings.head;
       it < parse.codeThings.tail;
       it++)
  {
  }

  fclose(f);
  return errorState;
}
