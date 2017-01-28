/*
 * Copyright (C) 2016, Isaac Woods. All rights reserved. 
 */

#pragma once

enum error
{
  ERROR_EXPECTED,                 // "Expected {string}"
  ERROR_EXPECTED_BUT_GOT,         // "Expected {string} but got {string} instead"
  ERROR_UNEXPECTED,               // "Unexpected token in {string} position: {string}"
  ERROR_ILLEGAL_ATTRIBUTE,        // "Unrecognised attribute '{string}'"
  ERROR_UNDEFINED_VARIABLE,       // "Failed to resolve variable called '{string}'"
  ERROR_UNDEFINED_FUNCTION,       // "Failed to resolve function called '{string}'"
  ERROR_UNDEFINED_TYPE,           // "Failed to resolve type with the name '{string}'"
  ERROR_MISSING_OPERATOR,         // "Can't find {string} operator for operands of type '{string}' and '{string}'"
  ERROR_INCOMPATIBLE_ASSIGN,      // "Can't assign a '{string}' to a variable of type '{string}'
  ERROR_INVALID_OPERATOR,         // "Can't overload operator with token {string}"
  ERROR_NO_START_SYMBOL,          // "Missing _start symbol (is this a freestanding environment??)"
  ERROR_INVALID_EXECUTABLE,       // "Unable to create executable at path: {string}"
  ERROR_UNRESOLVED_SYMBOL,        // "Failed to resolve symbol: {string}"
  ERROR_NO_PROGRAM_NAME,          // "A program name must be specified using the '#[Name(...)]' attribute"
  ERROR_WEIRD_LINKED_OBJECT,      // "'{string}' is not a valid ELF64 relocatable"
  ERROR_NO_ENTRY_FUNCTION,        // "Failed to find function with #[Entry] attribute"
  ERROR_UNIMPLEMENTED_PROTOTYPE,  // "Prototype function has no implementation: %s"
  ERROR_MEMBER_NOT_FOUND,         // "Field of name '%s' is not a member of type '%s'"

  ICE_GENERIC,                    // "{string}"
  ICE_UNHANDLED_NODE_TYPE,        // "Unhandled node type in %s"
  ICE_UNHANDLED_OPERATOR,         // "Unhandled operator (token=%s) in %s"
  ICE_UNHANDLED_RELOCATION,       // "Unable to handle relocation of type: {string}"

  NUM_ERRORS
};

void RaiseError(error e, ...);
