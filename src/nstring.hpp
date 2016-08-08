/*
 * Copyright (C) 2016, Isaac Woods. All rights reserved.
 */

#pragma once

struct nstring
{
  const char*  first;   // NOTE(Isaac): not null-terminated!
  unsigned int length;
};

nstring Copy(const nstring& str);
char* ToCStr(const nstring& str);
