/*
 * Copyright (C) 2016, Isaac Woods. All rights reserved.
 */

#include <error.hpp>
#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include <common.hpp>
#include <parsing.hpp>
#include <ir.hpp>
#include <ast.hpp>

enum error_level
{
  NOTE,
  WARNING,
  ERROR,
  ICE,
};

enum poison_strategy
{
  DO_NOTHING,
  TO_END_OF_STATEMENT,
  TO_END_OF_ATTRIBUTE,
  TO_END_OF_BLOCK,
  TO_END_OF_STRING,
  GIVE_UP
};

struct error_def
{
  error_level level;
  poison_strategy poisonStrategy;
  const char* messageFmt;
};

error_def errors[NUM_ERRORS] = {};

__attribute__((constructor))
void InitErrorDefs()
{
#define N(tag, msg)             errors[tag] = error_def{error_level::NOTE, poison_strategy::DO_NOTHING, msg};
#define W(tag, msg)             errors[tag] = error_def{error_level::WARNING, poison_strategy::DO_NOTHING, msg};
#define E(tag, poisoning, msg)  errors[tag] = error_def{error_level::ERROR, poison_strategy::poisoning, msg};
#define I(tag, msg)             errors[tag] = error_def{error_level::ICE, poison_strategy::GIVE_UP, msg};

  N(NOTE_IGNORED_ELF_SECTION,                             "Ignored section in ELF relocatable: %s");

  W(WARNING_FOUND_TAB,                                    "Found a tab; their use is discouraged in Roo");

  E(ERROR_EXPECTED,                 TO_END_OF_STATEMENT,  "Expected %s");
  E(ERROR_EXPECTED_BUT_GOT,         TO_END_OF_STATEMENT,  "Expected %s but got %s instead");
  E(ERROR_UNEXPECTED,               TO_END_OF_STATEMENT,  "Unexpected token in %s position: %s");
  E(ERROR_ILLEGAL_ATTRIBUTE,        TO_END_OF_ATTRIBUTE,  "Unrecognised attribute '%s'");
  E(ERROR_UNDEFINED_VARIABLE,       TO_END_OF_STATEMENT,  "Failed to resolve variable called '%s'");
  E(ERROR_UNDEFINED_FUNCTION,       TO_END_OF_STATEMENT,  "Failed to resolve function called '%s'");
  E(ERROR_UNDEFINED_TYPE,           TO_END_OF_STATEMENT,  "Failed to resolve type with the name '%s'");
  E(ERROR_MISSING_OPERATOR,         TO_END_OF_STATEMENT,  "Can't find %s operator for operands of type '%s' and '%s'");
  E(ERROR_INCOMPATIBLE_ASSIGN,      TO_END_OF_STATEMENT,  "Can't assign a '%s' to a variable of type '%s'");
  E(ERROR_INVALID_OPERATOR,         TO_END_OF_BLOCK,      "Can't overload operator with token %s");
  E(ERROR_INVALID_OPERATOR,         TO_END_OF_STATEMENT,  "Can't overload operator with token %s");
  E(ERROR_NO_START_SYMBOL,          GIVE_UP,              "No _start symbol (is this a freestanding environment?\?)");
  E(ERROR_INVALID_EXECUTABLE,       GIVE_UP,              "Unable to create executable at path: %s");
  E(ERROR_UNRESOLVED_SYMBOL,        DO_NOTHING,           "Failed to resolve symbol: %s");
  E(ERROR_NO_PROGRAM_NAME,          GIVE_UP,              "A program name must be specified using the '#[Name(...)]' attribute");
  E(ERROR_WEIRD_LINKED_OBJECT,      DO_NOTHING,           "'%s' is not a valid ELF64 relocatable");
  E(ERROR_NO_ENTRY_FUNCTION,        GIVE_UP,              "Failed to find function with #[Entry] attribute");
  E(ERROR_UNIMPLEMENTED_PROTOTYPE,  DO_NOTHING,           "Prototype function has no implementation: %s");
  E(ERROR_MEMBER_NOT_FOUND,         DO_NOTHING,           "Field of name '%s' is not a member of type '%s'");
  E(ERROR_ASSIGN_TO_IMMUTABLE,      DO_NOTHING,           "Cannot assign to an immutable binding: %s");
  E(ERROR_OPERATE_UPON_IMMUTABLE,   DO_NOTHING,           "Cannot operate upon an immutable binding: %s");
  E(ERROR_ILLEGAL_ESCAPE_SEQUENCE,  TO_END_OF_STRING,     "Illegal escape sequence in string: '\\%c'");
  E(ERROR_FAILED_TO_OPEN_FILE,      GIVE_UP,              "Failed to open file: %s");
  E(ERROR_COMPILE_ERRORS,           GIVE_UP,              "There were compile errors. Stopping.");

  I(ICE_GENERIC,                                          "%s");
  I(ICE_UNHANDLED_NODE_TYPE,                              "Unhandled node type in %s");
  I(ICE_UNHANDLED_INSTRUCTION_TYPE,                       "Unhandled instruction type (%s) in %s");
  I(ICE_UNHANDLED_SLOT_TYPE,                              "Unhandled slot type (%s) in %s");
  I(ICE_UNHANDLED_OPERATOR,                               "Unhandled operator (token=%s) in %s");
  I(ICE_UNHANDLED_RELOCATION,                             "Unable to handle relocation of type: %s");

#undef N
#undef W
#undef E
#undef I
}

error_state CreateErrorState(error_state_type stateType, ...)
{
  va_list args;
  va_start(args, stateType);

  error_state state;
  state.stateType = stateType;
  state.hasErrored = false;

  switch (stateType)
  {
    case GENERAL_STUFF:
    {
    } break;

    case PARSING_UNIT:
    {
      state.parser            = va_arg(args, roo_parser*);
    } break;

    case TRAVERSING_AST:
    {
      state.astSection.code   = va_arg(args, thing_of_code*);
      state.astSection.n      = va_arg(args, node*);
    } break;

    case CODE_GENERATION:
    case FUNCTION_FILLING_IN:
    {
      state.code              = va_arg(args, thing_of_code*);
    } break;

    case TYPE_FILLING_IN:
    {
      state.type              = va_arg(args, type_def*);
    } break;

    case LINKING:
    {
    } break;
  }

  va_end(args);
  return state;
}

void RaiseError(error_state& state, error e, ...)
{
  va_list args;
  va_start(args, e);
  const error_def& def = errors[e];

  // Mark to the error state that an error has occured in its domain
  state.hasErrored = true;

  //                                   White          Light Purple    Orange          Cyan
  static const char* levelColors[]  = {"\x1B[1;37m",  "\x1B[1;35m",   "\x1B[1;31m",   "\x1B[1;36m"};
  static const char* levelStrings[] = {"NOTE",        "WARNING",      "ERROR",        "ICE"};

  /*
   *  NOTE(Isaac): we can't use the vsnprintf trick here to dynamically allocate the string buffer,
   *  because we can't use the vararg list twice.
   */
  char message[1024u];
  vsnprintf(message, 1024u, def.messageFmt, args);

  switch (state.stateType)
  {
    case GENERAL_STUFF:
    {
      fprintf(stderr, "%s%s: \x1B[0m%s\n", levelColors[def.level], levelStrings[def.level], message);
    } break;

    case PARSING_UNIT:
    {
      fprintf(stderr, "%s(%u:%u): %s%s: \x1B[0m%s\n", state.parser->path, state.parser->currentLine,
          state.parser->currentLineOffset, levelColors[def.level], levelStrings[def.level], message);
    } break;

    case TRAVERSING_AST:
    {
      // TODO(Isaac): be more helpful by working out where the AST node is in the source
      fprintf(stderr, "%s%s: \x1B[0m%s\n", levelColors[def.level], levelStrings[def.level], message);
    } break;

    case FUNCTION_FILLING_IN:
    {
      fprintf(stderr, "%s%s: \x1B[0m%s\n", levelColors[def.level], levelStrings[def.level], message);
    } break;

    case TYPE_FILLING_IN:
    {
      fprintf(stderr, "%s%s: \x1B[0m%s\n", levelColors[def.level], levelStrings[def.level], message);
    } break;

    case CODE_GENERATION:
    {
      fprintf(stderr, "%s%s: \x1B[0m%s\n", levelColors[def.level], levelStrings[def.level], message);
    } break;

    case LINKING:
    {
      fprintf(stderr, "%s%s: \x1B[0m%s\n", levelColors[def.level], levelStrings[def.level], message);
    } break;
  }
  va_end(args);

  // --- Follow the poisoning strategy ---
  switch (def.poisonStrategy)
  {
    case DO_NOTHING:
    {
    } break;

    case TO_END_OF_STATEMENT:
    {
      // TODO
    } break;

    case TO_END_OF_ATTRIBUTE:
    {
      roo_parser& parser = *(state.parser);

      while (PeekToken(parser).type != TOKEN_RIGHT_BLOCK)
      {
        NextToken(parser);
      }
    } break;

    case TO_END_OF_STRING:
    {
      // TODO
    } break;

    case TO_END_OF_BLOCK:
    {
      // TODO
    } break;

    case GIVE_UP:
    {
      Crash();
    } break;
  }
}
