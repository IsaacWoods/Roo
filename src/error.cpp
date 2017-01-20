/*
 * Copyright (C) 2016, Isaac Woods. All rights reserved.
 */

#include <error.hpp>
#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include <common.hpp>

enum error_level
{
  NOTE,
  WARNING,
  ERROR,
  FATAL,
  ICE,
};

enum poison_strategy
{
  DO_NOTHING,
  TO_END_OF_STATEMENT,
  TO_END_OF_ATTRIBUTE,
  TO_END_OF_BLOCK,
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
#define F(tag, msg)             errors[tag] = error_def{error_level::FATAL, poison_strategy::GIVE_UP, msg};
#define I(tag, msg)             errors[tag] = error_def{error_level::ICE, poison_strategy::GIVE_UP, msg};

  E(ERROR_EXPECTED,             TO_END_OF_STATEMENT,  "Expected %s");
  E(ERROR_EXPECTED_BUT_GOT,     TO_END_OF_STATEMENT,  "Expected %s but got %s instead");
  E(ERROR_UNEXPECTED,           TO_END_OF_STATEMENT,  "Unexpected token in %s position: %s");
  E(ERROR_ILLEGAL_ATTRIBUTE,    TO_END_OF_ATTRIBUTE,  "Unrecognised attribute '%s'");
  E(ERROR_UNDEFINED_VARIABLE,   TO_END_OF_STATEMENT,  "Failed to resolve variable called '%s'");
  E(ERROR_UNDEFINED_FUNCTION,   TO_END_OF_STATEMENT,  "Failed to resolve function called '%s'");
  E(ERROR_UNDEFINED_TYPE,       TO_END_OF_STATEMENT,  "Failed to resolve type with the name '%s'");
  E(ERROR_MISSING_OPERATOR,     TO_END_OF_STATEMENT,  "Can't find %s operator for operands of type '%s' and '%s'");
  E(ERROR_INCOMPATIBLE_ASSIGN,  TO_END_OF_STATEMENT,  "Can't assign a '%s' to a variable of type '%s'");
  E(ERROR_INVALID_OPERATOR,     TO_END_OF_BLOCK,      "Can't overload operator with token %s");

  F(FATAL_NO_PROGRAM_NAME,    "A program name must be specified using the '#[Name(...)]' attribute");

  I(ICE_GENERIC,              "%s");
  I(ICE_UNHANDLED_NODE_TYPE,  "Unhandled node type for returning %s in GenNodeAIR for type: %s");

#undef N
#undef W
#undef E
#undef F
#undef I
}

void RaiseError(error e, ...)
{
  va_list args;
  va_start(args, e);
  const error_def& def = errors[e];

  // TODO: follow the poisoning strategy

  //                                   White          Light Purple    Orange          Red           Cyan
  static const char* levelColors[]  = {"\x1B[1;37m",  "\x1B[1;35m",   "\x1B[1;31m",   "\x1B[0:31m", "\x1B[1;36m"};
  static const char* levelStrings[] = {"NOTE",        "WARNING",      "ERROR",        "FATAL",      "ICE"};

  /*
   *  NOTE(Isaac): we can't use the vsnprintf trick here to dynamically allocate the string buffer,
   *  because we can't use the vararg list twice.
   */
  char message[1024u];
  vsnprintf(message, 1024u, def.messageFmt, args);

  fprintf(stderr, "%s%s: \x1B[0m%s\n", levelColors[def.level], levelStrings[def.level], message);
  va_end(args);

  switch (def.poisonStrategy)
  {
    case DO_NOTHING:
    {
      // TODO
    } break;

    case TO_END_OF_STATEMENT:
    {
      // TODO
    } break;

    case TO_END_OF_ATTRIBUTE:
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
