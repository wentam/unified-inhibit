#include "util.hpp"
#include <cstdlib>
#include <cstdio>
#include <cstdint>
#include "Inhibitor.hpp"
#include <fcntl.h>
#include <signal.h>
#include "DBus.hpp"
#include <condition_variable>
#include <chrono>
using namespace std::chrono_literals;

using namespace uinhibit;

uint64_t pass = 0, fail = 0;

int stdoutFD = -1;
bool quiet = false;

// Disables stdout while in scope
class Quiet {
  public:
    Quiet() {
      quiet = true;
      fflush(stdout);
      stdoutFD = dup(STDOUT_FILENO);
      int devnull = open("/dev/null", O_RDWR);
      dup2(devnull, STDOUT_FILENO);
      close(devnull);
    }

    ~Quiet() {
      quiet = false;
      fflush(stdout);
      dup2(stdoutFD, STDOUT_FILENO);
    }
};

// Overrides a 'Quiet' session while in scope
class UnQuiet {
  public:
    UnQuiet() {
      if (quiet){
        fflush(stdout);
        dup2(stdoutFD, STDOUT_FILENO);
      }
    }

    ~UnQuiet() {
      if (quiet){
        fflush(stdout);
        stdoutFD = dup(STDOUT_FILENO);
        int devnull = open("/dev/null", O_RDWR);
        dup2(devnull, STDOUT_FILENO);
        close(devnull);
      }
    }
};

// Runs the inhibitor in a thread until out of scope
class InhibitorSession {
  public:
    InhibitorSession(Inhibitor* i) : tr(&InhibitorSession::runInhibitorThread, this, i) {}
    ~InhibitorSession() { std::unique_lock<std::mutex> lk(stopMutex); stop = true; }
    std::jthread tr;

    // Blocking. Will be slow as we're waiting on mainloop polling.
    void runInThread(std::function<void()> stuff) {
      runme = &stuff;
      {
        std::unique_lock<std::mutex> lk(runmeMutex);
        runmeCV.wait_for(lk, 100ms, [this]{ return runme == nullptr; });
      }
    }

    std::mutex runmeMutex;
    std::condition_variable runmeCV;
    std::function<void()>* runme = nullptr;

    std::mutex stopMutex;
    bool stop = false;

    void runInhibitorThread(Inhibitor* i) {
      auto ro = i->start();
      while(1) {
        {
          std::unique_lock<std::mutex> lk(stopMutex);
          if (stop) break;
        }

        ro.handle.resume();
        if (runme != nullptr) {
          (*runme)();
          runme = nullptr;
          runmeCV.notify_all();
        }
        fflush(stdout);
      }
    }
};

bool assert(bool condition, std::string msg) {
  UnQuiet uq;
  if (condition) {
    printf(ANSI_COLOR_BOLD_GREEN "[ TRUE ]" ANSI_COLOR_RESET " %s\n", msg.c_str()); pass++;
  } else {
    printf(ANSI_COLOR_BOLD_RED "[ FALSE ]" ANSI_COLOR_RESET " %s\n", msg.c_str()); fail++;
  }
  return condition;
}

// Assertion with a dependency: code will only run if the dependency is true.
// Always false if dependency is false
bool assert(bool dependency, std::function<bool()> run, std::string msg) {
  if (dependency) return assert(run(), msg);
  else return assert(false, "(Because assertion dependency false) "+msg);
}

std::vector<int> dbusPIDs;
void startDbusDaemon() {
  auto pid = fork();

  if (pid == 0) {
    setpgid(getpid(), getpid());
    if (system("dbus-daemon --config-file=test/dbus.conf --address='unix:path=/tmp/uitest.sock'") == -1) {
      printf("Failed to spool up local dbus-daemon (do you have dbus-daemon?)\n");
      exit(1);
    };
    _exit(0);
  }
  dbusPIDs.push_back(pid);
}

void exitHandler() {
  printf(ANSI_COLOR_BOLD_YELLOW "\n%ld/%ld assertions true\n" ANSI_COLOR_RESET, pass, pass+fail);
  printf(ANSI_COLOR_BOLD_YELLOW "%ld/%ld assertions false\n" ANSI_COLOR_RESET, fail, pass+fail);
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

  #include "tests.hpp"

  if (fail == 0) return 0; else return 1;
}
