#pragma once
#include <unistd.h>

#define ANSI_COLOR_BOLD    "\x1b[1m"
#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_BRIGHTRED "\x1b[1;31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"

static std::string strMerge(std::vector<std::string> strings, char sep) {
  std::string ret;
  for (auto str : strings) { for (auto c : str) ret.push_back(c); ret.push_back(sep); }
  ret.pop_back();
  return ret;
};

static std::string strMerge(std::vector<std::string> strings) {
  std::string ret;
  for (auto str : strings) { for (auto c : str) ret.push_back(c); }
  return ret;
};
