#include "Fork.hpp"

using namespace uinhibit;

#define THIS NewlineMessageFork

THIS::THIS() : Fork() {}

void THIS::doRun() {
  // TODO: use rxLine()?
  std::string buf;
  while(1) {
    if (buf.find('\n') == std::string::npos)
      buf += this->rx();

    std::string out;
    int64_t newline = -1;
    int64_t i = 0;
    for (auto c : buf) {
      if (c == '\n') { newline = i; break; }
      out.push_back(c);
      i++;
    }

    this->handleMsg(out);

    if (newline >= 0) buf.erase(0,newline+1); // Remove this message from buf
    if (buf.size() > 1024*1024) throw std::runtime_error("Buffer overflow");
  }
}
