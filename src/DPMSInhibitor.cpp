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


#ifdef BUILDFLAG_X11

#include "Inhibitor.hpp"
#include "util.hpp"

#include <X11/Xlib.h>
#include <X11/extensions/dpms.h>

#define THIS DPMSInhibitor

using namespace uinhibit;

THIS::THIS(std::function<void(Inhibitor*,Inhibit)> inhibitCB,
           std::function<void(Inhibitor*,Inhibit)> unInhibitCB) :
  Inhibitor(inhibitCB, unInhibitCB, "x11-dpms")
{
  if(!(this->dpy = XOpenDisplay(NULL))) {
    printf("[" ANSI_COLOR_RED "x" ANSI_COLOR_RESET "] X11-dpms (screen blanking): "
           "Cannot open display '%s'.\n", XDisplayName(NULL));
    return;
  }

  printf("[" ANSI_COLOR_GREEN "->" ANSI_COLOR_RESET "] X11-dpms (screen blanking): "
         "Feeding events, will disable screen blanking upon screensaver inhibit.\n");
  this->ok = true;
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
    DPMSDisable(this->dpy);
    XSync(this->dpy, False);
  } else {
    DPMSEnable(this->dpy);
    XSync(this->dpy, False);
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

#endif
