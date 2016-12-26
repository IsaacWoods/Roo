/*
 * Copyright (C) 2016, Isaac Woods. All rights reserved.
 */

#include <error.hpp>
#include <cstdio>
#include <cstdarg>

struct error_def
{
  enum error_level
  {
    NOTE,
    WARNING,
    ERROR,
    FATAL,
  } level;

  enum poison_strategy
  {
    DO_NOTHING,
    TO_NEW_LINE,
    GIVE_UP,
  } poisonStrategy;

  const char* message;
};

error_def errors[NUM_ERRORS] = {};

__attribute__((constructor))
void InitErrorDefs()
{
  #define NOTE(tag, message) \
    errors[tag] = error_def{error_def::error_level::NOTE, error_def::poison_strategy::DO_NOTHING, message};

  NOTE(TEST_ERROR_OH_NO_POTATO, "Oh no potato")
}

void RaiseError(error e, ...)
{
  va_list args;
  va_start(args, e);
  const error_def& def = errors[e];

  // TODO: follow the poisoning strategy

  //                                  White         Light Purple  Light Red     Red
  static const char* levelColors[] = {"\x1B[1;37m", "\x1B[1;35m", "\x1B[1;31m", "\x1B[0:31m"};
  static const char* levelStrings[] = {"NOTE", "WARNING", "ERROR", "FATAL"};

  fprintf(stderr, "%s%s: \x1B[0m%s\n", levelColors[def.level], levelStrings[def.level], def.message);
  va_end(args);
}
