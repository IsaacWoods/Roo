/*
 * Copyright (C) 2017, Isaac woods.
 * See LICENCE.md
 */

#include <module.hpp>

#define ROO_MOD_VERSION       0u
#define ROO_MOD_LITTLE_ENDIAN 0u
#define ROO_MOD_BIG_ENDIAN    1u

/*
 * XXX(Isaac): Resources allocated by these methods are expected to be managed by the caller
 */

template<typename T>
inline T ReadField(FILE* f)
{
  T retValue;
  fread(&retValue, sizeof(T), 1, f);
  return retValue;
}

template<>
char* ReadField<char*>(FILE* f)
{
  // NOTE(Isaac): The length includes the null-terminator
  uint8_t length = ReadField<uint8_t>(f);
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
variable_def* ReadField<variable_def*>(FILE* f)
{
  variable_def* var = static_cast<variable_def*>(malloc(sizeof(variable_def)));

  var->name = ReadField<char*>(f);
  var->type.name = ReadField<char*>(f);
  var->type.isResolved = true;
  var->type.isMutable = static_cast<bool>(ReadField<uint8_t>(f));
  var->type.isReference = static_cast<bool>(ReadField<uint8_t>(f));
  var->type.isReferenceMutable = static_cast<bool>(ReadField<uint8_t>(f));

  uint32_t arraySize = ReadField<uint32_t>(f);
  var->type.isArray = (arraySize > 0u);
  var->type.isArraySizeResolved = true;
  var->type.arraySize = static_cast<unsigned int>(arraySize);

  return var;
}

template<>
vector<variable_def*> ReadField<vector<variable_def*>>(FILE* f)
{
  uint8_t numElements = ReadField<uint8_t>(f);
  vector<variable_def*> v;
  InitVector<variable_def*>(v, numElements);

  for (size_t i = 0u;
       i < numElements;
       i++)
  {
    Add<variable_def*>(v, ReadField<variable_def*>(f));
  }

  return v;
}

template<>
type_def* ReadField<type_def*>(FILE* f)
{
  type_def* type = static_cast<type_def*>(malloc(sizeof(type_def)));
  InitVector<variable_def*>(type->members);
  type->errorState = CreateErrorState(TYPE_FILLING_IN, type);

  type->name = ReadField<char*>(f);
  printf("Type name: %s\n", type->name);

  return type;
}

error_state ImportModule(parse_result& parse, const char* modulePath)
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

  uint8_t version         = ReadField<uint8_t>(f);
  uint8_t endianess       = ReadField<uint8_t>(f);

  uint32_t typeCount      = ReadField<uint32_t>(f);
  uint32_t functionCount  = ReadField<uint32_t>(f);
  uint32_t operatorCount  = ReadField<uint32_t>(f);

  if (version != ROO_MOD_VERSION)
  {
    RaiseError(errorState, ERROR_MALFORMED_MODULE_INFO, modulePath, "Unsupported version");
  }

  if (endianess != ROO_MOD_LITTLE_ENDIAN)
  {
    RaiseError(errorState, ERROR_MALFORMED_MODULE_INFO, modulePath, "Platform expects little-endian");
  }

  printf("Type count: %u\n", typeCount);
  printf("Function count: %u\n", functionCount);
  printf("Operator count: %u\n", operatorCount);

  for (size_t i = 0u;
       i < typeCount;
       i++)
  {
    Add<type_def*>(parse.types, ReadField<type_def*>(f));
  }

  fclose(f);
  return errorState;
}
