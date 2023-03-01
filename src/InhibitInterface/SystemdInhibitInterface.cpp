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
#include <cstdio>
#include <unistd.h>
#include "util.hpp"
#include <cstring>

namespace cpoll {
#include <poll.h>
}

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <thread>
#include <dirent.h>

#define THIS SystemdInhibitInterface
#define METHOD_CAST (void (DBusInhibitInterface::*)(DBus::Message* msg, DBus::Message* retmsg))
#define SIGNAL_CAST (void (DBusInhibitInterface::*)(DBus::Message* msg))
#define INTERFACE "org.freedesktop.login1.Manager"
#define DBUSNAME "org.freedesktop.login1"
#define DBUS_INTERFACE "org.freedesktop.DBus"
#define INTROSPECT_INTERFACE "org.freedesktop.DBus.Introspectable"
#define PATH "/org/freedesktop/login1"

using namespace uinhibit;

THIS::THIS(std::function<void(InhibitInterface*, Inhibit)> inhibitCB,
           std::function<void(InhibitInterface*, Inhibit)> unInhibitCB,
           SystemdInhibitFork* inhibitFork)
  : DBusInhibitInterface
    (inhibitCB, unInhibitCB, DBUSNAME, DBUSNAME, DBUS_BUS_SYSTEM,
     {
       {INTERFACE, "Inhibit", METHOD_CAST &THIS::handleInhibitMsg, "*"},
       {INTERFACE, "ListInhibitors", METHOD_CAST &THIS::handleListInhibitorsMsg, "*"},
       {"org.freedesktop.DBus.Properties", "Get", METHOD_CAST &THIS::handleGetProperty, "*"},
       {INTROSPECT_INTERFACE, "Introspect", METHOD_CAST &THIS::handleIntrospect, INTERFACE}
     },
     {}),
    inhibitFork(inhibitFork)
{
  this->forkSender = this->inhibitFork->rx();
  this->forkSender.pop_back(); // Remove trailing newline
}

void THIS::handleIntrospect(DBus::Message* msg, DBus::Message* retmsg) {
  if (this->monitor || std::string(msg->destination()) != DBUSNAME) return;

  if (std::string(msg->path()) == "/")  {
    const char* introspectXml = DBUS_INTROSPECT_1_0_XML_DOCTYPE_DECL_NODE
      "<node name='/'>"
      "  <node name='org/freedesktop/login1' />"
      "</node>";

    msg->newMethodReturn().appendArgs(DBUS_TYPE_STRING,&introspectXml,DBUS_TYPE_INVALID)->send();
  }

  if (std::string(msg->path()) == "/org/freedesktop/login1")  {
    const char* introspectXml = DBUS_INTROSPECT_1_0_XML_DOCTYPE_DECL_NODE
      "<node name='org/freedesktop/login1'>"
      "  <interface name='" INTERFACE "'>"
      "    <method name='Inhibit'>"
      "      <arg name='what' type='s' direction='in'/>"
      "      <arg name='who' type='s' direction='in'/>"
      "      <arg name='why' type='s' direction='in'/>"
      "      <arg name='mode' type='s' direction='in'/>"
      "      <arg name='fd' type='h' direction='out'/>"
      "    </method>"
      "    <method name='ListInhibitors'>"
      "      <arg name='inhibitor_list' type='a(ssssuu)' direction='out'/>"
      "    </method>"
      "    <property name='BlockInhibited' type='s' access='read'/>"
      "  </interface>"
      "</node>";

    msg->newMethodReturn().appendArgs(DBUS_TYPE_STRING,&introspectXml,DBUS_TYPE_INVALID)->send();
  }
}

void THIS::handleGetProperty(DBus::Message* msg, DBus::Message* retmsg) {
  if (this->monitor) return;
  const char* interface; const char* property;
  msg->getArgs(DBUS_TYPE_STRING, &interface, DBUS_TYPE_STRING, &property, DBUS_TYPE_INVALID);

  if (std::string(interface) == INTERFACE) {
    if (std::string(property) == "BlockInhibited") {
      std::string what = us2systemdType(this->inhibited());
      const char* str = what.c_str();
      msg->newMethodReturn().appendArgs(DBUS_TYPE_STRING, &str, DBUS_TYPE_INVALID)->send();
    }
  }
}

void THIS::handleListInhibitorsMsg(DBus::Message* msg, DBus::Message* retmsg) {
  if (this->monitor) return;

  struct Minhibitor {
    const char* what;
    const char* who;
    const char* why;
    const char* mode;
    uint32_t pid;
    uint32_t uid;
  };

  std::vector<Minhibitor> list;
  for (auto& [id, in] : this->activeInhibits) {
    std::string whatstr = us2systemdType(in.type);

    Minhibitor mmin = {
      .what = whatstr.c_str(),
      .who = in.appname.c_str(),
      .why = in.reason.c_str(),
      .mode = "block", // TODO support lock expiry?
      .pid = this->pidUids[id].pid,
      .uid = this->pidUids[id].uid,
    };

    list.push_back(mmin);
  }

  // TODO abstract this properly in DBus wrapper
  auto ret = msg->newMethodReturn();
  auto mmsg = ret.msg.get()->msg;

  DBusMessageIter iter;
  dbus_message_iter_init_append(mmsg, &iter);

  DBusMessageIter arrayIter;
  dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "(ssssuu)", &arrayIter);

  for (auto min : list) {
    DBusMessageIter structIter;
    dbus_message_iter_open_container(&arrayIter, DBUS_TYPE_STRUCT, NULL, &structIter);
    dbus_message_iter_append_basic(&structIter, DBUS_TYPE_STRING, &(min.what));
    dbus_message_iter_append_basic(&structIter, DBUS_TYPE_STRING, &(min.who));
    dbus_message_iter_append_basic(&structIter, DBUS_TYPE_STRING, &(min.why));
    dbus_message_iter_append_basic(&structIter, DBUS_TYPE_STRING, &(min.mode));
    dbus_message_iter_append_basic(&structIter, DBUS_TYPE_UINT32, &(min.uid));
    dbus_message_iter_append_basic(&structIter, DBUS_TYPE_UINT32, &(min.pid));
    dbus_message_iter_close_container(&arrayIter, &structIter);
  }

  dbus_message_iter_close_container(&iter, &arrayIter);

  ret.send();
}

void THIS::releaseThread(const char* path, Inhibit in, bool delay) {
  uint64_t startTime = time(NULL);

  // We don't have read access to wait for EOF, so we just need to wait until the file goes away
  // TODO: could probably use inotify for this
  while (1) {

    // Systemd keeps the fd around for delay locks even after the delay time
    // This delay time is system-configurable, but defaults to 5 seconds.
    // We will just release any delay locks after 5 seconds.
    if (delay && time(NULL)-startTime > 5) {
      {
        std::unique_lock<std::mutex> lk(this->releaseQueueMutex);
        this->releaseQueue.push_back(in);
      }
      break;
    }

    int r = access(path, F_OK);
    if (r != 0) {
      {
        std::unique_lock<std::mutex> lk(this->releaseQueueMutex);
        this->releaseQueue.push_back(in);
      }
      break;
    }
    usleep(100*1000);
  }
}

void THIS::releaseThreadOurFd(int32_t fd, std::string path, Inhibit in) {
  struct cpoll::pollfd pollfd = {
    .fd = fd,
    .events = POLLIN,
    .revents = 0
  };
  cpoll::poll(&pollfd, 1, -1);
  unlink(path.c_str());
  {
    std::unique_lock<std::mutex> lk(this->releaseQueueMutex);
    this->releaseQueue.push_back(in);
  }
}

void THIS::handleInhibitMsg(DBus::Message* msg, DBus::Message* retmsg) {
  if (msg->sender() == this->forkSender) return;

  const char* what; const char* who; const char* why; const char* mode;
  msg->getArgs(DBUS_TYPE_STRING, &what,
               DBUS_TYPE_STRING, &who,
               DBUS_TYPE_STRING, &why,
               DBUS_TYPE_STRING, &mode,
               DBUS_TYPE_INVALID);

  if (retmsg != nullptr && this->monitor) {
    int32_t fd = -1;
    retmsg->getArgs(DBUS_TYPE_UNIX_FD, &fd, DBUS_TYPE_INVALID);

    Inhibit in = { this->systemdType2us(what), who, why, this->mkId(fd), (uint64_t)time(NULL)};
    this->registerInhibit(in);

    char filePath[1024*10];
    std::string fdpath = "/proc/self/fd/";
    fdpath += std::to_string(fd);
    int rl = readlink(fdpath.c_str(), filePath, 1024*10);
    close(fd);

    if (rl >= 0) {
      std::thread t(&THIS::releaseThread, this, filePath, in, (std::string(mode) == "delay"));
      t.detach();
    }
  } else {
    auto lockRef = this->newLockRef();
    auto id = this->mkId(lockRef.wfd);

    try {
      this->pidUids[id] = { .pid = msg->senderPID(), .uid = msg->senderUID() };
    } catch (DBus::NameHasNoOwnerError& e) {}

    msg->newMethodReturn().appendArgs(DBUS_TYPE_UNIX_FD, &lockRef.wfd, DBUS_TYPE_INVALID)->send();

    close(lockRef.wfd); // Close our end so we can watch for EOF on release

    Inhibit in = { this->systemdType2us(what), who, why, id };
    this->registerInhibit(in);

    std::thread t(&THIS::releaseThreadOurFd, this, lockRef.rfd, lockRef.file, in);
    t.detach();
  }
}

THIS::lockRef THIS::newLockRef() {
    mkdir("/tmp/uinhibitd-systemd-inhibits/", 0777);
    std::string file =
      "/tmp/uinhibitd-systemd-inhibits/inhibit" + std::to_string(++this->lastLockRef) + ".ref";
    unlink(file.c_str());

    int32_t fifo_r = mkfifo(file.c_str(), 0600);
    int32_t rfd = open(file.c_str(), O_RDONLY | O_NONBLOCK);
    int32_t wfd = open(file.c_str(), O_WRONLY | O_NONBLOCK);
    if (rfd < 0 || wfd < 0 || fifo_r < 0) throw std::runtime_error("Failed to set up fifo.");

    return { rfd, wfd, file };
}

void THIS::poll() {
  std::unique_lock<std::mutex> lk(this->releaseQueueMutex);

  for (auto r : this->releaseQueue) {
    if (this->pidUids.contains(r.id)) this->pidUids.erase(r.id);
    this->registerUnInhibit(r.id);
  }

  this->releaseQueue.clear();
}

Inhibit THIS::doInhibit(InhibitRequest r) {
  if (((r.type & InhibitType::SUSPEND) == InhibitType::NONE)
      && ((r.type & InhibitType::SCREENSAVER) == InhibitType::NONE))
    throw uinhibit::InhibitRequestUnsupportedTypeException();

  int32_t fd = -1;
  if (this->monitor) {
    this->inhibitFork->tx(us2systemdType(r.type)+'\t'+r.appname+'\t'+r.reason+'\t'+"block"+'\n');
    fd = atol(this->inhibitFork->rx().c_str());
  } else {
    auto lockRef = this->newLockRef();
    close(lockRef.wfd);
    fd = lockRef.rfd;
  }

  Inhibit i = {r.type, r.appname, r.reason, {}, (uint64_t)time(NULL)};
  i.id = this->mkId(fd);
  return i;
}

void THIS::doUnInhibit(InhibitID id) {
  int32_t fd = reinterpret_cast<_InhibitID*>(&id[0])->fd;
  if (this->monitor) {
    this->inhibitFork->tx(std::to_string(fd)+'\n');
  } else {
    char filePath[1024*10];
    std::string fdpath = "/proc/self/fd/";
    fdpath += std::to_string(fd);
    if (readlink(fdpath.c_str(), filePath, 1024*10) > 0) {
      unlink(filePath);
    }
  }
}

InhibitID THIS::mkId(uint32_t fd) {
  _InhibitID idStruct = {this->instanceId, fd};
  auto ptr = reinterpret_cast<std::byte*>(&idStruct);
  InhibitID id(ptr, ptr+sizeof(idStruct));
  return id;
}

InhibitType THIS::systemdType2us(std::string what) {
  std::vector<std::string> types;
  types.emplace_back();

  for (auto c : what) {
    if (c == ':') types.emplace_back();
    else types.back().push_back(c);
  }

  InhibitType t = InhibitType::NONE;

  for (auto str : types) {
    if (str == "idle") t = static_cast<InhibitType>(t | InhibitType::SCREENSAVER);
    if (str == "sleep") t = static_cast<InhibitType>(t | InhibitType::SUSPEND);
    // TODO support others
  }

  return t;
}

std::string THIS::us2systemdType(InhibitType t) {
  std::set<std::string> whats;

  if ((t & InhibitType::SCREENSAVER) > 0) whats.insert("idle");
  if ((t & InhibitType::SUSPEND) > 0) {
    if(!whats.contains("idle")) whats.insert("idle");
    whats.insert("sleep");
  }

  std::string ret;
  int32_t i = 0;
  for (auto w : whats) {
    if (i != 0) ret += ":";
    ret += w;
    i++;
  }

  return ret;
}
