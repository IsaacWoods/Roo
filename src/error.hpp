/*
 * Copyright (C) 2016, Isaac Woods. All rights reserved. 
 */

#pragma once

enum error
{
  TEST_ERROR_OH_NO_POTATO,

  NUM_ERRORS
};

void RaiseError(error e, ...);
