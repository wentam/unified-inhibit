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

#include <cstdio>
#include "InhibitInterface.hpp"
#include <thread>
#include <mutex>
#include "util.hpp"
#include "Fork.hpp"
#include <signal.h>

extern char **environ;

// TODO
// * command line argument to disable specific inhibitors
// * command line argument to force specific D-Bus inhibitors to monitoring mode (to ensure some
//   other application can implement/restart freely)
// * command line argument to disable takeover for specific (or all) inhibitors (monitoring
//   inhibitors won't try to implement if existing implementer disappears)
// * if another application tries to take over our D-Bus interface implementation,
//   step back to monitoring mode?
// * when logging inhibit state change and there are active inhibits, list the inhibitors
//   responsible
// * log levels
// * optional ability to write the current inhibit state to a file
// * optional ability to forward screensaver locks to suspend and vice-versa
// * ability to ignore inhibits from certain appnames (--ignore steam)

using namespace uinhibit;

static std::vector<InhibitInterface*> inhibitors;
static InhibitType lastInhibitType = InhibitType::NONE;
static std::map<InhibitID, std::vector<std::pair<InhibitInterface*, InhibitID>>> releasePlan;

static InhibitType inhibited() {
  InhibitType i = InhibitType::NONE;
  for (auto& in : inhibitors) {
    for (auto& [in, inhibit] : in->activeInhibits) i = static_cast<InhibitType>(i | inhibit.type);
  }
  return i;
}

static void printInhibited() {
  auto i = inhibited();
  if (lastInhibitType != i) {
    printf("Inhibit state changed to: screensaver=%d suspend=%d\n",
           ((i & InhibitType::SCREENSAVER) > 0),
           ((i & InhibitType::SUSPEND) > 0));
    lastInhibitType = i;
  }
}

static void inhibitCB(InhibitInterface* inhibitor, Inhibit inhibit) {
  printf("Inhibit event type=%d appname='%s' reason='%s' from='%s'\n",
         inhibit.type,
         inhibit.appname.c_str(),
         inhibit.reason.c_str(),
         inhibitor->name.c_str());
  // Forward to all active inhibitors (other than the originator)
  try {
    for (auto& ai : inhibitors) {
      InhibitRequest r = {inhibit.type, inhibit.appname, inhibit.reason};
      if (ai->instanceId != inhibitor->instanceId) try {
        auto newInhibit = ai->inhibit(r);
        releasePlan[inhibit.id].push_back({ai, newInhibit.id});
      } catch (uinhibit::InhibitRequestUnsupportedTypeException& e) {}
    }
  }
  catch (uinhibit::InhibitNoResponseException& e) {
    printf(ANSI_COLOR_YELLOW "Warning: no response to a dbus method call\n" ANSI_COLOR_RESET);
  }

  // Output our global inhibit state to STDOUT if it's changed
  printInhibited();
}

static void unInhibitCB(InhibitInterface* inhibitor, Inhibit inhibit) {
  printf("UnInhibit event type=%d appname='%s' from='%s'\n",
         inhibit.type,
         inhibit.appname.c_str(),
         inhibitor->name.c_str());
  // Forward to all active inhibitors (other than the originator)
  try {
    if (releasePlan.contains(inhibit.id)) {
      for (auto& release : releasePlan.at(inhibit.id)) {
        try {
        release.first->unInhibit(release.second);
        } catch (uinhibit::InhibitRequestUnsupportedTypeException& e) {}
      }
      releasePlan.erase(inhibit.id);
    }
  }
  catch (uinhibit::InhibitNoResponseException& e) {
    printf(ANSI_COLOR_YELLOW "Warning: no response to a dbus method call\n" ANSI_COLOR_RESET);
  }
  // Output our global inhibit state to STDOUT if it's changed
  printInhibited();
}

static std::string cleanDisplayEnv(std::string display) {
  std::string cleanDisplay;
  uint32_t i = 0;
  for (auto c : display) {
    if (isdigit(c) || (c == ':' && i == 0)) cleanDisplay.push_back(c);
    i++;
  }

  return cleanDisplay;
}

static void handleExit() {
  for (auto& [id, plan] : releasePlan) {
    for (auto& r : plan) {
      try {
        r.first->unInhibit(r.second);
      } catch (uinhibit::InhibitRequestUnsupportedTypeException& e) {}
    }
  }
  releasePlan.clear();
  usleep(100*1000); // Give forks/threads a chance to release stuff
}

static void handleSig(int param) {
  exit(0);
}

static Args parseArgs(int argc, char* argv[]) {
  Args ret = {};
  std::string lastArg;
  for (int i = 1; i < argc; i++) {
    std::string arg = argv[i];
    uint8_t leadingDashes = 0;

    // Count up leading dashes for this arg
    for (auto c : arg) { if (c == '-') leadingDashes++; else break; }

    // Parse flags (bundling supported -abcd)
    if (leadingDashes == 1) {
      for (auto c : arg) {
        if (c == '-') continue;
        ret.flags.insert(c);
      }
    }

    // Parse params
    if (leadingDashes >= 2) {
      while(arg.front() == '-') arg.erase(0,1); // Remove leading dashes

      if (ret.params.contains(arg)) {
        printf(ANSI_COLOR_RED "Error: argument --%s specified twice\n" ANSI_COLOR_RESET ,
               arg.c_str());
        exit(1);
      }
      ret.params.insert({arg, {}});
      lastArg = arg;
    }

    // Param values
    if (leadingDashes == 0 && ret.params.contains(lastArg)) ret.params.at(lastArg).push_back(arg);
  }

  return ret;
}

int main([[maybe_unused]]int argc, [[maybe_unused]]char *argv[]) {
  atexit(handleExit);
  std::set_terminate(handleExit);
  signal(SIGINT, handleSig);

  puts("===============================================================================");
  printf("unified-inhibit v%s\n\n", version());
  puts(
       "Built with volunteered time. You can support my work at"
       " https://liberapay.com/wentam"
       );
  puts("===============================================================================");

  // Parse args/user input
  auto args = parseArgs(argc, argv);

  if (args.params.contains("version")) exit(0);
  if (args.params.contains("help") || args.flags.contains('h')) {
    int r = system("man uinhibitd");
    if (r != 0) r = system("man -l doc/uinhibitd.1.roff");
    if (r != 0) r = system("man -l ../doc/uinhibitd.1.roff");
    exit(0);
  }

  puts("\n[<-] : Listening for events from interface");
  puts("[->] : Sending events to interface");
  puts("[<->]: Bidirectional");
  puts("[x]  : Doing nothing\n");


  // Clone environment to restore after setuid stuff
  std::vector<std::string> startEnv;
  for(char **current = environ; *current; current++) startEnv.push_back(*current);

  // Security: We might be setuid. Clean up environment.
  const char* xdgRuntimeDir = getenv("XDG_RUNTIME_DIR");
  const char* sessionBusEnv = getenv("DBUS_SESSION_BUS_ADDRESS");
  const char* display = getenv("DISPLAY");
  std::string cleanDisplay = "";
  if (display != NULL) cleanDisplay = cleanDisplayEnv(display);

  if (clearenv() != 0) { printf("Failed to clear environment\n"); exit(1); }

  setenv("DISPLAY", cleanDisplay.c_str(), 0);
  if (sessionBusEnv != nullptr) setenv("DBUS_SESSION_BUS_ADDRESS", sessionBusEnv, 0);
  if (xdgRuntimeDir != nullptr) setenv("XDG_RUNTIME_DIR", xdgRuntimeDir, 0);

  // Some InhibitInterface instances need setuid forks, create them now before any threads exist
  SystemdInhibitFork systemdInhibitFork; systemdInhibitFork.run();
  LinuxKernelInhibitFork linuxInhibitFork; linuxInhibitFork.run();

  // D-Bus tries to prevent usage of setuid binaries by checking if euid != ruid.
  // We need setuid, but we can just set both euid *and* ruid and D-Bus is happy.
  auto ruid = getuid();

  // D-Bus inhibitors that need the system bus should be constructed as root
  // ---------------------------- SETUID ROOT --------------------------
  if (setresuid(0,0,0) != 0) {
    // All good, we just might get access denied depending on user config.
  }

  // Security note: we're root, always ensure these constructors are safe and don't touch raw
  // user input in any way. Our user input may be unprivileged.
  uinhibit::SystemdInhibitInterface i4(inhibitCB, unInhibitCB, &systemdInhibitFork); inhibitors.push_back(&i4);

  // D-Bus inhibitors that need the session bus should be constructed as the user
  if (setresuid(ruid,ruid,ruid) != 0) { printf("Failed to drop privileges\n"); exit(1); }
  if (getuid() != ruid) exit(1); // Should never happen
  // ---------------------------- SETUID SAFE --------------------------

  // Restore environment such that any user-commands have everything they need
  std::vector<char*> envMem;
  for (auto str : startEnv) {
    envMem.emplace_back();
    envMem.back() = (char*)malloc(str.size()+1);
    strcpy(envMem.back(), str.c_str());
    putenv(envMem.back());
  }

  FreedesktopScreenSaverInhibitInterface i1(inhibitCB, unInhibitCB); inhibitors.push_back(&i1);
  FreedesktopPowerManagerInhibitInterface i2(inhibitCB, unInhibitCB); inhibitors.push_back(&i2);
  GnomeSessionManagerInhibitInterface i3(inhibitCB, unInhibitCB); inhibitors.push_back(&i3);
  GnomeScreenSaverInhibitInterface i5(inhibitCB, unInhibitCB); inhibitors.push_back(&i5);
  CinnamonScreenSaverInhibitInterface i6(inhibitCB, unInhibitCB); inhibitors.push_back(&i6);
  MateScreenSaverInhibitInterface i11(inhibitCB, unInhibitCB); inhibitors.push_back(&i11);
  LinuxKernelInhibitInterface i7(inhibitCB, unInhibitCB, &linuxInhibitFork);
  inhibitors.push_back(&i7);
#ifdef BUILDFLAG_X11
  X11DPMSScreensaverInhibitInterface i12(inhibitCB, unInhibitCB); inhibitors.push_back(&i12);
#endif
  XautolockInhibitInterface i8(inhibitCB, unInhibitCB); inhibitors.push_back(&i8);
  XidlehookInhibitInterface i9(inhibitCB, unInhibitCB); inhibitors.push_back(&i9);
  SxmoInhibitInterface i13(inhibitCB, unInhibitCB); inhibitors.push_back(&i13);
  UserCommandsInhibitInterface i10(inhibitCB, unInhibitCB, args); inhibitors.push_back(&i10);

  // Run inhibitors
  // Security note: it is critical we have dropped privileges before this point, as we will be
  // running user-inputted commands.
  std::vector<InhibitInterface::ReturnObject> ros;
  for (auto& inhibitor : inhibitors) ros.push_back(inhibitor->start());


  puts("\n------------- Started successfully --------------");

  while(1) for (auto& r : ros) {r.handle.resume(); fflush(stdout); }

  for (auto m : envMem) free(m);
}
