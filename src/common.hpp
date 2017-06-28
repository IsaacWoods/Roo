/*
 * Copyright (C) 2017, Isaac Woods.
 * See LICENCE.md
 */

#pragma once

#include <string>
#include <vector>
#include <memory>
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

template<typename... Args>
std::string FormatString(const std::string& format, Args... args)
{
  size_t size = std::snprintf(nullptr, 0, format.c_str(), args...) + 1u;
  std::unique_ptr<char[]> buffer(new char[size]);
  std::snprintf(buffer.get(), size, format.c_str(), args...);
  return std::string(buffer.get(), buffer.get() + size - 1u);
}
