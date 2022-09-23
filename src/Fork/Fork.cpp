#include "Fork.hpp"
#include <stdexcept>

using namespace uinhibit;

Fork::Fork() {}

void Fork::run() {
  pid_t pid;
  if (pipe(inPipe) == -1 || pipe(outPipe) == -1) throw std::runtime_error("Failed to create pipe");
  if ((pid = fork()) < 0) throw std::runtime_error("Failed to fork");

  if (pid == 0) try {
    // Child
    this->child = true;
    close(inPipe[1]); // Close our own write side
    close(outPipe[0]); // Close our own read side
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
