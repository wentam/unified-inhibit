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
#include <sys/inotify.h>
#include <fcntl.h>
#include "util.hpp"

#define THIS LinuxKernelInhibitInterface
#define WAKE_LOCK_PATH "/sys/power/wake_lock"
#define WAKE_UNLOCK_PATH "/sys/power/wake_unlock"

using namespace uinhibit;

THIS::THIS(std::function<void(InhibitInterface*,Inhibit)> inhibitCB,
           std::function<void(InhibitInterface*,Inhibit)> unInhibitCB,
           int32_t forkFD, int32_t forkOutFD) :
          InhibitInterface(inhibitCB, unInhibitCB, "linux-kernel-wakelock"),
          forkFD(forkFD)
{
  if (access(WAKE_LOCK_PATH, F_OK) != 0) {
    printf("[" ANSI_COLOR_RED "x" ANSI_COLOR_RESET "] Linux kernel wakelock: "
           WAKE_LOCK_PATH " doesn't exist. You probably don't have CONFIG_PM_WAKELOCKS enabled"
           " in your kernel.\n");
    ok = false;
  } else if (access(WAKE_UNLOCK_PATH, F_OK) != 0) {
    printf("[" ANSI_COLOR_RED "x" ANSI_COLOR_RESET "] Linux kernel wakelock: "
           WAKE_UNLOCK_PATH " doesn't exist. You probably don't have CONFIG_PM_WAKELOCKS enabled"
           " in your kernel.\n");
    ok = false;
  }

  char buf[8] = "";
  int64_t bytes = -1;
  if (ok
      && (bytes = read(forkOutFD, &buf, sizeof(buf))) != 0 
      && (strncmp(buf, "nowrite", 7) == 0)) {
    printf("[" ANSI_COLOR_RED "x" ANSI_COLOR_RESET "] Linux kernel wakelock: "
           "Don't have write access to " WAKE_LOCK_PATH ". You probably need to give me setuid "
           "(chown root uinhibitd && chmod 4755 uinhibitd).\n");

    ok = false;
  }

  if (ok)
    printf("[" ANSI_COLOR_GREEN "<->" ANSI_COLOR_RESET "] Linux kernel wakelock\n");
};

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
  /*int32_t inotifyFD = inotify_init();
  if (inotifyFD == -1) throw std::runtime_error("Failed to create inotify instance");

  int32_t inotifyLockWD = inotify_add_watch(inotifyFD, "/sys/power/wake_lock", IN_MODIFY);
  if (inotifyLockWD == -1) throw std::runtime_error("Failed to create inotify watch descriptor");

  int32_t inotifyUnlockWD = inotify_add_watch(inotifyFD, "/sys/power/wake_unlock", IN_MODIFY);
  if (inotifyUnlockWD == -1) throw std::runtime_error("Failed to create inotify watch descriptor");

  struct InotifyEvent {
    int      wd;
    uint32_t mask;
    uint32_t cookie;
    uint32_t len;
    char     name[NAME_MAX+1];
  } inotifyEvent;*/

  //while(read(inotifyFD, &inotifyEvent, sizeof(InotifyEvent)) > 0) {}

  // Unfortunately we must poll such that we capture locks that expire without inotify event
  while(1) {
    int32_t wakeLockFile = open(WAKE_LOCK_PATH, O_RDONLY); // Must open every time to get
                                                                   // latest info
    char buf[4096] = "";
    std::vector<std::string> locks;
    char prevChar = ' ';
    int64_t got = -1;
    while ((got = read(wakeLockFile, buf, 2)) > 0) {
      for (int i = 0; i < got; i++) {
        if (prevChar == ' ' && buf[i] != ' ' && buf[i] != '\n') locks.emplace_back();
        if (buf[i] != ' ' && buf[i] != '\n') locks.back().push_back(buf[i]);
        prevChar = buf[i];
      }
    }

    // Check for any locks we have not tracked yet
    for (auto lock : locks) {
      bool exists = false;
      // TODO activeInhibits mutex lock needed?
      for (auto& [id, in] : this->activeInhibits) {
        auto idStruct = reinterpret_cast<const _InhibitID*>(&id[0]);
        if (strcmp(idStruct->lockName, lock.c_str()) == 0) exists = true;
      }

      if (!exists) {
        std::unique_lock<std::mutex> lk(this->registerMutex);
        auto id = this->mkId(lock.c_str());

        if (!this->ourInhibits.contains(id))
          registerQueue.push_back({
            InhibitType::SUSPEND,
              "unknown-app",
              "unknown-reason",
              id,
              (uint64_t)time(NULL)
          });
      }
    }

    // Check for any locks we are tracking and have been removed
    for (auto& [id, in] : this->activeInhibits) {
      auto idStruct = reinterpret_cast<const _InhibitID*>(&id[0]);
      bool exists = false;
      for (auto lock : locks) {
        if (strcmp(idStruct->lockName, lock.c_str()) == 0) exists = true;
      }

      if (!exists) {
        std::unique_lock<std::mutex> lk(this->registerMutex);
        this->unregisterQueue.push_back(this->mkId(idStruct->lockName));
      }
    }

    close(wakeLockFile);
    usleep(500*1000);
  }
  //close(inotifyFD);
}

void THIS::lockFork(int32_t inFD, int32_t outFD) {
  if (clearenv() != 0) { printf("Failed to clear environment\n"); exit(1); }
  if (setresuid(0,0,0) != 0) {
    // All good, we will probably just fail to take any locks
  }

  if (access(WAKE_LOCK_PATH, W_OK) != 0) dprintf(outFD, "nowrite");
  else dprintf(outFD, "good");
  fsync(outFD);

  // Input format:
  // * "lockname\n" to take a lock
  // * "\tlockname\n" to remove a lock

  char buf[1024] = "";
  std::string token = "";
  int64_t got = -1;
  while((got = read(inFD, &buf, sizeof(buf))) > 0) { 
    for (int i = 0; i < got; i++) {
      if (buf[i] == '\n') {
        if (token.size() > 0) {
          token.erase(std::remove(token.begin(), token.end(), ' '), token.end());
          if (token.front() == '\t') {
            // Remove lock
            token.erase(0,1);
            int32_t wfd = open(WAKE_UNLOCK_PATH, O_WRONLY);
            dprintf(wfd, "%s\n", token.c_str());
            close(wfd);
          } else {
            // Place lock
            int32_t wfd = open(WAKE_LOCK_PATH, O_WRONLY);
            dprintf(wfd, "%s\n", token.c_str());
            close(wfd);
          }
        }

        token.clear();
      } else {
        token.push_back(buf[i]);
      }
    }
  }

  exit(0);
}

Inhibit THIS::doInhibit(InhibitRequest r) { 
  if ((r.type & InhibitType::SUSPEND) == InhibitType::NONE)
    throw uinhibit::InhibitRequestUnsupportedTypeException();

  // TODO: appname-reason isn't gauranteed to be completely unique.
  // Ideally we'd use InhibitID in some way, but it's an opaque type currently.
  // InhibitID should probably be refactored to be a class that has a 'serialize' method.
  //
  // Alternatively InhibitID could just a be string-encoded type. This would get around the
  // annoyances of using custom types in a std::map etc

  std::unique_lock<std::mutex> lk(this->registerMutex);

  auto id = this->mkId((r.appname+"-"+r.reason).c_str());

  this->ourInhibits.insert(id);

  // Tell our setuid fork to add the inhibit  
  dprintf(forkFD, "%s-%s\n", r.appname.c_str(), r.reason.c_str());

  return {
    InhibitType::SUSPEND,
    r.appname,
    r.reason,
    id,
    (uint64_t)time(NULL),
  };
}

void THIS::doUnInhibit(InhibitID id) {
  if (this->activeInhibits.contains(id)) {
    std::unique_lock<std::mutex> lk(this->registerMutex);
    auto r = this->activeInhibits[id];
    auto id = this->mkId((r.appname+"-"+r.reason).c_str());
    dprintf(forkFD, "\t%s-%s\n", r.appname.c_str(), r.reason.c_str());
    this->ourInhibits.erase(id);
  }
}

void THIS::handleInhibitEvent(Inhibit inhibit) {}
void THIS::handleUnInhibitEvent(Inhibit inhibit) {}
void THIS::handleInhibitStateChanged(InhibitType inhibited, Inhibit inhibit) {}

InhibitID THIS::mkId(const char* lockName) {  
  std::string cleanLockName(lockName);
  cleanLockName.erase(std::remove(cleanLockName.begin(), cleanLockName.end(), ' '),
                      cleanLockName.end());

  _InhibitID idStruct = {this->instanceId, {}};
  strncpy(idStruct.lockName, cleanLockName.c_str(), sizeof(idStruct.lockName)-1);

  auto ptr = reinterpret_cast<std::byte*>(&idStruct);
  InhibitID id(ptr, ptr+sizeof(idStruct));
  return id;
}
