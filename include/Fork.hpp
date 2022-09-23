#pragma once

#include "DBus.hpp"
#include <string>

namespace uinhibit {
  class Fork {
    public:
      Fork();

      void tx(std::string);
      std::string rx(); // blocking

      void run();

    protected:
      virtual void doRun() = 0;
      virtual void childSetup() = 0;
      bool child = false;

    private:
      int32_t inPipe[2];
      int32_t outPipe[2];
  };

  class NewlineMessageFork : public Fork {
    public:
      NewlineMessageFork();
      void doRun() override;

    protected:
      // Will always be exactly one line. Excludes the newline.
      virtual void handleMsg(std::string) = 0;
  };

  class SystemdInhibitFork : public NewlineMessageFork {
    public:
      SystemdInhibitFork();
      void childSetup() override;

      // Inhibit: 'what\twho\twhy\tmode\n' -> '37\n'
      // Release: '37\n'
      void handleMsg(std::string) override;

    private:
      std::unique_ptr<DBus> dbus;
      int32_t call(std::string what, std::string who, std::string why, std::string mode);
  };

}
