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

#define THIS CinnamonScreenSaverInhibitor
#define METHOD_CAST (void (DBusInhibitor::*)(DBus::Message* msg, DBus::Message* retmsg))
#define SIGNAL_CAST (void (DBusInhibitor::*)(DBus::Message* msg))
#define INTERFACE "org.cinnamon.ScreenSaver"
#define DBUS_INTERFACE "org.freedesktop.DBus"
#define INTROSPECT_INTERFACE "org.freedesktop.DBus.Introspectable"
#define PATH "/org/cinnamon/ScreenSaver"
#define RELPATH "org/cinnamon/ScreenSaver"

using namespace uinhibit;

THIS::THIS(std::function<void(Inhibitor*, Inhibit)> inhibitCB,
           std::function<void(Inhibitor*, Inhibit)> unInhibitCB)
  : DBusInhibitor
    (inhibitCB, unInhibitCB, INTERFACE, DBUS_BUS_SESSION,
     {
       {INTERFACE, "SimulateUserActivity", METHOD_CAST &THIS::handleSimActivityMsg, "*"},
       {INTROSPECT_INTERFACE, "Introspect", METHOD_CAST &THIS::handleIntrospect, INTERFACE}
     },
     {
      {DBUS_INTERFACE, "NameOwnerChanged", SIGNAL_CAST &THIS::handleNameLostMsg}
     }){}


void THIS::handleSimActivityMsg(DBus::Message* msg, DBus::Message* retmsg) {
  // We treat this as an inhibit that expires in 5min
  Inhibit i = {
    InhibitType::SCREENSAVER,
    msg->sender(),
    "SimulateUserActivity",
    this->mkId(msg->sender()),
    (uint64_t)time(NULL)
  };

  // Clear any existing simActivity inhibits from this sender
  std::vector<InhibitID> eraseIDs;
  for (auto& [id, in] : this->activeInhibits) {
    auto idc = id;
    auto idStruct = reinterpret_cast<const _InhibitID*>(&id[0]);
    if (idStruct->sender == msg->sender()) eraseIDs.push_back(idc);
  }
  for (auto& id : eraseIDs) this->registerUnInhibit(id);
  this->inhibitOwners[std::string(msg->sender())].clear();

  // Register new inhibit
  this->registerInhibit(i);
  this->inhibitOwners[std::string(msg->sender())].push_back(i.id);

  if (!this->monitor) msg->newMethodReturn().send();
}

void THIS::handleNameLostMsg(DBus::Message* msg) {
  const char* name; msg->getArgs(DBUS_TYPE_STRING, &name, DBUS_TYPE_INVALID);
  for (auto id : this->inhibitOwners[name]) this->registerUnInhibit(id);
  this->inhibitOwners[name].clear();
}

void THIS::handleIntrospect(DBus::Message* msg, DBus::Message* retmsg) {
  if (this->monitor || std::string(msg->destination()) != this->interface) return;

  if (std::string(msg->path()) == "/")  {
    std::string xml = DBUS_INTROSPECT_1_0_XML_DOCTYPE_DECL_NODE
      "<node name='/'>"
      "  <node name='" RELPATH "' />"
      "</node>";

    const char* introspectXml = xml.c_str();
    msg->newMethodReturn().appendArgs(DBUS_TYPE_STRING,&introspectXml,DBUS_TYPE_INVALID)->send();
  }

  if (std::string(msg->path()) == PATH)  {
    std::string xml = DBUS_INTROSPECT_1_0_XML_DOCTYPE_DECL_NODE
      "<node name='" RELPATH "'>"
      "  <interface name=\"" INTERFACE "\">"
      "    <method name='SimulateUserActivity' />"
      "  </interface>"
      "</node>";

    const char* introspectXml = xml.c_str();
    msg->newMethodReturn().appendArgs(DBUS_TYPE_STRING,&introspectXml,DBUS_TYPE_INVALID)->send();
  }
}

static void simThread(std::stop_token stop_token, DBus* callDbus) {
  while(!stop_token.stop_requested()) {
    callDbus->newMethodCall(INTERFACE, PATH, INTERFACE, "SimulateUserActivity").send();
    sleep(60);
  }
}

Inhibit THIS::doInhibit(InhibitRequest r) {
  if ((r.type & InhibitType::SCREENSAVER) == InhibitType::NONE)
    throw uinhibit::InhibitRequestUnsupportedTypeException();

  this->lastUsInhibit++;
  std::string sender = "us"+std::to_string(lastUsInhibit);
  auto id = this->mkId(sender);

  if (this->monitor) {
    simThreads.emplace(std::piecewise_construct,
                       std::forward_as_tuple(sender),
                       std::forward_as_tuple(&simThread, this->callDbus.get()));
  }

  return {InhibitType::SCREENSAVER, r.appname, r.reason, id, (uint64_t)time(NULL)};
};

void THIS::doUnInhibit(InhibitID id) {
  auto idStruct = reinterpret_cast<_InhibitID*>(&id[0]);
  if (this->monitor && this->simThreads.contains(idStruct->sender)) {
    this->simThreads.at(idStruct->sender).request_stop();
    this->simThreads.at(idStruct->sender).detach();
    this->simThreads.erase(idStruct->sender);
  }
}

void THIS::poll() {
  // Unregister any expired inhibits
  std::vector<InhibitID> eraseIDs;
  for (auto& [id, in] : this->activeInhibits) {
    auto idc = id;
    auto idStruct = reinterpret_cast<const _InhibitID*>(&id[0]);
    if (in.created <= (time(NULL)-(60*5))) {
      this->inhibitOwners[idStruct->sender].clear();
      eraseIDs.push_back(idc);
    }
  }

  for (auto& id : eraseIDs) this->registerUnInhibit(id);
}

InhibitID THIS::mkId(std::string sender) {
  _InhibitID idStruct = {this->instanceId, {}};

  uint32_t size = (sender.size()>sizeof(idStruct.sender)) ? sizeof(idStruct.sender) : sender.size();
  strncpy(idStruct.sender, sender.c_str(), size);
  if(size>0) idStruct.sender[size]=0; // NULL terminate

  auto ptr = reinterpret_cast<std::byte*>(&idStruct);
  InhibitID id(ptr, ptr+sizeof(idStruct));
  return id;
}
