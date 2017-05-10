/*
 * Copyright (C) 2017, Isaac Woods.
 * See LICENCE.md
 */

#pragma once

enum error
{
  NOTE_IGNORED_ELF_SECTION,       // "Ignoring section in ELF relocatable: %s"

  WARNING_FOUND_TAB,              // "Found a tab; their use is discouraged in Roo"

  ERROR_COMPILE_ERRORS,           // "There were compile errors. Stopping."
  ERROR_EXPECTED,                 // "Expected %s"
  ERROR_EXPECTED_BUT_GOT,         // "Expected %s but got %s instead"
  ERROR_UNEXPECTED,               // "Unexpected token in %s position: %s"
  ERROR_UNEXPECTED_EXPRESSION,    // "Unexpected expression type in %s position: %s"
  ERROR_ILLEGAL_ATTRIBUTE,        // "Unrecognised attribute '%s'"
  ERROR_UNDEFINED_VARIABLE,       // "Failed to resolve variable called '%s'"
  ERROR_UNDEFINED_FUNCTION,       // "Failed to resolve function called '%s'"
  ERROR_UNDEFINED_TYPE,           // "Failed to resolve type with the name '%s'"
  ERROR_MISSING_OPERATOR,         // "Can't find %s operator for operands of type '%s' and '%s'"
  ERROR_INCOMPATIBLE_ASSIGN,      // "Can't assign a '%s' to a variable of type '%s'
  ERROR_INCOMPATIBLE_TYPE,        // "Expected type of '%s' but got a '%s'"
  ERROR_INVALID_OPERATOR,         // "Can't overload operator with token %s"
  ERROR_NO_START_SYMBOL,          // "Missing _start symbol (is this a freestanding environment??)"
  ERROR_INVALID_EXECUTABLE,       // "Unable to create executable at path: %s"
  ERROR_UNRESOLVED_SYMBOL,        // "Failed to resolve symbol: %s"
  ERROR_NO_PROGRAM_NAME,          // "A program name must be specified using the '#[Name(...)]' attribute"
  ERROR_WEIRD_LINKED_OBJECT,      // "'%s' is not a valid ELF64 relocatable: %s"
  ERROR_NO_ENTRY_FUNCTION,        // "Failed to find function with #[Entry] attribute"
  ERROR_UNIMPLEMENTED_PROTOTYPE,  // "Prototype function has no implementation: %s"
  ERROR_MEMBER_NOT_FOUND,         // "Field of name '%s' is not a member of type '%s'"
  ERROR_ASSIGN_TO_IMMUTABLE,      // "Cannot assign to an immutable binding: %s"
  ERROR_OPERATE_UPON_IMMUTABLE,   // "Cannot operate upon an immutable binding: %s"
  ERROR_ILLEGAL_ESCAPE_SEQUENCE,  // "Illegal escape sequence in string: '\\%c'"
  ERROR_FAILED_TO_OPEN_FILE,      // "Failed to open file: %s"
  ERROR_BIND_USED_BEFORE_INIT,    // "The binding '%s' was used before it was initialised"
  ERROR_INVALID_ARRAY_SIZE,       // "Array size must be an unsigned constant value"
  ERROR_MISSING_MODULE,           // "Couldn't find module: %s"
  ERROR_MALFORMED_MODULE_INFO,    // "Couldn't parse module info file(%s): %s"
  ERROR_FAILED_TO_EXPORT_MODULE,  // "Failed to export module(%s): %s"

  ICE_GENERIC,                    // "%s"
  ICE_UNHANDLED_NODE_TYPE,        // "Unhandled node type in %s: %s"
  ICE_UNHANDLED_INSTRUCTION_TYPE, // "Unhandled instruction type (%s) in %s"
  ICE_UNHANDLED_SLOT_TYPE,        // "Unhandled slot type (%s) in %s"
  ICE_UNHANDLED_OPERATOR,         // "Unhandled operator (token=%s) in %s"
  ICE_UNHANDLED_RELOCATION,       // "Unable to handle relocation of type: %s"

  NUM_ERRORS
};

enum error_state_type
{
  GENERAL_STUFF,
  PARSING_UNIT,         // `parser`     field is valid
  TRAVERSING_AST,       // `astSection` field is valid
  FUNCTION_FILLING_IN,  // `code`       field is valid
  TYPE_FILLING_IN,      // `type`       field is valid
  GENERATING_AIR,       // `instuction` field is valid
  CODE_GENERATION,      // `code`       field is valid
  LINKING
};

struct roo_parser;
struct thing_of_code;
struct node;
struct type_def;
struct air_instruction;

struct error_state
{
  error_state_type  stateType;
  bool              hasErrored;

  union
  {
    struct
    {
      thing_of_code*    code;
      node*             n;
    }                 astSection;
    roo_parser*       parser;
    thing_of_code*    code;
    type_def*         type;
    air_instruction*  instruction;
  };
};

error_state CreateErrorState(error_state_type type, ...);
void RaiseError(error_state& state, error e, ...);
