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

#define THIS XautolockInhibitor

using namespace uinhibit;

THIS::THIS(std::function<void(Inhibitor*,Inhibit)> inhibitCB,
           std::function<void(Inhibitor*,Inhibit)> unInhibitCB) :
  Inhibitor(inhibitCB, unInhibitCB, "xautolock")
{
  // TODO: cleaner way to detect if xautolock is running/exists?
  int32_t r = system("ps aux | grep xautolock | grep -v grep > /dev/null 2>/dev/null");
  bool xautolockRunning = (r == 0);
  r = system("xautolock -version > /dev/null 2> /dev/null");
  bool xautolockExists = (r == 0);

  if (!xautolockRunning) {
    printf("[" ANSI_COLOR_RED "x" ANSI_COLOR_RESET "] xautolock: "
           "Doesn't look like xautolock is running. \n");
  } else if (!xautolockExists) {
    printf("[" ANSI_COLOR_RED "x" ANSI_COLOR_RESET "] xautolock: "
           "xautolock looks like it's running, but we couldn't find the xautolock command. \n");
  } else {
    printf("[" ANSI_COLOR_GREEN "->" ANSI_COLOR_RESET "] xautolock: "
           "Feeding events with xautolock -disable/enable\n");
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

  if ((inhibited & InhibitType::SCREENSAVER) > 0) {
    int32_t r = system("xautolock -disable > /dev/null 2> /dev/null");
    if (r != 0) puts(ANSI_COLOR_YELLOW "Warning: failed to disable xautolock (return code)" ANSI_COLOR_RESET);
  } else {
    int32_t r = system("xautolock -enable > /dev/null 2> /dev/null");
    if (r != 0) puts(ANSI_COLOR_YELLOW "Warning: failed to enable xautolock (return code)" ANSI_COLOR_RESET);
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
