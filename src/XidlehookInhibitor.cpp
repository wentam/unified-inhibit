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


#include "Inhibitor.hpp"
#include "util.hpp"

#define THIS XidlehookInhibitor
#define SOCKETPATH "/tmp/xidlehook.sock"

using namespace uinhibit;

THIS::THIS(std::function<void(Inhibitor*,Inhibit)> inhibitCB,
           std::function<void(Inhibitor*,Inhibit)> unInhibitCB) : Inhibitor(inhibitCB, unInhibitCB)
{
  // TODO: cleaner way to detect if xidlehook is running/exists?
  int32_t r = system("ps aux | grep xidlehook | grep -v grep > /dev/null 2>/dev/null");
  bool xidlehookRunning = (r == 0);
  r = system("xidlehook --version > /dev/null 2> /dev/null");
  bool xidlehookExists = (r == 0);
  r = access(SOCKETPATH, F_OK);
  bool socketExists = (r == 0);

  if (!xidlehookRunning) {
    printf("[" ANSI_COLOR_RED "x" ANSI_COLOR_RESET "] xidlehook: "
           "Doesn't look like xidlehook is running. Make sure you start it with xidlehook --socket " SOCKETPATH " \n");
  } else if (!xidlehookExists) {
    printf("[" ANSI_COLOR_RED "x" ANSI_COLOR_RESET "] xidlehook: "
           "xidlehook looks like it's running, but we couldn't find the xidlehook command. \n");
  } else if (!socketExists) {
    printf("[" ANSI_COLOR_RED "x" ANSI_COLOR_RESET "] xidlehook: "
           "xidlehook looks like it's running, but we couldn't find the socket at " SOCKETPATH "."
           " Make sure you start xidlehook with '--socket " SOCKETPATH "'\n");
  } else {
    printf("[" ANSI_COLOR_GREEN "->" ANSI_COLOR_RESET "] xidlehook: "
           "Feeding events with xidlehook-client --socket " SOCKETPATH " control --action Disable/Enable\n");
    this->ok = true;
  }
}

Inhibitor::ReturnObject THIS::start() {
  while(1) co_await std::suspend_always();
}

void THIS::handleInhibitStateChanged(InhibitType inhibited, Inhibit inhibit) {
  if (!this->ok) return;

  // Just because the inhibit state changed doesn't mean the screensaver type has changed.
  // We only care when screensaver type has changed
  if ((inhibited & InhibitType::SCREENSAVER) == (lastInhibited & InhibitType::SCREENSAVER)) return;

  if (inhibited & InhibitType::SCREENSAVER) {
    int32_t r = system("xidlehook-client --socket " SOCKETPATH " control --action Disable > /dev/null 2> /dev/null");
    if (r != 0) puts(ANSI_COLOR_YELLOW "Warning: failed to disable xidlehook (return code)" ANSI_COLOR_RESET);
  } else {
    int32_t r = system("xidlehook-client --socket " SOCKETPATH " control --action Enable > /dev/null 2> /dev/null");
    if (r != 0) puts(ANSI_COLOR_YELLOW "Warning: failed to enable xidlehook (return code)" ANSI_COLOR_RESET);
  }

  lastInhibited = inhibited;
};

Inhibit THIS::doInhibit(InhibitRequest r) {
  Inhibit ret = {};
  ret.type = r.type;
  ret.appname = r.appname;
  ret.reason = r.reason;
  ret.id = {};
  ret.created = time(NULL);
  return ret;
}
