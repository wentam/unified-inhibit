#include "Fork.hpp"
#include <stdexcept>
#include <signal.h>
#include <thread>
#include <stdio.h>
#include <sys/prctl.h>

using namespace uinhibit;

Fork::Fork() {}

Fork::~Fork() {
  close(inPipe[0]);
  close(inPipe[1]);
  close(outPipe[0]);
  close(outPipe[1]);
}

static void handleSig(int param) {
  std::jthread([]() {
    sleep(1);
    exit(0);
  }).detach();
}

void Fork::run() {
  pid_t ppid_before_fork = getpid();
  pid_t pid;
  if (pipe(inPipe) == -1 || pipe(outPipe) == -1) throw std::runtime_error("Failed to create pipe");
  if ((pid = fork()) < 0) throw std::runtime_error("Failed to fork");

  if (pid == 0) try {
    // Child
    this->child = true;

    // Make sure we exit if we become orphaned
    std::jthread([](pid_t before_fork) {
      while (getppid() == before_fork) sleep(5);
      sleep(1);
      exit(0);
    }, ppid_before_fork).detach();

    close(inPipe[1]); // Close our own write side
    close(outPipe[0]); // Close our own read side
    signal(SIGINT, handleSig); // So we stay alive long enough to free stuff
    this->childSetup();
    this->doRun();
    close(inPipe[0]);
    close(outPipe[1]);
    exit(0);
  } catch (...) {
    std::terminate();
    exit(1);
  }

  // Parent
  close(inPipe[0]); // Close our own read side
  close(outPipe[1]); // Close our own write side
};

void Fork::tx(std::string str) {
  if (write(((child) ? outPipe[1] : inPipe[1]), str.c_str(), str.size()) < 0)
    throw std::runtime_error("tx failed\n");

  fsync(((child) ? outPipe[1] : inPipe[1]));
}

std::string Fork::rx() {
  std::string str;

  char buf[1024] = "";
  int64_t got = read(((child) ? inPipe[0] : outPipe[0]), &buf, sizeof(buf));
  if (got < 0) throw std::runtime_error("Failed to read from pipe");
  str += buf;
  return str;
}

std::string Fork::rxLine() {
  while (1) {
    if (this->lineBuf.find('\n') == std::string::npos)
      this->lineBuf += this->rx();

    std::string out;
    int64_t newline = -1;
    int64_t i = 0;
    for (auto c : this->lineBuf) {
      if (c == '\n') { newline = i; break; }
      out.push_back(c);
      i++;
    }

    if (newline >= 0) this->lineBuf.erase(0,newline+1); // Remove this message from buf
    if (this->lineBuf.size() > 1024*1024) throw std::runtime_error("Buffer overflow");

    return out;
  }
}
