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

#pragma once
#include <functional>
#include <dbus/dbus.h>
#include <map>
#include <cstdint>
#include <string>
#include <memory>
#include <charconv>
#include <cstddef>
#include "DBus.hpp"
#include <atomic>
#include <mutex>
#include <coroutine>
#include <thread>
#include <set>

namespace uinhibit {
  class InhibitRequestUnsupportedTypeException : std::exception {};
  class InhibitNoResponseException : std::exception {};
  class InhibitNotFoundException : std::exception {};

  // Unique ID for an Inhibit. Data within is Inhibitor-specific.
  // Unique among all Inhibitors.
  typedef std::vector<std::byte> InhibitID; 

  enum InhibitType {
    NONE = 0,
    SCREENSAVER = 0b00000001,
    SUSPEND     = 0b00000010,
  };

  struct InhibitRequest {
    InhibitType type = InhibitType::NONE;
    std::string appname = "";
    std::string reason = "";
  };

  struct Inhibit : public InhibitRequest {
    InhibitID id;
    uint64_t created = 0;
  };

  class Inhibitor {
    public:
      Inhibitor(std::function<void(Inhibitor*,Inhibit)> inhibitCB,
                std::function<void(Inhibitor*,Inhibit)> unInhibitCB);

      struct ReturnObject {
        ReturnObject(std::coroutine_handle<> h) : handle{h} {}
        std::coroutine_handle<> handle;

        struct promise_type {
          ReturnObject get_return_object() {
            return {std::coroutine_handle<promise_type>::from_promise(*this)};
          }
          std::suspend_never initial_suspend() { return {}; }
          std::suspend_never final_suspend() noexcept { return {}; }
          void unhandled_exception() { }

          auto getHandle(){
            return std::coroutine_handle<promise_type>::from_promise(*this);
          }
        };
      };

      // Start operating/listening for inhibit events
      virtual ReturnObject start() = 0;

      // Bitflags of all currently inhibited types
      InhibitType inhibited();

      // Call do(Un)Inhibit for subclasses (TODO) and adds/removes activeInhibits
      // Also calls handle(un)InhibitEvent handleInhibitStateChanged()
      // Must be awaiting for these to function
      Inhibit inhibit(InhibitRequest);
      void unInhibit(InhibitID);

      uint64_t instanceId = 0; // Uniquely identifies this Inhibitor instance
      std::map<InhibitID, Inhibit> activeInhibits;
    protected:
      // Implementation of (un)inhibit action. Do not register the inhibit, as this was a
      // user-requested action and they don't need to be called back about it (this could result in
      // infinite loops).
      //
      // Throw InhibitRequestUnsupportedTypeException if you don't support this type.
      virtual Inhibit doInhibit(InhibitRequest) = 0;
      virtual void doUnInhibit(InhibitID) = 0;

      // Add/remove from activeInhibits and call the inhibit/uninhibit callbacks
      void registerInhibit(Inhibit& i);
      void registerUnInhibit(InhibitID&);

      // Called any time register(un)Inhibit or (un)inhibit is called 
      //
      // ie. This event may have been generated by your implementation, or the user of this 
      // Inhibitor asked for the inhibit
      //
      // Useful to implement Inhibitors that actively 'push' their state on these events,
      // such as the org.freedesktop.PowerManager.HasInhibitChanged signal
      virtual void handleInhibitEvent(Inhibit inhibit) = 0;
      virtual void handleUnInhibitEvent(Inhibit inhibit) = 0;
      virtual void handleInhibitStateChanged(InhibitType inhibited, Inhibit inhibit) = 0;

      std::function<void(Inhibitor*, Inhibit)> inhibitCB;
      std::function<void(Inhibitor*, Inhibit)> unInhibitCB;
    private:
      void callEvent(bool isInhibit, Inhibit i);
      InhibitType lastInhibitState = InhibitType::NONE;
  };

  class LinuxKernelInhibitor : public Inhibitor {
    public:
      LinuxKernelInhibitor(std::function<void(Inhibitor*,Inhibit)> inhibitCB,
                           std::function<void(Inhibitor*,Inhibit)> unInhibitCB,
                           int32_t forkFD, int32_t forkOutFD);

      ReturnObject start();
      static void lockFork(int32_t inFD, int32_t outFD);
    protected:
      struct _InhibitID {
        uint64_t instanceID;
        char lockName[1024];
      };

      Inhibit doInhibit(InhibitRequest) override;
      void doUnInhibit(InhibitID) override;
      void handleInhibitEvent(Inhibit inhibit) override;
      void handleUnInhibitEvent(Inhibit inhibit) override;
      void handleInhibitStateChanged(InhibitType inhibited, Inhibit inhibit) override;

    private:
      bool ok = true;
      void watcherThread();
      InhibitID mkId(const char* lockName);

      std::mutex registerMutex;
      std::vector<Inhibit> registerQueue;
      std::vector<InhibitID> unregisterQueue;
      int32_t forkFD = -1;

      std::set<InhibitID> ourInhibits;
  };

  class XautolockInhibitor: public Inhibitor {
    public:
      XautolockInhibitor(std::function<void(Inhibitor*,Inhibit)> inhibitCB,
                         std::function<void(Inhibitor*,Inhibit)> unInhibitCB);

    protected:
      ReturnObject start();
      Inhibit doInhibit(InhibitRequest) override;
      void doUnInhibit(InhibitID) override {};
      void handleInhibitEvent(Inhibit inhibit) override {};
      void handleUnInhibitEvent(Inhibit inhibit) override {};
      void handleInhibitStateChanged(InhibitType inhibited, Inhibit inhibit) override;
    private:
      InhibitType lastInhibited = InhibitType::NONE;
  };

  class DBusInhibitor : public Inhibitor {
    public:
      struct DBusMethodCB {
        std::string interface;
        std::string member;
        void (DBusInhibitor::*callback)(DBus::Message* msg, DBus::Message* retmsg);
        std::string destination;
      };

      struct DBusSignalCB {
        std::string interface;
        std::string member;
        void (DBusInhibitor::*callback)(DBus::Message* msg);
      };

      DBusInhibitor(std::function<void(Inhibitor*, Inhibit)> inhibitCB,
                    std::function<void(Inhibitor*, Inhibit)> unInhibitCB,
                    std::string interface,
                    DBusBusType busType,
                    std::vector<DBusMethodCB> myMethods,
                    std::vector<DBusSignalCB> mySignals);

      ReturnObject start() override;

      bool monitor;
    protected:
      DBus dbus;
      std::unique_ptr<DBus> callDbus; // We're not permitted to send messages when in monitoring 
                                      // mode. As such, in monitoring mode this connection will be
                                      // defined for that purpose.
                                      //
                                      // Undefined when not in monitoring mode.

      std::vector<DBusMethodCB> myMethods;
      std::vector<DBusSignalCB> mySignals;

      std::map<uint32_t, DBus::Message> methodCalls; // serial, message
      std::string interface;

      virtual void poll() = 0;
  };

  // Multiple inhibitors share this common base interface:
  // Inhibit(appname, reason) -> cookie
  // UnInhibit(cookie)
  //
  // Always producing strictly one type of inhibit.
  //
  // This implements that common interface.
  class SimpleDBusInhibitor : public DBusInhibitor {
    public:
      SimpleDBusInhibitor(std::function<void(Inhibitor*, Inhibit)> inhibitCB,
                          std::function<void(Inhibitor*, Inhibit)> unInhibitCB,
                          std::vector<DBusMethodCB> myMethods,
                          std::vector<DBusSignalCB> mySignals,
                          std::string interface,
                          std::string path,
                          InhibitType inhibitType,
                          std::string extraIntrospect);
    protected:
      struct _InhibitID {
        uint64_t instanceID;
        char sender[1024];
        uint32_t cookie;
      };  

      void handleInhibitMsg(DBus::Message* msg, DBus::Message* retmsg);
      void handleUnInhibitMsg(DBus::Message* msg, DBus::Message* retmsg);
      void handleNameLostMsg(DBus::Message* msg);
      void handleIntrospect(DBus::Message* msg, DBus::Message* retmsg);
      InhibitID mkId(std::string sender, uint32_t cookie);
      std::map<std::string, std::vector<InhibitID>> inhibitOwners; // sender, {ids}
      uint32_t lastCookie = 0;

      void handleInhibitEvent(Inhibit inhibit) override {};
      void handleUnInhibitEvent(Inhibit inhibit) override {};
      void handleInhibitStateChanged(InhibitType inhibited, Inhibit inhibit) override {};

      Inhibit doInhibit(InhibitRequest r) override;
      void doUnInhibit(InhibitID id) override;

      void poll() override {};

      std::string interface;
      std::string path; // Will have any leading / removed
      InhibitType inhibitType;
      std::string extraIntrospect;
  };

  class FreedesktopScreenSaverInhibitor : public SimpleDBusInhibitor {  
    public:
      FreedesktopScreenSaverInhibitor(std::function<void(Inhibitor*, Inhibit)> inhibitCB,
                                      std::function<void(Inhibitor*, Inhibit)> unInhibitCB) :
        SimpleDBusInhibitor(inhibitCB, unInhibitCB, {}, {},
                            "org.freedesktop.ScreenSaver",
                            "/ScreenSaver",
                            InhibitType::SCREENSAVER,
                            "") {};
  };

  class GnomeScreenSaverInhibitor : public SimpleDBusInhibitor {  
    public:
      GnomeScreenSaverInhibitor(std::function<void(Inhibitor*, Inhibit)> inhibitCB,
                                std::function<void(Inhibitor*, Inhibit)> unInhibitCB);
    protected:
      void handleSimActivityMsg(DBus::Message* msg, DBus::Message* retmsg);
      void poll() override;
  };

  class FreedesktopPowerManagerInhibitor : public SimpleDBusInhibitor {
    public:
      FreedesktopPowerManagerInhibitor(std::function<void(Inhibitor*, Inhibit)> inhibitCB,
                                      std::function<void(Inhibitor*, Inhibit)> unInhibitCB);
    protected:
      void handleHasInhibitMsg(DBus::Message* msg, DBus::Message* retmsg);
      void handleInhibitStateChanged(InhibitType inhibited, Inhibit inhibit);
  };

  class GnomeSessionManagerInhibitor : public DBusInhibitor {
    public:
      GnomeSessionManagerInhibitor(std::function<void(Inhibitor*, Inhibit)> inhibitCB,
                                   std::function<void(Inhibitor*, Inhibit)> unInhibitCB);
    protected:
      struct _InhibitID {
        uint64_t instanceID;
        char sender[1024];
        uint32_t cookie;
      };

      enum GnomeInhibitType {
        NONE = 0,
        LOGOUT = 1,
        USERSWITCH = 2,
        SUSPEND = 4,
        SESSIONIDLE = 8,
        AUTOMOUNT = 16
      };

      void handleInhibitMsg(DBus::Message* msg, DBus::Message* retmsg);
      void handleUnInhibitMsg(DBus::Message* msg, DBus::Message* retmsg);
      void handleIsInhibitedMsg(DBus::Message* msg, DBus::Message* retmsg);
      void handleGetInhibitors(DBus::Message* msg, DBus::Message* retmsg);
      void handleNameLostMsg(DBus::Message* msg);
      void handleGetProperty(DBus::Message* msg, DBus::Message* retmsg);
      void handleIntrospect(DBus::Message* msg, DBus::Message* retmsg);

      // For each inhibitor (org/gnome/SessionManager/Inhibitorxyzw)
      void handleGetAppID(DBus::Message* msg, DBus::Message* retmsg);
      void handleGetReason(DBus::Message* msg, DBus::Message* retmsg);
      void handleGetFlags(DBus::Message* msg, DBus::Message* retmsg);

      std::map<std::string, std::vector<InhibitID>> inhibitOwners; // sender, {ids}
      uint32_t lastCookie = 0;

      void handleInhibitEvent(Inhibit inhibit) override;
      void handleUnInhibitEvent(Inhibit inhibit) override;
      void handleInhibitStateChanged(InhibitType inhibited, Inhibit inhibit) override {};
      Inhibit doInhibit(InhibitRequest r) override;
      void doUnInhibit(InhibitID id) override;

      void poll() override {};

    private:
      InhibitType gnomeType2us(GnomeInhibitType t);
      GnomeInhibitType us2gnomeType(InhibitType us);
      InhibitID mkId(std::string sender, uint32_t cookie);
      uint32_t inhibitorPathToCookie(std::string path);
      Inhibit* inhibitFromCookie(uint32_t cookie); // throws InhibitNotFoundException
  };

  class SystemdInhibitor : public DBusInhibitor {
    public:
      SystemdInhibitor(std::function<void(Inhibitor*, Inhibit)> inhibitCB,
                       std::function<void(Inhibitor*, Inhibit)> unInhibitCB);
    protected:
      struct _InhibitID {
        uint64_t instanceID;
        uint32_t fd;
      };

      void handleInhibitMsg(DBus::Message* msg, DBus::Message* retmsg);
      void handleIntrospect(DBus::Message* msg, DBus::Message* retmsg);
      void handleListInhibitorsMsg(DBus::Message* msg, DBus::Message* retmsg);

      void handleInhibitEvent(Inhibit inhibit) override {};
      void handleUnInhibitEvent(Inhibit inhibit) override {};
      void handleInhibitStateChanged(InhibitType inhibited, Inhibit inhibit) override {};
      Inhibit doInhibit(InhibitRequest r) override;
      void doUnInhibit(InhibitID id) override;
      void poll() override;

    private:
      void releaseThread(const char* path, Inhibit in);
      void releaseThreadOurFd(int32_t fd, std::string path, Inhibit in);
      InhibitID mkId(uint32_t fd);
      InhibitType systemdType2us(std::string what); 
      std::string us2systemdType(InhibitType t);

      struct lockRef {
        int32_t rfd = -1;
        int32_t wfd = -1;
        std::string file;
      };

      lockRef newLockRef();

      uint64_t lastLockRef = 0;

      std::mutex releaseQueueMutex; 
      std::vector<Inhibit> releaseQueue;

      struct PidUid {
        uint32_t pid;
        uint32_t uid;
      };

      std::map<InhibitID, PidUid> pidUids;
  };

  class CinnamonScreenSaverInhibitor : public DBusInhibitor {
    public:
      CinnamonScreenSaverInhibitor(std::function<void(Inhibitor*, Inhibit)> inhibitCB,
                                   std::function<void(Inhibitor*, Inhibit)> unInhibitCB);
    protected:
      struct _InhibitID {
        uint64_t instanceID;
        char sender[1024];
      };

      void handleSimActivityMsg(DBus::Message* msg, DBus::Message* retmsg);
      void handleIntrospect(DBus::Message* msg, DBus::Message* retmsg);
      InhibitID mkId(std::string sender);
      std::map<std::string, std::vector<InhibitID>> inhibitOwners; // sender, {ids}
      uint32_t lastUsInhibit = 0;
      std::map<std::string, std::jthread> simThreads; // Our made-up sender, thread

      void handleInhibitEvent(Inhibit inhibit) override {};
      void handleUnInhibitEvent(Inhibit inhibit) override {};
      void handleInhibitStateChanged(InhibitType inhibited, Inhibit inhibit) override {};

      // TODO: need to spawn a thread on doInhibit to constantly keep us alive with
      // SimulateUserActivity requests
      Inhibit doInhibit(InhibitRequest r) override;
      void doUnInhibit(InhibitID id) override;

      void poll() override;

    private:
      //void simThread(std::stop_token stop_token);
  };

  // wayland inhibit
  // xscreensaver inhibit?
  // XFCE inhibit
  // org.freedesktop.portal.Inhibit xdg-desktop-portal - integration for sandboxed apps?
  // sxmo?
}
