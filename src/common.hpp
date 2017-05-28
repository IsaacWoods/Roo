/*
 * Copyright (C) 2017, Isaac Woods.
 * See LICENCE.md
 */

#pragma once

#include <cstdlib>
#include <cstdint>
#include <cstdio>
#include <vector.hpp>

// NOTE(Isaac): Define this to output .dot files of the AST and interference graph of each function
#define OUTPUT_DOT

// --- Ameanable crashes ---
[[noreturn]] void Crash();

// --- File stuff and things ---
struct file
{
  char* name;

  /*
   * NOTE(Isaac): if the file doesn't have an extension, this is null
   * NOTE(Isaac): this points into `name`, because it'll be at the end
   */
  char* extension;
};

struct directory
{
  char* path;
  vector<file> files;
};

void OpenDirectory(directory& dir, const char* path);

// --- Common functions ---
char* itoa(int num, char* str, int base);
char* ReadFile(const char* path);
bool DoesFileExist(const char* path);
