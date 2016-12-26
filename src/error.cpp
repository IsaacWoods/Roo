/*
 * Copyright (C) 2016, Isaac Woods. All rights reserved.
 */

#include <error.hpp>
#include <cstdio>
#include <cstdlib>
#include <cstdarg>

enum error_level
{
  NOTE,
  WARNING,
  ERROR,
  FATAL,
};

enum poison_strategy
{
  DO_NOTHING,
  TO_END_OF_STATEMENT,
  TO_END_OF_BLOCK,
  JUST_GIVE_UP
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
#define N(tag, message)    errors[tag] = error_def{error_level::NOTE, poison_strategy::DO_NOTHING, message};
#define W(tag, message) errors[tag] = error_def{error_level::WARNING, poison_strategy::DO_NOTHING, message};
#define E(tag, poisoning, message) errors[tag] = error_def{error_level::ERROR, poison_strategy::poisoning, message};
#define F(tag, message)   errors[tag] = error_def{error_level::FATAL, poison_strategy::GIVE_UP, message};

  E(ERROR_EXPECTED,           TO_END_OF_STATEMENT,    "Expected %s");
  E(ERROR_EXPECTED_BUT_GOT,   TO_END_OF_STATEMENT,    "Expected %s but got %s instead");
  E(ERROR_UNEXPECTED,         TO_END_OF_STATEMENT,    "Unexpected token in %s position: %s");
}

void RaiseError(error e, ...)
{
  va_list args;
  va_start(args, e);
  const error_def& def = errors[e];

  // TODO: follow the poisoning strategy

  //                                   White          Light Purple    Orange          Red
  static const char* levelColors[]  = {"\x1B[1;37m",  "\x1B[1;35m",   "\x1B[1;31m",   "\x1B[0:31m"};
  static const char* levelStrings[] = {"NOTE",        "WARNING",      "ERROR",        "FATAL"};

  /*
   *  NOTE(Isaac): we can't use the vsnprintf trick here to dynamically allocate the string buffer,
   *  because we can't use the vararg list twice.
   */
  char message[1024u];
  vsnprintf(message, 1024u, def.messageFmt, args);

  fprintf(stderr, "%s%s: \x1B[0m%s\n", levelColors[def.level], levelStrings[def.level], message);
  va_end(args);
}
