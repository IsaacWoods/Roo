/*
 * Copyright (C) 2016, Isaac Woods. All rights reserved.
 */

#include <nstring.hpp>
#include <cstring>
#include <cstdlib>

nstring Copy(const nstring& str)
{
  char* newFirst = static_cast<char*>(malloc(sizeof(char) * str.length));
  memcpy(newFirst, str.first, sizeof(char) * str.length);

  return nstring{newFirst, str.length};
}

char* ToCStr(const nstring& str)
{
  char* newFirst = static_cast<char*>(malloc(sizeof(char) * str.length + 1u));
  memcpy(newFirst, str.first, sizeof(char) * str.length);
  newFirst[str.length] = '\0';

  return newFirst;
}
