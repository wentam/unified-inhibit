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

#define THIS GnomeScreenSaverInhibitor
#define METHOD_CAST (void (DBusInhibitor::*)(DBus::Message* msg, DBus::Message* retmsg))
#define SIGNAL_CAST (void (DBusInhibitor::*)(DBus::Message* msg))
#define INTERFACE "org.gnome.ScreenSaver"
#define DBUS_INTERFACE "org.freedesktop.DBus"
#define INTROSPECT_INTERFACE "org.freedesktop.DBus.Introspectable"

using namespace uinhibit;

THIS::THIS(std::function<void(Inhibitor*, Inhibit)> inhibitCB, 
           std::function<void(Inhibitor*, Inhibit)> unInhibitCB)
  : SimpleDBusInhibitor
    (inhibitCB, unInhibitCB,
     {
       {INTERFACE, "SimulateUserActivity", METHOD_CAST &THIS::handleSimActivityMsg, "*"},
     },
     {},
     INTERFACE,
     "/org/gnome/ScreenSaver",
     InhibitType::SCREENSAVER,  
     "<method name='SimulateUserActivity' />")
{}

void THIS::handleSimActivityMsg(DBus::Message* msg, DBus::Message* retmsg) {
  // We treat this as an inhibit that expires in 5min
  // We use a cookie of 0 to represent sim activity inhibits.
  Inhibit i = {
    this->inhibitType,
    msg->sender(),
    "SimulateUserActivity",
    this->mkId(msg->sender(), 0),
    (uint64_t)time(NULL)
  };

  // Clear any existing simActivity inhibits from this sender 
  std::vector<InhibitID> eraseIDs;
  for (auto& [id, in] : this->activeInhibits) {
    auto idc = id;
    auto idStruct = reinterpret_cast<const _InhibitID*>(&id[0]);
    if (idStruct->cookie == 0 && idStruct->sender == msg->sender()) eraseIDs.push_back(idc);
  }
  for (auto& id : eraseIDs) this->registerUnInhibit(id);
  this->inhibitOwners[std::string(msg->sender())].clear();

  // Register new inhibit
  this->registerInhibit(i);
  this->inhibitOwners[std::string(msg->sender())].push_back(i.id);

  if (!this->monitor) msg->newMethodReturn().send();
}

void THIS::poll() {
  // Unregister any expired inhibits  
  std::vector<InhibitID> eraseIDs;
  for (auto& [id, in] : this->activeInhibits) {
    auto idc = id;
    auto idStruct = reinterpret_cast<const _InhibitID*>(&id[0]);
    if (idStruct->cookie == 0 && in.created <= (time(NULL)-(60*5))) {
      this->inhibitOwners[idStruct->sender].clear();
      eraseIDs.push_back(idc);
    }
  }

  for (auto& id : eraseIDs) this->registerUnInhibit(id);
}
