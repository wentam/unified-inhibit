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
#include <cstdio>
#include <unistd.h>
#include "util.hpp"
#include <cstring>

#define THIS FreedesktopPowerManagerInhibitor
#define METHOD_CAST (void (DBusInhibitor::*)(DBus::Message* msg, DBus::Message* retmsg))
#define SIGNAL_CAST (void (DBusInhibitor::*)(DBus::Message* msg))
#define INTERFACE "org.freedesktop.PowerManager"
#define DBUS_INTERFACE "org.freedesktop.DBus"
#define INTROSPECT_INTERFACE "org.freedesktop.DBus.Introspectable"

using namespace uinhibit;

THIS::THIS(std::function<void(Inhibitor*, Inhibit)> inhibitCB,
           std::function<void(Inhibitor*, Inhibit)> unInhibitCB)
  : SimpleDBusInhibitor
    (inhibitCB, unInhibitCB, INTERFACE,
     {
       {INTERFACE, "HasInhibit", METHOD_CAST &THIS::handleHasInhibitMsg, "*"},
     },
     {},
     INTERFACE,
     "/PowerManager",
     InhibitType::SUSPEND,
      "<method name='HasInhibit'>"
      "  <arg type='b' name='has_inhibit' direction='out'/>"
      "</method>"
      "<signal name='HasInhibitChanged'>"
      "  <arg type='b' name='has_inhibit' direction='out'/>"
      "</signal>"){}

void THIS::handleHasInhibitMsg(DBus::Message* msg, DBus::Message* retmsg) {
  if (this->monitor) return;

  bool ret = ((this->inhibited() & this->inhibitType) > 0);
  int32_t boolRet = (ret) ? 1 : 0;
  msg->newMethodReturn().appendArgs(DBUS_TYPE_BOOLEAN, &boolRet, DBUS_TYPE_INVALID)->send();
}

void THIS::handleInhibitStateChanged(InhibitType inhibited, Inhibit inhibit) {
  if (this->monitor) return; // If we are monitoring, this should be generated by whoever
                             // has this implemented

  int32_t send = 0;
  if (inhibited & this->inhibitType) send = 1;
  dbus.newSignal(("/"+this->path).c_str(), this->interface.c_str(), "HasInhibitChanged")
    .appendArgs(DBUS_TYPE_BOOLEAN, &send, DBUS_TYPE_INVALID)
    ->send();
}
