/*
 * Copyright (C) 2017, Isaac Woods.
 * See LICENCE.md
 */

#pragma once

#include <string>
#include <vector>
#include <cstdlib>
#include <cstdint>
#include <cstdio>

// NOTE(Isaac): Define this to output .dot files of the AST and interference graph of each function
#define OUTPUT_DOT

// --- Ameanable crashes ---
[[noreturn]] void Crash();

// --- File stuff and things ---
struct File
{
  File(const std::string& name, const std::string& extension);

  std::string name;
  std::string extension;
};

struct Directory
{
  Directory(const char* path);
  ~Directory();

  char*             path;
  std::vector<File> files;
};

// --- Common functions ---
char* itoa(int num, char* str, int base);
char* ReadFile(const char* path);
bool DoesFileExist(const char* path);
std::string FormatString(const std::string& format, ...);
