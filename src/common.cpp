/*
 * Copyright (C) 2017, Isaac Woods.
 * See LICENCE.md
 */

#include <common.hpp>
#include <algorithm>
#include <cstring>
#include <dirent.h>
#include <ast.hpp>
#include <air.hpp>
#include <sys/stat.h>

#define USING_GDB

#ifdef USING_GDB
  #include <signal.h>
  [[noreturn]] void Crash()
  {
    raise(SIGINT);
    exit(1);
  }
#else
  [[noreturn]] void Crash()
  {
    fprintf(stderr, "((ABORTING))\n");
    exit(1);
  }
#endif

template<>
void Free<directory>(directory& dir)
{
  free(dir.path);
  FreeVector<file>(dir.files);
}

template<>
void Free<file>(file& f)
{
  free(f.name);
  // NOTE(Isaac): don't free the extension string, it shares memory with the path
}

void OpenDirectory(directory& dir, const char* path)
{
  dir.path = strdup(path);
  InitVector<file>(dir.files);

  DIR* d;
  struct dirent* dirent;

  if (!(d = opendir(path)))
  {
    fprintf(stderr, "FATAL: failed to open directory: '%s'\n", path);
    Crash();
  }

  while ((dirent = readdir(d)))
  {
    file f;
    f.name = strdup(dirent->d_name);
    f.extension = nullptr;

    /*
     * NOTE(Isaac): if the path is of the form '~/.test', the extension may actually be the base name
     */
    char* dotIndex = strchr(f.name, '.');
    if (dotIndex)
    {
      f.extension = dotIndex + (ptrdiff_t)1u;
    }

    Add<file>(dir.files, f);
  }

  closedir(d);
}

char* itoa(int num, char* str, int base)
{
  int i = 0;
  bool isNegative = false;

  // Explicitly handle 0
  if (num == 0)
  {
    str[i++] = '0';
    str[i] = '\0';
    return str;
  }

  // Handle negative numbers if base10
  if (num < 0 && base == 10)
  {
    isNegative = true;
    num = -num;
  }

  // Loop over each digit and output its char representation
  while (num != 0)
  {
    int rem = num % base;
    str[i++] = (rem > 9) ? ((rem - 10) + 'a') : (rem + '0');
    num /= base;
  }

  if (isNegative)
  {
    str[i++] = '-';
  }

  str[i] = '\0';

  // Reverse the string
  int start = 0;
  int end = i - 1;

  while (start < end)
  {
    std::swap(*(str + start), *(str + end));
    ++start;
    --end;
  }

  return str;
}

char* ReadFile(const char* path)
{
  FILE* file = fopen(path, "rb");

  if (!file)
  {
    fprintf(stderr, "Failed to read source file: %s\n", path);
    Crash();
  }

  fseek(file, 0, SEEK_END);
  unsigned long length = ftell(file);
  fseek(file, 0, SEEK_SET);
  char* contents = static_cast<char*>(malloc(length + 1));

  if (!contents)
  {
    fprintf(stderr, "Failed to allocate space for source file!\n");
    Crash();
  }

  if (fread(contents, 1, length, file) != length)
  {
    fprintf(stderr, "Failed to read source file: %s\n", path);
    Crash();
  }

  contents[length] = '\0';
  fclose(file);

  return contents;
}

bool DoesFileExist(const char* path)
{
  struct stat buffer;
  return (stat(path, &buffer) == 0);
}
