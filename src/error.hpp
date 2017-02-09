/*
 * Copyright (C) 2016, Isaac Woods. All rights reserved. 
 */

#pragma once

enum error
{
  NOTE_IGNORED_ELF_SECTION,       // "Ignoring section in ELF relocatable: %s"

  ERROR_EXPECTED,                 // "Expected %s"
  ERROR_EXPECTED_BUT_GOT,         // "Expected %s but got %s instead"
  ERROR_UNEXPECTED,               // "Unexpected token in %s position: %s"
  ERROR_ILLEGAL_ATTRIBUTE,        // "Unrecognised attribute '%s'"
  ERROR_UNDEFINED_VARIABLE,       // "Failed to resolve variable called '%s'"
  ERROR_UNDEFINED_FUNCTION,       // "Failed to resolve function called '%s'"
  ERROR_UNDEFINED_TYPE,           // "Failed to resolve type with the name '%s'"
  ERROR_MISSING_OPERATOR,         // "Can't find %s operator for operands of type '%s' and '%s'"
  ERROR_INCOMPATIBLE_ASSIGN,      // "Can't assign a '%s' to a variable of type '%s'
  ERROR_INVALID_OPERATOR,         // "Can't overload operator with token %s"
  ERROR_NO_START_SYMBOL,          // "Missing _start symbol (is this a freestanding environment??)"
  ERROR_INVALID_EXECUTABLE,       // "Unable to create executable at path: %s"
  ERROR_UNRESOLVED_SYMBOL,        // "Failed to resolve symbol: %s"
  ERROR_NO_PROGRAM_NAME,          // "A program name must be specified using the '#[Name(...)]' attribute"
  ERROR_WEIRD_LINKED_OBJECT,      // "'%s' is not a valid ELF64 relocatable"
  ERROR_NO_ENTRY_FUNCTION,        // "Failed to find function with #[Entry] attribute"
  ERROR_UNIMPLEMENTED_PROTOTYPE,  // "Prototype function has no implementation: %s"
  ERROR_MEMBER_NOT_FOUND,         // "Field of name '%s' is not a member of type '%s'"
  ERROR_ASSIGN_TO_IMMUTABLE,      // "Cannot assign to an immutable binding: %s"
  ERROR_OPERATE_UPON_IMMUTABLE,   // "Cannot operate upon an immutable binding: %s"
  ERROR_ILLEGAL_ESCAPE_SEQUENCE,  // "Illegal escape sequence in string: '\\%c'"
  ERROR_FAILED_TO_OPEN_FILE,      // "Failed to open file: %s"

  ICE_GENERIC,                    // "%s"
  ICE_UNHANDLED_NODE_TYPE,        // "Unhandled node type in %s"
  ICE_UNHANDLED_INSTRUCTION_TYPE, // "Unhandled instruction type (%s) in %s"
  ICE_UNHANDLED_SLOT_TYPE,        // "Unhandled slot type (%s) in %s"
  ICE_UNHANDLED_OPERATOR,         // "Unhandled operator (token=%s) in %s"
  ICE_UNHANDLED_RELOCATION,       // "Unable to handle relocation of type: %s"

  NUM_ERRORS
};

enum error_state_type
{
  GENERAL_STUFF,
  PARSING_UNIT,         // NOTE(Isaac): `parser` field is valid
  TRAVERSING_AST,       // NOTE(Isaac): `astSection` field is valid
  FUNCTION_FILLING_IN,  // NOTE(Isaac): `code` field is valid
  TYPE_FILLING_IN,      // NOTE(Isaac): `type` field is valid
  CODE_GENERATION,      // NOTE(Isaac): `code` field is valid
  LINKING
};

struct roo_parser;
struct thing_of_code;
struct node;
struct type_def;

struct error_state
{
  error_state_type stateType;

  union
  {
    struct
    {
      thing_of_code*  code;
      node*           n;
    }               astSection;
    roo_parser*     parser;
    thing_of_code*  code;
    type_def*       type;
  };
};

error_state CreateErrorState(error_state_type type, ...);
void RaiseError(error_state& state, error e, ...);
