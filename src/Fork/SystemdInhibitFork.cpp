#include "Fork.hpp"
#include "myExcept.hpp"
#include <regex>
#include "util.hpp"

using namespace uinhibit;

#define THIS SystemdInhibitFork
#define INTERFACE "org.freedesktop.login1.Manager"
#define DBUSNAME "org.freedesktop.login1"
#define PATH "/org/freedesktop/login1"

THIS::THIS() : NewlineMessageFork() {}

void THIS::childSetup() {
  if (setresuid(0,0,0) != 0) {
    // All good, we just might get access denied depending on PolicyKit config
  }

  dbus = std::unique_ptr<DBus>(new DBus(DBUS_BUS_SYSTEM));
  this->tx(std::string(dbus->getUniqueName())+'\n');
}

void THIS::handleMsg(std::string msg) {
  if (msg.size() == 0) return;

  // Count up tabs
  int32_t tabs = 0; for (auto c : msg) if (c == '\t') tabs++;

  if (tabs == 3) {
    // Inhibit
    std::vector<std::string> strings; strings.push_back("");
    for (auto c : msg) {
      if (c == '\t') strings.push_back("");
      else strings.back() += c;
    }

    if (strings.size() < 4) return;

    try {
      int32_t fd = this->call(strings.at(0),
                              strings.at(1),
                              strings.at(2),
                              strings.at(3));

      this->tx(std::to_string(fd)+"\n");
    } catch (DBus::AccessDeniedError& e) {
      std::string justIdle = strings.at(0);
      justIdle = std::regex_replace(justIdle, std::regex("sleep"), "");
      justIdle = std::regex_replace(justIdle, std::regex("::"), ":");
      justIdle = std::regex_replace(justIdle, std::regex(":$"), "");
      justIdle = std::regex_replace(justIdle, std::regex("^:"), "");

      bool retry = false;
      bool except = false;
      if (justIdle.size() > 0) try {
        retry = true;
        int32_t fd = this->call(justIdle,
                                strings.at(1),
                                strings.at(2),
                                strings.at(3));
        this->tx(std::to_string(fd)+"\n");
      } catch (DBus::AccessDeniedError& e) {
        except = true;
      }

      if ((retry && except) || !retry) {
        printf(ANSI_COLOR_YELLOW
               "Warning: access denied attempting org.freedesktop.login1 inhibit with what='%s'."
               " You might need to give me setuid (chown root uinhibitd && chmod 4755 uinhibitd)"
               " or configure PolicyKit."
               ANSI_COLOR_RESET "\n",
               strings.at(0).c_str());

        this->tx("-1\n");
      } else if (retry && !except) {
        printf(ANSI_COLOR_YELLOW
               "Warning: access denied attempting org.freedesktop.login1 inhibit with what='%s'."
               " We retried with what='%s' and it went through."
               " You might need to give me setuid (chown root uinhibitd && chmod 4755 uinhibitd)"
               " or configure PolicyKit."
               ANSI_COLOR_RESET "\n",
               strings.at(0).c_str(),
               justIdle.c_str());
      }
    }
  } else if (tabs == 0) {
    // Release
    close(atol(msg.c_str()));
  }
}

int32_t THIS::call(std::string what, std::string who, std::string why, std::string mode) {
  const char* swhat = what.c_str();
  const char* swho  = who.c_str();
  const char* swhy  = why.c_str();
  const char* smode = mode.c_str();

  try {
    auto replymsg = dbus->newMethodCall(DBUSNAME, PATH, INTERFACE, "Inhibit")
      .appendArgs(DBUS_TYPE_STRING, &swhat,
                  DBUS_TYPE_STRING, &swho,
                  DBUS_TYPE_STRING, &swhy,
                  DBUS_TYPE_STRING, &smode,
                  DBUS_TYPE_INVALID)
      ->sendAwait(500);
    int32_t fd = -1;
    if(replymsg.notNull()) replymsg.getArgs(DBUS_TYPE_UNIX_FD, &fd, DBUS_TYPE_INVALID);
    return fd;
  } catch (DBus::NoReplyError& e) {
    throw InhibitNoResponseException();
  }
}
