/*
 * Copyright (C) 2017, Isaac Woods.
 * See LICENCE.md
 */

#pragma once

#include <cstdio>
#include <common.hpp>
#include <token.hpp>

/*
 * XXX: You should use this instead of `assert` from <cassert>
 *
 * - We first evaluate the condition, if it passes we shortcircuit the || operator and return
 * - If the condition failed, we run RaiseError as the first expression of the comma
 * - We then return the right expression of the comma (`false`) to fail overall
 * - We then cast it all to `void` to stop the compiler complaining about the unused value
 */
#define Assert(expression, message)\
  (void)((expression) || (RaiseError(ICE_FAILED_ASSERTION, __FILE__, __LINE__, message),false));

enum Error
{
  NOTE_IGNORED_ELF_SECTION,                     // "Ignoring section in ELF relocatable: %s"

  WARNING_FOUND_TAB,                            // "Found a hard tab; their use is discouraged in Roo"

  ERROR_COMPILE_ERRORS,                         // "There were compile errors. Stopping."
  ERROR_EXPECTED,                               // "Expected %s"
  ERROR_EXPECTED_BUT_GOT,                       // "Expected %s but got %s instead"
  ERROR_UNEXPECTED,                             // "Unexpected token in %s position: %s"
  ERROR_UNEXPECTED_EXPRESSION,                  // "Unexpected expression type in %s position: %s"
  ERROR_ILLEGAL_ATTRIBUTE,                      // "Unrecognised attribute '%s'"
  ERROR_VARIABLE_NOT_IN_SCOPE,                  // "No such binding in current scope: '%s'"
  ERROR_UNDEFINED_FUNCTION,                     // "Failed to resolve function called '%s'"
  ERROR_UNDEFINED_TYPE,                         // "Failed to resolve type with the name '%s'"
  ERROR_MISSING_OPERATOR,                       // "Can't find %s operator for operands of type '%s' and '%s'"
  ERROR_INCOMPATIBLE_ASSIGN,                    // "Can't assign '%s' to a variable of type '%s'
  ERROR_INCOMPATIBLE_TYPE,                      // "Expected type of '%s' but got a '%s'"
  ERROR_INVALID_OPERATOR,                       // "Can't overload operator with token %s"
  ERROR_NO_START_SYMBOL,                        // "Missing _start symbol (is this a freestanding environment??)"
  ERROR_INVALID_EXECUTABLE,                     // "Unable to create executable at path: %s"
  ERROR_UNRESOLVED_SYMBOL,                      // "Failed to resolve symbol: %s"
  ERROR_NO_PROGRAM_NAME,                        // "A program name must be specified using '#[Name(...)]'"
  ERROR_WEIRD_LINKED_OBJECT,                    // "'%s' is not a valid ELF64 relocatable: %s"
  ERROR_MULTIPLE_ENTRY_POINTS,                  // "Found multiple entry points: '%s' and '%s' are two"
  ERROR_NO_ENTRY_FUNCTION,                      // "Failed to find function with #[Entry] attribute"
  ERROR_UNIMPLEMENTED_PROTOTYPE,                // "Prototype function has no implementation: %s"
  ERROR_MEMBER_NOT_FOUND,                       // "Field of name '%s' is not a member of type '%s'"
  ERROR_ASSIGN_TO_IMMUTABLE,                    // "Cannot assign to an immutable binding: %s"
  ERROR_OPERATE_UPON_IMMUTABLE,                 // "Cannot operate upon an immutable binding: %s"
  ERROR_ILLEGAL_ESCAPE_SEQUENCE,                // "Illegal escape sequence in string: '\\%c'"
  ERROR_FAILED_TO_OPEN_FILE,                    // "Failed to open file: %s"
  ERROR_BIND_USED_BEFORE_INIT,                  // "The binding '%s' was used before it was initialised"
  ERROR_INVALID_ARRAY_SIZE,                     // "Array size must be an unsigned constant value"
  ERROR_MISSING_MODULE,                         // "Couldn't find module: %s"
  ERROR_MALFORMED_MODULE_INFO,                  // "Couldn't parse module info file(%s): %s"
  ERROR_FAILED_TO_EXPORT_MODULE,                // "Failed to export module(%s): %s"
  ERROR_UNLEXABLE_CHARACTER,                    // "Failed to lex character: '%c'. Trying to skip."
  ERROR_MUST_RETURN_SOMETHING,                  // "Expected to return something of type: %s"
  ERROR_RETURN_VALUE_NOT_EXPECTED,              // "Shouldn't return anything, trying to return a: %s"
  ERROR_MISSING_TYPE_INFORMATION,               // "Missing type information: %s"
  ERROR_TYPE_CONSTRUCT_TOO_FEW_EXPRESSIONS,     // "Insufficient expressions to construct all members of type: %s"

  ICE_GENERIC,                                  // "%s"
  ICE_UNHANDLED_TOKEN_TYPE,                     // "Unhandled token type in %s: %s"
  ICE_UNHANDLED_NODE_TYPE,                      // "Unhandled node type in %s: %s"
  ICE_UNHANDLED_INSTRUCTION_TYPE,               // "Unhandled instruction type (%s) in %s"
  ICE_UNHANDLED_SLOT_TYPE,                      // "Unhandled slot type (%s) in %s"
  ICE_UNHANDLED_OPERATOR,                       // "Unhandled operator (token=%s) in %s"
  ICE_UNHANDLED_RELOCATION,                     // "Unable to handle relocation of type: %s"
  ICE_NONEXISTANT_AST_PASSLET,                  // "Nonexistant passlet for node of type: %s"
  ICE_NONEXISTANT_AIR_PASSLET,                  // "Nonexistant passlet for instruction of type: %s"
  ICE_FAILED_ASSERTION,                         // "Assertion failed at (%s:%d): %s"
  ICE_MISSING_ELF_SECTION,                      // "Failed to find ELF section with name: %s"

  NUM_ERRORS
};

template<typename T> struct Parser;
struct CodeThing;
struct TypeDef;
struct ASTNode;
struct AirInstruction;

enum ErrorLevel
{
  NOTE,
  WARNING,
  ERROR,
  ICE,
};

enum PoisonStrategy
{
  DO_NOTHING,
  SKIP_TOKEN,
  SKIP_CHARACTER,
  TO_END_OF_STATEMENT,
  TO_END_OF_ATTRIBUTE,
  TO_END_OF_BLOCK,
  TO_END_OF_STRING,
  GIVE_UP
};

struct ErrorDef
{
  ErrorLevel      level;
  PoisonStrategy  poisonStrategy;
  const char*     messageFmt;
};

extern const char* g_levelColors[];
extern const char* g_levelStrings[];

struct ErrorState
{
  ErrorState();
  virtual ~ErrorState() { }

  bool hasErrored;

  virtual void Poison(PoisonStrategy strategy);
  virtual void PrintError(const char* message, const ErrorDef& error);
};

template<typename T>
struct ParsingErrorState : ErrorState
{
  ParsingErrorState(Parser<T>& parser)
    :parser(parser)
  { }

  Parser<T>& parser;

  void Poison(PoisonStrategy strategy) override
  {
    switch (strategy)
    {
      case DO_NOTHING:
      {
      } break;

      case SKIP_TOKEN:
      {
        parser.NextToken(false);
      } break;

      case SKIP_CHARACTER:
      {
        // TODO: apprently we don't need to do anything here?
        // Shouldn't we do parser.NextChar() (and make ParsingErrorState a friend class)
      } break;

      case TO_END_OF_STATEMENT:
      {
        while (parser.PeekToken(false).type != TOKEN_LINE)
        {
          parser.NextToken(false);
        }
        parser.NextToken(false);
      } break;

      case TO_END_OF_ATTRIBUTE:
      {
        while (parser.PeekToken().type != TOKEN_RIGHT_BLOCK)
        {
          parser.NextToken();
        }
        parser.NextToken();
      } break;

      case TO_END_OF_STRING:
      {
        // TODO: lexing or parsing level?? When is this even used?
      } break;

      case TO_END_OF_BLOCK:
      {
        while (parser.PeekToken().type != TOKEN_RIGHT_BRACE)
        {
          parser.NextToken();
        }
        parser.NextToken();
      } break;

      case GIVE_UP:
      {
        Crash();
      } break;
    }
  }

  void PrintError(const char* message, const ErrorDef& error) override
  {
    fprintf(stderr, "\x1B[1;37m%s(%u:%u):\x1B[0m %s%s: \x1B[0m%s\n", parser.path.c_str(),
            parser.currentLine, parser.currentLineOffset, g_levelColors[error.level],
            g_levelStrings[error.level], message);
  }
};

struct CodeThingErrorState : ErrorState
{
  CodeThingErrorState(CodeThing* thing);
  ~CodeThingErrorState() { }

  CodeThing* thing;
};

extern const char* g_levelColors[];
extern const char* g_levelStrings[];
extern ErrorDef g_errors[NUM_ERRORS];
void ReportErrorInErrorReporter();
void PrintStackTrace();

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-security"
template<typename... Ts>
void RaiseError(ErrorState* state, Error e, Ts... args)
{
  const ErrorDef& def = g_errors[e];

  // Mark to the error state that an error has occured in its domain
  state->hasErrored = true;

  /*
   *  We can't use the snprintf trick here to dynamically allocate the string buffer, because we can't use the
   *  list twice
   */
  char message[1024u];
  snprintf(message, 1024u, def.messageFmt, args...);
  state->PrintError(message, def);

  if (e == ICE_FAILED_ASSERTION)
  {
    PrintStackTrace();
  }

  state->Poison(def.poisonStrategy);
}

template<typename... Ts>
void RaiseError(Error e, Ts... args)
{
  const ErrorDef& def = g_errors[e];

  char message[1024u];
  snprintf(message, 1024u, def.messageFmt, args...);
  fprintf(stderr, "%s%s: \x1B[0m%s\n", g_levelColors[def.level], g_levelStrings[def.level], message);

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
    ReportErrorInErrorReporter();
  }
}
#pragma clang diagnostic pop
