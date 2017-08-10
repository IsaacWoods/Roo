/*
 * Copyright (C) 2017, Isaac Woods.
 * See LICENCE.md
 */

#include <error.hpp>
#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include <common.hpp>
#include <parsing.hpp>
#include <ir.hpp>
#include <ast.hpp>
#include <air.hpp>

//                              White          Light Purple    Orange          Cyan
const char* g_levelColors[]  = {"\x1B[1;37m",  "\x1B[1;35m",   "\x1B[1;31m",   "\x1B[1;36m"};
const char* g_levelStrings[] = {"NOTE",        "WARNING",      "ERROR",        "ICE"};

/*
 * If this option is set, we will print backtraces when an Assert fails
 */
//#define PRINT_STACK_TRACE

#ifdef PRINT_STACK_TRACE
  #ifdef __linux__
    #include <execinfo.h>
  #endif
#endif

ErrorDef g_errors[NUM_ERRORS] = {};

__attribute__((constructor))
void InitErrorDefs()
{
#define N(tag, msg)             g_errors[tag] = ErrorDef{ErrorLevel::NOTE,    PoisonStrategy::DO_NOTHING, msg};
#define W(tag, msg)             g_errors[tag] = ErrorDef{ErrorLevel::WARNING, PoisonStrategy::DO_NOTHING, msg};
#define E(tag, poisoning, msg)  g_errors[tag] = ErrorDef{ErrorLevel::ERROR,   PoisonStrategy::poisoning,  msg};
#define I(tag, msg)             g_errors[tag] = ErrorDef{ErrorLevel::ICE,     PoisonStrategy::GIVE_UP,    msg};

  N(NOTE_IGNORED_ELF_SECTION,                                       "Ignored section in ELF relocatable: %s");

  W(WARNING_FOUND_TAB,                                              "Found a hard tab; their use is discouraged in Roo");

  E(ERROR_COMPILE_ERRORS,                     GIVE_UP,              "There were compile errors. Stopping.");
  E(ERROR_EXPECTED,                           TO_END_OF_STATEMENT,  "Expected %s");
  E(ERROR_EXPECTED_BUT_GOT,                   TO_END_OF_STATEMENT,  "Expected %s but got %s instead");
  E(ERROR_UNEXPECTED,                         SKIP_TOKEN,           "Unexpected token in %s position: %s. Skipping.");
  E(ERROR_UNEXPECTED_EXPRESSION,              TO_END_OF_STATEMENT,  "Unexpected expression type in %s position: %s");
  E(ERROR_ILLEGAL_ATTRIBUTE,                  TO_END_OF_ATTRIBUTE,  "Unrecognised attribute '%s'");
  E(ERROR_VARIABLE_NOT_IN_SCOPE,              DO_NOTHING,           "No such binding in current scope: '%s'");
  E(ERROR_UNDEFINED_FUNCTION,                 DO_NOTHING,           "Failed to resolve function called '%s'");
  E(ERROR_UNDEFINED_TYPE,                     DO_NOTHING,           "Failed to resolve type with the name '%s'");
  E(ERROR_MISSING_OPERATOR,                   DO_NOTHING,           "Can't find %s operator for operand types '%s' and '%s'");
  E(ERROR_INCOMPATIBLE_ASSIGN,                TO_END_OF_STATEMENT,  "Can't assign '%s' to a variable of type '%s'");
  E(ERROR_INCOMPATIBLE_TYPE,                  DO_NOTHING,           "Expected type of '%s' but got a '%s'");
  E(ERROR_INVALID_OPERATOR,                   TO_END_OF_BLOCK,      "Can't overload operator with token %s");
  E(ERROR_INVALID_OPERATOR,                   TO_END_OF_STATEMENT,  "Can't overload operator with token %s");
  E(ERROR_NO_START_SYMBOL,                    GIVE_UP,              "No _start symbol (is this a freestanding environment?\?)");
  E(ERROR_INVALID_EXECUTABLE,                 GIVE_UP,              "Unable to create executable at path: %s");
  E(ERROR_UNRESOLVED_SYMBOL,                  DO_NOTHING,           "Failed to resolve symbol: %s");
  E(ERROR_NO_PROGRAM_NAME,                    GIVE_UP,              "A program name must be specified using the '#[Name(...)]' attribute");
  E(ERROR_WEIRD_LINKED_OBJECT,                GIVE_UP,              "Failed to link against object '%s': %s");
  E(ERROR_MULTIPLE_ENTRY_POINTS,              GIVE_UP,              "Found multiple entry points: '%s' and '%s' are two");
  E(ERROR_NO_ENTRY_FUNCTION,                  GIVE_UP,              "Failed to find function with #[Entry] attribute");
  E(ERROR_UNIMPLEMENTED_PROTOTYPE,            DO_NOTHING,           "Prototype function has no implementation: %s");
  E(ERROR_MEMBER_NOT_FOUND,                   DO_NOTHING,           "Field of name '%s' is not a member of type '%s'");
  E(ERROR_ASSIGN_TO_IMMUTABLE,                DO_NOTHING,           "Cannot assign to an immutable binding: %s");
  E(ERROR_OPERATE_UPON_IMMUTABLE,             DO_NOTHING,           "Cannot operate upon an immutable binding: %s");
  E(ERROR_ILLEGAL_ESCAPE_SEQUENCE,            TO_END_OF_STRING,     "Illegal escape sequence in string: '\\%c'");
  E(ERROR_FAILED_TO_OPEN_FILE,                GIVE_UP,              "Failed to open file: %s");
  E(ERROR_BIND_USED_BEFORE_INIT,              GIVE_UP,              "The binding '%s' was used before it was initialised");
  E(ERROR_INVALID_ARRAY_SIZE,                 DO_NOTHING,           "Array size must be an unsigned constant value");
  E(ERROR_MISSING_MODULE,                     DO_NOTHING,           "Couldn't find module: %s");
  E(ERROR_MALFORMED_MODULE_INFO,              GIVE_UP,              "Couldn't parse module info file(%s): %s");
  E(ERROR_FAILED_TO_EXPORT_MODULE,            GIVE_UP,              "Failed to export module(%s): %s");
  E(ERROR_UNLEXABLE_CHARACTER,                SKIP_CHARACTER,       "Failed to lex character: '%c'. Trying to skip.");
  E(ERROR_MUST_RETURN_SOMETHING,              DO_NOTHING,           "Expected to return something of type: %s");
  E(ERROR_RETURN_VALUE_NOT_EXPECTED,          DO_NOTHING,           "Shouldn't return anything, trying to return a: %s");
  E(ERROR_MISSING_TYPE_INFORMATION,           DO_NOTHING,           "Missing type information: %s");
  E(ERROR_TYPE_CONSTRUCT_TOO_FEW_EXPRESSIONS, DO_NOTHING,           "Too few expressions to construct all members of type: %s");

  I(ICE_GENERIC,                                                    "%s");
  I(ICE_UNHANDLED_TOKEN_TYPE,                                       "Unhandled token type in %s: %s");
  I(ICE_UNHANDLED_NODE_TYPE,                                        "Unhandled node type in %s: %s");
  I(ICE_UNHANDLED_INSTRUCTION_TYPE,                                 "Unhandled instruction type (%s) in %s");
  I(ICE_UNHANDLED_SLOT_TYPE,                                        "Unhandled slot type (%s) in %s");
  I(ICE_UNHANDLED_OPERATOR,                                         "Unhandled operator (token=%s) in %s");
  I(ICE_NONEXISTANT_AST_PASSLET,                                    "Nonexistant passlet for node of type: %s");
  I(ICE_NONEXISTANT_AIR_PASSLET,                                    "Nonexistant passlet for instruction of type: %s");
  I(ICE_UNHANDLED_RELOCATION,                                       "Unable to handle relocation of type: %s");
  I(ICE_FAILED_ASSERTION,                                           "Assertion failed at (%s:%d): %s");
  I(ICE_MISSING_ELF_SECTION,                                        "Failed to find ELF section with name: %s");

#undef N
#undef W
#undef E
#undef I
}

ErrorState::ErrorState()
  :hasErrored(false)
{ }

void ErrorState::Poison(PoisonStrategy /*strategy*/) { }
void ErrorState::PrintError(const char* message, const ErrorDef& error)
{
  fprintf(stderr, "%s%s: \x1B[0m%s\n", g_levelColors[error.level], g_levelStrings[error.level], message);
}

CodeThingErrorState::CodeThingErrorState(CodeThing* thing)
  :thing(thing)
{ }

/*
 * XXX: `Assert` must not be used in the following methods, because it executes `RaiseError` to report errors.
 * Instead, use this method to crash with a relatively helpful message.
 */
#define REPORT_ERROR_IN_ERROR_REPORTER()\
  {\
  fprintf(stderr, "\x1B[1;31m!!! MEGA ICE !!!\x1B[0m The error reporting system has broken, if you're seeing this message in production, please file a bug report! (%s:%d)", __FILE__, __LINE__);\
  Crash();\
  }

static void PrintStackTrace()
{
#ifdef PRINT_STACK_TRACE
  #ifdef __linux__
    static const unsigned int BUFFER_SIZE = 100u;

    void* buffer[BUFFER_SIZE];
    int entryCount = backtrace(buffer, BUFFER_SIZE);
    char** strings = backtrace_symbols(buffer, entryCount);

    if (!strings)
    {
      perror("backtrace_symbols");
      REPORT_ERROR_IN_ERROR_REPORTER();
    }

    for (unsigned int i = 0u;
         i < (unsigned int)entryCount;
         i++)
    {
      fprintf(stderr, "%s\n", strings[i]);
    }

    free(strings);
  #else
    fprintf(stderr, "Printing stacktraces is not supported on your platform, please turn it off before compiling\n");
    REPORT_ERROR_IN_ERROR_REPORTER();
  #endif
#endif
}

void RaiseError(ErrorState* state, Error e, ...)
{
  va_list args;
  va_start(args, (unsigned int)e);
  const ErrorDef& def = g_errors[e];

  // Mark to the error state that an error has occured in its domain
  state->hasErrored = true;

  /*
   *  We can't use the vsnprintf trick here to dynamically allocate the string buffer, because we can't use the
   *  list twice
   */
  char message[1024u];
  vsnprintf(message, 1024u, def.messageFmt, args);
  state->PrintError(message, def);
  va_end(args);

  if (e == ICE_FAILED_ASSERTION)
  {
    PrintStackTrace();
  }

  state->Poison(def.poisonStrategy);
}

void RaiseError(Error e, ...)
{
  va_list args;
  va_start(args, (unsigned int)e);
  const ErrorDef& def = g_errors[e];

  char message[1024u];
  vsnprintf(message, 1024u, def.messageFmt, args);

  fprintf(stderr, "%s%s: \x1B[0m%s\n", g_levelColors[def.level], g_levelStrings[def.level], message);
  va_end(args);

  if (e == ICE_FAILED_ASSERTION)
  {
    PrintStackTrace();
  }

  // --- Make sure it doesn't expect us to poison anything - we can't without an ErrorState ---
  if (def.poisonStrategy == GIVE_UP)
  {
    Crash();
  }
  else if (def.poisonStrategy != DO_NOTHING)
  {
    REPORT_ERROR_IN_ERROR_REPORTER();
  }
}
