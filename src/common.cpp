/*
 * Copyright (C) 2017, Isaac Woods.
 * See LICENCE.md
 */

#include <common.hpp>
#include <algorithm>
#include <cstring>
#include <cstddef>
#include <cstdarg>
#include <dirent.h>
#include <sys/stat.h>
#include <ast.hpp>

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

File::File(const std::string& name, const std::string& extension)
  :name(name)
  ,extension(extension)
{
}

Directory::Directory(const std::string& path)
  :path(path)
  ,files()
{
  DIR* d;
  struct dirent* dirent;

  if (!(d = opendir(path.c_str())))
  {
    fprintf(stderr, "FATAL: failed to open directory: '%s'\n", path.c_str());
    Crash();
  }

  while ((dirent = readdir(d)))
  {
    /*
     * XXX: If the path is of the form '~/.test', the extension may actually be the base name.
     * I don't really think this will be a problem (why would we compile in a hidden directory?) but we may need
     * to develop a more robust parsing solution for the path.
     */
    char* name = strdup(dirent->d_name);
    char* dotIndex = strchr(name, '.');
    files.push_back(File(std::string(dirent->d_name), (dotIndex ? std::string(dotIndex + (ptrdiff_t)1u) : "")));
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

char* ReadFile(const std::string& path)
{
  FILE* file = fopen(path.c_str(), "rb");

  if (!file)
  {
    fprintf(stderr, "Failed to read source file: %s\n", path.c_str());
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
    fprintf(stderr, "Failed to read source file: %s\n", path.c_str());
    Crash();
  }

  contents[length] = '\0';
  fclose(file);

  return contents;
}

bool DoesFileExist(const std::string& path)
{
  struct stat buffer;
  return (stat(path.c_str(), &buffer) == 0);
}
