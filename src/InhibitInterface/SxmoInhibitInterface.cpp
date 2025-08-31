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


#include "InhibitInterface.hpp"
#include "util.hpp"
#include <sys/inotify.h>
#include <algorithm>

#define THIS SxmoInhibitInterface

using namespace uinhibit;

THIS::THIS(std::function<void(InhibitInterface*,Inhibit)> inhibitCB,
           std::function<void(InhibitInterface*,Inhibit)> unInhibitCB) :
  InhibitInterface(inhibitCB, unInhibitCB, "sxmo")
{
  int32_t r = system("sxmo_mutex.sh can_suspend list > /dev/null 2> /dev/null");
  bool sxmoMutexExists = (r == 0);

  if (!sxmoMutexExists) {
    printf("[" ANSI_COLOR_RED "x" ANSI_COLOR_RESET "] Sxmo: "
           "Can't find sxmo_mutex.sh. You probably don't have Sxmo. \n");
  } else {
    printf("[" ANSI_COLOR_GREEN "<->" ANSI_COLOR_RESET "] Sxmo: "
           "Feeding via 'sxmo_mutex.sh can_suspend lock|free'. Will map sent screensaver+suspend"
           " events to can_suspend. Will also read suspend events. If running in ssh/tty, you need"
           " to run 'export DBUS_SESSION_BUS_ADDRESS=$(cat $XDG_RUNTIME_DIR/dbus.bus)' before"
           " starting uinhibitd\n");
    this->ok = true;
  }
}

InhibitInterface::ReturnObject THIS::start() {
  while (!ok) co_await std::suspend_always();

  std::jthread(&THIS::watcherThread, this).detach();


  while(1) {
    {
      std::unique_lock<std::mutex> lk(this->registerMutex);
      for (auto& r : this->registerQueue) this->registerInhibit(r);
      for (auto& r : this->unregisterQueue) this->registerUnInhibit(r);
      this->registerQueue.clear();
      this->unregisterQueue.clear();
    }
    co_await std::suspend_always();
  }
}

void THIS::watcherThread() {
  std::string xdgRuntimeDir = getenv("XDG_RUNTIME_DIR");
  std::string can_suspend = xdgRuntimeDir + "/sxmo_mutex/can_suspend";
  std::string can_suspend_dir = xdgRuntimeDir + "/sxmo_mutex/";

  int32_t inotifyFD = inotify_init();
  if (inotifyFD == -1) throw std::runtime_error("Failed to create inotify instance");

  int32_t inotifyLockWD = inotify_add_watch(inotifyFD,
                                            can_suspend.c_str(),
                                            IN_MODIFY | IN_DELETE_SELF);
  if (inotifyLockWD == -1) throw std::runtime_error("Failed to create inotify watch descriptor");


  int32_t inotifyLockWD2 = inotify_add_watch(inotifyFD, can_suspend_dir.c_str(), IN_CREATE);
  if (inotifyLockWD2 == -1) throw std::runtime_error("Failed to create inotify watch descriptor");

  struct InotifyEvent {
    int      wd;
    uint32_t mask;
    uint32_t cookie;
    uint32_t len;
    char     name[NAME_MAX+1];
  } inotifyEvent;

  size_t got;
  while((got = read(inotifyFD, &inotifyEvent, sizeof(InotifyEvent)) > 0)) {
    int64_t i = 0;
    while ( i < got ) {
      InotifyEvent* event = (InotifyEvent*) &((&inotifyEvent)[i]);
      i += sizeof(InotifyEvent) + event->len;

      // can_suspend gets constantly deleted and recreated. Need to remake our watch when this
      // happens.
      if (((event->mask & IN_IGNORED) > 0)
          || ((event->mask & IN_DELETE_SELF) > 0)
          || (event->mask & IN_CREATE) > 0) {
        inotify_rm_watch(inotifyFD, inotifyLockWD);
        inotifyLockWD = inotify_add_watch(inotifyFD,
                                          can_suspend.c_str(),
                                          IN_MODIFY | IN_DELETE_SELF);
        if (inotifyLockWD == -1)
          throw std::runtime_error("Failed to create inotify watch descriptor");
      }

      FILE* p = popen("sxmo_mutex.sh can_suspend list", "r");
      if (p == NULL) {
        puts(ANSI_COLOR_YELLOW
             "Warning: failed to run sxmo_mutex.sh to check for new events"
             ANSI_COLOR_RESET);
        sleep(30);
        continue;
      }

      std::string output;
      char buf[1024];
      while((fgets(buf, 1024, p)) != NULL) output += buf;

      std::vector<std::string> tokens;
      tokens.push_back("");
      for (auto c : output) {
        if (c == '\n') tokens.push_back("");
        else tokens.back().push_back(c);
      }

      if (tokens.size() > 0 && tokens.back() == "") tokens.pop_back();

      tokens.erase(
        std::remove(tokens.begin(), tokens.end(), "Playing with leds"),
        tokens.end()
        );

      tokens.erase(
        std::remove(tokens.begin(), tokens.end(), "Checking some mutexes"),
        tokens.end()
        );

      // Check for any locks we have not tracked yet
      for (auto tok : tokens) {
        std::unique_lock<std::mutex> lk(this->registerMutex);

        bool exists = false;
        // TODO activeInhibits mutex lock needed?
        for (auto& [id, in] : this->activeInhibits) {
          auto idStruct = reinterpret_cast<const _InhibitID*>(&id[0]);
          if (strcmp(idStruct->token, tok.c_str()) == 0) exists = true;
        }

        for (auto& in : this->registerQueue) {
          auto idStruct = reinterpret_cast<const _InhibitID*>(&in.id[0]);
          if (strcmp(idStruct->token, tok.c_str()) == 0) exists = true;
        }

        if (!exists) {
          auto id = this->mkId(tok.c_str());

          if (!this->ourInhibits.contains(id)) {
            registerQueue.push_back({
              InhibitType::SUSPEND,
                "unknown-app",
                tok,
                id,
                (uint64_t)time(NULL)
            });
          }
        }
      }

      // Check for any lokcs we are tracking and have been removed
      for (auto& [id, in] : this->activeInhibits) {
        auto idStruct = reinterpret_cast<const _InhibitID*>(&id[0]);
        bool exists = false;
        for (auto tok : tokens) if (strcmp(idStruct->token, tok.c_str()) == 0) exists = true;

        if (!exists) {
          std::unique_lock<std::mutex> lk(this->registerMutex);
          this->unregisterQueue.push_back(id);
        }
      }

      pclose(p);
    }
  }

  printf(ANSI_COLOR_YELLOW "Warning: sxmo read loop broke!\n" ANSI_COLOR_RESET);

  close(inotifyFD);
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

  std::string token = this->mkToken(r.appname, r.reason);

  std::string cmd = "sxmo_mutex.sh can_suspend lock \""+token+"\"";
  int32_t rr = system(cmd.c_str());
  if (rr != 0) puts(ANSI_COLOR_YELLOW "Warning: failed to set sxmo lock" ANSI_COLOR_RESET);

  Inhibit ret = {};
  ret.type = r.type;
  ret.appname = r.appname;
  ret.reason = r.reason;
  ret.id = this->mkId(token);
  ret.created = time(NULL);

  this->ourInhibits.insert(ret.id);

  return ret;
}

void THIS::doUnInhibit(InhibitID id) {
  if (!this->ok) return;

  this->ourInhibits.erase(id);

  auto idStruct = reinterpret_cast<_InhibitID*>(&id[0]);
  std::string token = idStruct->token;

  std::string cmd = "sxmo_mutex.sh can_suspend free \""+token+"\"";
  int32_t r = system(cmd.c_str());
  if (r != 0) puts(ANSI_COLOR_YELLOW "Warning: failed to release sxmo lock" ANSI_COLOR_RESET);
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

std::string THIS::mkToken(std::string appname, std::string reason) {
  std::string cleanReason;
  for (auto c : reason) if (c != '"') cleanReason.push_back(c);

  std::string cleanAppname;
  for (auto c : appname) {
    if (c != '"') cleanAppname.push_back(c);
    if (c == '/') cleanAppname.clear();
  }

  if (cleanAppname.size() > 0) cleanAppname.at(0) = toupper(cleanAppname.at(0));

  std::string token = cleanAppname+" - "+cleanReason;
  return token;
}
