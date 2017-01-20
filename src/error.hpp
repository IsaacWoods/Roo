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

  FATAL_NO_PROGRAM_NAME,          // "A program name must be specified using the '#[Name(...)]' attribute"

  ICE_GENERIC,                    // "{string}"
  ICE_UNHANDLED_NODE_TYPE,        // "Unhandled node type for returning {string} in GenNodeAIR for type: {string|"

  NUM_ERRORS
};

void RaiseError(error e, ...);
