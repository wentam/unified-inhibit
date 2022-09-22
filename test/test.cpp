#include "util.hpp"
#include <cstdlib>
#include <cstdio>
#include <cstdint>
#include "InhibitInterface.hpp"
#include <fcntl.h>
#include <signal.h>
#include "DBus.hpp"
#include <condition_variable>
#include <chrono>
#include "assertions.hpp"
#include "testutils.hpp"
using namespace uinhibit;

std::vector<int> dbusPIDs;
// kills daemon after 30 seconds to handle corner-cases that can leave dbus-daemon running
void startDbusDaemon() {
  auto pid = fork();

  if (pid == 0) {
    setpgid(getpid(), getpid());
    if (system("dbus-daemon --config-file=test/dbus.conf --address='unix:path=/tmp/uitest.sock'"
               " 1>/dev/null 2>/dev/null &"
               " export DDPID=$! && sleep 30 && kill $DDPID") == -1) {
      printf("Failed to spool up local dbus-daemon (do you have dbus-daemon?)\n");
      exit(1);
    };
    _exit(0);
  }
  dbusPIDs.push_back(pid);
}

void exitHandler() {
  printResults();
  for(auto pid : dbusPIDs) kill(-pid, SIGKILL);
}

int main() {
  atexit(exitHandler);
  std::set_terminate(exitHandler);

  // Set up our own dbus daemon with our own socket. We can use this to run tests
  // in isolation from the system.
  if (setenv("DBUS_SESSION_BUS_ADDRESS", "unix:path=/tmp/uitest.sock", 1) == -1) {
    printf("Failed to setenv()\n");
    exit(1);
  };

  startDbusDaemon();

  usleep(200*1000); // give dbus-daemon a chance to start

  DBus dbus(DBUS_BUS_SESSION);

  assertions(dbus);

  if (fail == 0) return 0; else return 1;
}
