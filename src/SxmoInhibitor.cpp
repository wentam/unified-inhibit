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

#define THIS SxmoInhibitor

using namespace uinhibit;

THIS::THIS(std::function<void(Inhibitor*,Inhibit)> inhibitCB,
           std::function<void(Inhibitor*,Inhibit)> unInhibitCB) :
  Inhibitor(inhibitCB, unInhibitCB, "sxmo")
{
  int32_t r = system("sxmo_mutex.sh can_suspend list > /dev/null 2> /dev/null");
  bool sxmoMutexExists = (r == 0);

  if (!sxmoMutexExists) {
    printf("[" ANSI_COLOR_RED "x" ANSI_COLOR_RESET "] Sxmo: "
           "Can't find sxmo_mutex.sh. You probably don't have Sxmo. \n");
  } else {
    printf("[" ANSI_COLOR_GREEN "->" ANSI_COLOR_RESET "] Sxmo: "
           "Feeding events via 'sxmo_mutex.sh can_suspend lock|free'. Will map screensaver+suspend events to can_suspend."
           " If running in ssh, you need to run"
           " 'export DBUS_SESSION_BUS_ADDRES=$(cat $XDG_RUNTIME_DIR/dbus.bus)' before starting"
           " uinhibitd\n");
    this->ok = true;
  }
}

Inhibitor::ReturnObject THIS::start() {
  while(1) co_await std::suspend_always();
}

Inhibit THIS::doInhibit(InhibitRequest r) {
  if (((r.type & InhibitType::SCREENSAVER) == InhibitType::NONE)
      && ((r.type & InhibitType::SUSPEND) == InhibitType::NONE))
    throw uinhibit::InhibitRequestUnsupportedTypeException();

  // TODO: it would be better to inform the top level not to call into us in the first place
  // in this situation
  if (!this->ok) {
    Inhibit ret = {};
    ret.type = r.type;
    ret.appname = r.appname;
    ret.reason = r.reason;
    ret.id = {};
    ret.created = time(NULL);
    return ret;
  }

  std::string cleanReason;
  for (auto c : r.reason) if (c != '"') cleanReason.push_back(c);

  std::string cleanAppname;
  for (auto c : r.appname) {
    if (c != '"') cleanAppname.push_back(c);
    if (c == '/') cleanAppname.clear();
  }

  std::string token = cleanAppname+" "+cleanReason;

  std::string cmd = "sxmo_mutex.sh can_suspend lock \""+token+"\"";
  printf("Running cmd: %s\n", cmd.c_str());
  system(cmd.c_str());

  Inhibit ret = {};
  ret.type = r.type;
  ret.appname = r.appname;
  ret.reason = r.reason;
  ret.id = this->mkId(token);
  ret.created = time(NULL);
  return ret;
}

void THIS::doUnInhibit(InhibitID id) {
  if (!this->ok) return;

  auto idStruct = reinterpret_cast<_InhibitID*>(&id[0]);
  std::string token = idStruct->token;

  std::string cmd = "sxmo_mutex.sh can_suspend free \""+token+"\"";
  printf("Running cmd: %s\n", cmd.c_str());
  system(cmd.c_str());
}

InhibitID THIS::mkId(std::string token) {
  _InhibitID idStruct = {this->instanceId, {}};

  uint32_t size = (token.size()>sizeof(idStruct.token)-1) ? sizeof(idStruct.token)-1 : token.size();
  strncpy(idStruct.token, token.c_str(), size);
  if(size>0) idStruct.token[size]=0; // NULL terminate

  auto ptr = reinterpret_cast<std::byte*>(&idStruct);
  InhibitID id(ptr, ptr+sizeof(idStruct));
  return id;
}
