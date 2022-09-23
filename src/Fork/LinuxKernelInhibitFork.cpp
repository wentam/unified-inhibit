#include "Fork.hpp"
#include <fcntl.h>

using namespace uinhibit;

#define THIS LinuxKernelInhibitFork
#define WAKE_LOCK_PATH "/sys/power/wake_lock"
#define WAKE_UNLOCK_PATH "/sys/power/wake_unlock"

THIS::THIS() : NewlineMessageFork() {};

void THIS::childSetup() {
  if (setresuid(0,0,0) != 0) {
    // All good, we will probably just fail to take any locks
  }

  if (access(WAKE_LOCK_PATH, W_OK) != 0) tx("nowrite"); else tx("good");
}

void THIS::handleMsg(std::string msg) {
  if (msg.size() == 0) return;

  if (msg.front() == '\t') {
    msg.erase(0,1);
    int32_t wfd = open(WAKE_UNLOCK_PATH, O_WRONLY);
    dprintf(wfd, "%s\n", msg.c_str());
    close(wfd);
  } else {
    int32_t wfd = open(WAKE_LOCK_PATH, O_WRONLY);
    dprintf(wfd, "%s\n", msg.c_str());
    close(wfd);
  }
}
