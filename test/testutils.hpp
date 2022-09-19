#pragma once
#include <chrono>
using namespace std::chrono_literals;
using namespace uinhibit;

static uint64_t pass = 0, fail = 0;

static int stdoutFD = -1;
static bool quiet = false;

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

        {
          std::unique_lock<std::mutex> lk(runmeMutex);
          if (runme != nullptr) {
            (*runme)();
            runme = nullptr;
            runmeCV.notify_all();
          }
        }
        fflush(stdout);
      }
    }
};

static bool assert(bool condition, std::string msg) {
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
static bool assert(bool dependency, std::function<bool()> run, std::string msg) {
  if (dependency) return assert(run(), msg);
  else return assert(false, "(Because assertion dependency false) "+msg);
}

static bool assert(bool dependency, bool condition, std::string msg) {
  if (dependency) return assert(condition, msg);
  else return assert(false, "(Because assertion dependency false) "+msg);
}

static void printResults() {
  printf(ANSI_COLOR_BOLD_YELLOW "\n%ld/%ld assertions true\n" ANSI_COLOR_RESET, pass, pass+fail);
  printf(ANSI_COLOR_BOLD_YELLOW "%ld/%ld assertions false\n" ANSI_COLOR_RESET, fail, pass+fail);
}
