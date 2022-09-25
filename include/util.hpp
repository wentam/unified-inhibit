// Copyright (C) 2022 Matthew Egeler
//
// This file is part of unified-inhibit.
//
// unified-inhibit is free software: you can redistribute it and/or modify it under the terms of the
// GNU General Public License as published by the Free Software Foundation, either version 3 of the
// License, or (at your option) any later version.
//
// unified-inhibit is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
// without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along with unified-inhibit. If
// not, see <https://www.gnu.org/licenses/>.

#pragma once
#include <unistd.h>
#include <string>
#include <vector>
#include <set>
#include <map>

#define VERSION_MAJOR 0
#define VERSION_MINOR 2
#define VERSION_REVISION 1

static char _version[64];
static char* version() {
  sprintf(_version, "%d.%d.%d",VERSION_MAJOR,VERSION_MINOR,VERSION_REVISION);
  return _version;
}

#define ANSI_COLOR_BOLD    "\x1b[1m"
#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_BRIGHTRED "\x1b[1;31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"

#define ANSI_COLOR_BOLD_BLK "\x1B[1;30m"
#define ANSI_COLOR_BOLD_RED "\x1B[1;31m"
#define ANSI_COLOR_BOLD_GREEN "\x1B[1;32m"
#define ANSI_COLOR_BOLD_YELLOW "\x1B[1;33m"
#define ANSI_COLOR_BOLD_BLUE "\x1B[1;34m"
#define ANSI_COLOR_BOLD_MAGENTA "\x1B[1;35m"
#define ANSI_COLOR_BOLD_CYAN "\x1B[1;36m"
#define ANSI_COLOR_BOLD_WHITE "\x1B[1;37m"

#define ANSI_COLOR_RESET   "\x1b[0m"

static std::string strMerge(std::vector<std::string> strings, char sep) {
  std::string ret;
  for (auto str : strings) { for (auto c : str) ret.push_back(c); ret.push_back(sep); }
  if (ret.size() > 0) ret.pop_back();
  return ret;
};

static std::string strMerge(std::vector<std::string> strings) {
  std::string ret;
  for (auto str : strings) { for (auto c : str) ret.push_back(c); }
  return ret;
};

struct Args {
  std::set<char> flags;
  std::map<std::string, std::vector<std::string>> params;
};
