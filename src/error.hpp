/*
 * Copyright (C) 2016, Isaac Woods. All rights reserved. 
 */

#pragma once

enum error
{
  ERROR_EXPECTED,           // "Expected {string}"
  ERROR_EXPECTED_BUT_GOT,   // "Expected {string} but got {string} instead"
  ERROR_UNEXPECTED,         // "Unexpected token in {string} position: {string}"
  ERROR_ILLEGAL_ATTRIBUTE,  // "Unrecognised attribute '{string}'"

  FATAL_NO_PROGRAM_NAME,    // "A program name must be specified using the '#[Name(...)]' attribute"

  NUM_ERRORS
};

void RaiseError(error e, ...);
