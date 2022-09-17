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
#include "DBus.hpp"
#include <cxxabi.h>

// TODO: if we started in monitoring mode but the interface becomes no longer implemented,
// we should probably take it over (implement at DBusInhibitor level)
//
// TODO: if we are implementing, allow someone to take over the name and implement in our place.
// if this happens, we need to fall back to monitoring

namespace uinhibit {
  DBusInhibitor::DBusInhibitor(
    std::function<void(Inhibitor*, Inhibit)> inhibitCB,
    std::function<void(Inhibitor*, Inhibit)> unInhibitCB,
    std::string interface,
    DBusBusType busType,
    std::vector<DBusMethodCB> myMethods,
    std::vector<DBusSignalCB> mySignals
  ) 
  : 
    Inhibitor(inhibitCB, unInhibitCB),
    dbus(busType),
    myMethods(myMethods),
    mySignals(mySignals),
    interface(interface)
  { 
    this->monitor = dbus.nameHasOwner(interface.c_str());

    if (this->monitor) {
      this->callDbus = std::unique_ptr<DBus>(new DBus(busType));

      try {
        std::vector<std::string> rules;

        for (auto method : myMethods) {
          std::string dest = "";
          if (method.destination != "*") dest = ",destination="+method.destination;
          rules.push_back("type='method_call',interface='"+method.interface+"',member='"+method.member+"'"+dest);
        }

        for (auto signal : mySignals)
          rules.push_back("type='signal',interface='"+signal.interface+"',member='"+signal.member+"'");

        // TODO: it may be possible to monitor only the method returns going to the sender that
        // happens to be implementing this interface (destination=org.freedesktop.ScreenSaver)
        rules.push_back("type='method_return'");

        std::vector<const char*> Crules;
        Crules.reserve(rules.size());

        for (auto& r : rules) {
          Crules.push_back(r.c_str());
        }

        // TODO: becomeMonitor should provide an overloaded version taking a vector of std::string
        dbus.becomeMonitor(Crules);

        printf("[" ANSI_COLOR_YELLOW "-" ANSI_COLOR_RESET "] %s: Someone "
               "else has this dbus interface implemented. Became a monitor and will eavesdrop.\n",
               interface.c_str());
      } catch (DBus::UnknownInterfaceError& e) {
        printf("[" ANSI_COLOR_RED "x" ANSI_COLOR_RESET "] %s: "
               "UNSUPPORTED: Someone else has this dbus interface implemented. Tried to become a monitor in"
               " order to eavesdrop but your dbus daemon doesn't appear to support that (it's "
               "probably too old).\n", interface.c_str());
      } catch (DBus::AccessDeniedError& e) {
        printf("[" ANSI_COLOR_RED "x" ANSI_COLOR_RESET "] %s: "
               "ACCESS DENIED: Someone else has this dbus interface implemented. Tried to become a"
               " monitor to eavesdrop but was denied access. You might need to give me setuid "
               " (chown root uinhibitd && chmod 4755 uinhibitd). You might need to allow monitoring"
               " in D-Bus config.\n", interface.c_str());
      }
    } else {
      int ret = 0;

      try {
        ret = dbus.requestName(interface.c_str(), DBUS_NAME_FLAG_REPLACE_EXISTING);
      } catch (const DBus::AccessDeniedError& e) {
        printf("[" ANSI_COLOR_RED "x" ANSI_COLOR_RESET "] %s: ACCESS DENIED: We tried to implement"
               " this interface but dbus denied our name request. You might need to configure D-Bus"
               " to allow us access (see README.md)\n", interface.c_str());
        return;
      }

      if (ret != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) {
        printf("[" ANSI_COLOR_RED "x" ANSI_COLOR_RESET "] %s: Need to "
               "implement this interface, but failed to obtain the interface name.\n",
               interface.c_str());
      } else {

        std::vector<std::string> signalStrings;

        for (auto signal : mySignals) 
          signalStrings.push_back("type='signal',interface='"+signal.interface+"',member='"+signal.member+"'");

        for (auto& str : signalStrings) dbus.addMatch(str.c_str());

        printf("[" ANSI_COLOR_GREEN "✓" ANSI_COLOR_RESET "] %s: "
               "Implementing interface\n", interface.c_str());
      }
    }
  }

  static const char* currentExceptionTypeName() {
    int status;
    return abi::__cxa_demangle(abi::__cxa_current_exception_type()->name(), 0, 0, &status);
  }

  Inhibitor::ReturnObject DBusInhibitor::start() {
    const char* mName = dbus.getUniqueName();
    const char* mName2 = "";
    if (callDbus != NULL) {
     mName2 = callDbus->getUniqueName();
    }

    while(1) try {
      dbus.readWriteDispatch(40);
      while (1) try {
        auto msg = dbus.popMessage();
        if (msg.isNull()) break;
        if (strcmp(msg.sender(),mName) == 0) continue;
        if (strcmp(msg.sender(),mName2) == 0) continue;

        if (msg.type() == DBUS_MESSAGE_TYPE_METHOD_CALL) {
          for (auto& method : myMethods) {
            if ((method.member == std::string(msg.member())) && 
                (method.interface == std::string(msg.interface()))) {
              if (this->monitor) this->methodCalls.insert({msg.serial(), msg});
              else (this->*method.callback)(&msg, nullptr);

              break;
            }
          }
        }

        if (msg.type() == DBUS_MESSAGE_TYPE_METHOD_RETURN &&
            this->methodCalls.contains(msg.replySerial())) {
          auto callMsg = this->methodCalls.at(msg.replySerial());
          // TODO myMethods should probably be a map
          for (auto method : myMethods) {
            if (method.member == callMsg.member()) {
              (this->*method.callback)(&callMsg, &msg);
              this->methodCalls.erase(msg.replySerial());
              break;
            }
          }
        }

        if (msg.type() == DBUS_MESSAGE_TYPE_SIGNAL) {
          // TODO mySignals should probably be a map
          for (auto& signal : mySignals) {
            if ((signal.member == std::string(msg.member())) &&
                (signal.interface == std::string(msg.interface()))) {
              (this->*signal.callback)(&msg);
              break;
            }
          }
        }
      } catch (DBus::InvalidArgsError& e) {
        printf("Got invalid args for a method call, ignoring.\n");
        // TODO should we respond to the bad request in some way in this situation? Some apps
        // could hang waiting for a response.
      }
      this->poll();
      co_await std::suspend_always();
    }
    catch (DBus::DisconnectedError& e) { 
      printf(ANSI_COLOR_RED "DBus disconnection for interface %s. Trying to reconnect..." 
             ANSI_COLOR_RESET, this->interface.c_str());

      // TODO: we need to re-set ourselves up (as in what the constructor does)
      // note that this won't always work because some constructors need root,
      // and by this point we are not and can't go back.

      bool fail = false;
      try {
        dbus.reconnect();     
      } catch (...) { fail = true; }

      if (!fail) printf(ANSI_COLOR_GREEN "reconnected\n" ANSI_COLOR_RESET);
      else printf(ANSI_COLOR_RED "failed. Stopping inhibitor\n" ANSI_COLOR_RESET);

      if (fail) {
        // TODO this situation seems to lead to segfault, possibly attempting to resume a returned
        // coroutine
        break;
      }
    }
    catch (std::exception &e) { 
      printf("Unhandled exception %s: %s\n", currentExceptionTypeName(), e.what());
    }
    catch (...) { std::terminate(); }
  }
}; // End namespace uinhibit
