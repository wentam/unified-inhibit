#pragma once
#include "testutils.hpp"
#include "simpleDbusAssertions.hpp"
using namespace uinhibit;

static void freedesktopPowerManagerAssertions(DBus& dbus) {
  simpleDbusAssertions(
    dbus,
    "org.freedesktop.PowerManager",
    "org.freedesktop.PowerManager",
    "/PowerManager",
    InhibitType::SUSPEND,
    [](auto in1, auto in2) {
      return std::shared_ptr<SimpleDBusInhibitor>(new FreedesktopPowerManagerInhibitor(in1, in2));
    });


  for (int j = 0; j < 2; j++) {
    Quiet q;
    bool monitor = (j == 1);

    std::string mode = "Implementation";

    std::shared_ptr<SimpleDBusInhibitor> impl_i;
    std::unique_ptr<InhibitorSession> impl_session;
    if (monitor) {
      mode = "Monitoring";

      impl_i = std::shared_ptr<FreedesktopPowerManagerInhibitor>(
        new FreedesktopPowerManagerInhibitor([](auto a, auto b){}, [](auto a, auto b){})
      );
      impl_session = std::unique_ptr<InhibitorSession>(new InhibitorSession(impl_i.get()));
      usleep(50*1000); // Give D-Bus a chance to ensure name is fully claimed
    }

    FreedesktopPowerManagerInhibitor i([](auto a, Inhibit in){}, [](auto a, Inhibit in){});
    InhibitorSession session(&i);

    // --- D-Bus HasInhibit ---

    bool noReply = false;
    bool isNull = false;
    int32_t response = -1;
    try {
      auto r = dbus.newMethodCall("org.freedesktop.PowerManager", "/PowerManager",
                                "org.freedesktop.PowerManager", "HasInhibit").sendAwait(200);
      isNull = r.isNull();

      r.getArgs(DBUS_TYPE_BOOLEAN, &response, DBUS_TYPE_INVALID);
    } catch (DBus::NoReplyError& e) { noReply = true; }

    bool hasResult = assert(!isNull && !noReply,
                            mode+" mode: Returns a result when calling HasInhibit over D-Bus");

    assert(hasResult, response == 0, mode+" mode: HasInhibit returns false when no inhibits are present");

    const char* appname = "appname";
    const char* reason = "reason";
    auto r = dbus.newMethodCall("org.freedesktop.PowerManager", "/PowerManager",
                                "org.freedesktop.PowerManager", "Inhibit")
      .appendArgs(DBUS_TYPE_STRING, &appname, DBUS_TYPE_STRING, &reason, DBUS_TYPE_INVALID)
      ->sendAwait(200);

    // HasInhibit returns true when inhibits are present
    assert(hasResult, [&dbus]() {
      bool noReply = false;
      bool isNull = false;
      int32_t response = -1;
      try {
        auto r = dbus.newMethodCall("org.freedesktop.PowerManager", "/PowerManager",
                                    "org.freedesktop.PowerManager", "HasInhibit").sendAwait(200);
        isNull = r.isNull();

        r.getArgs(DBUS_TYPE_BOOLEAN, &response, DBUS_TYPE_INVALID);
      } catch (DBus::NoReplyError& e) { noReply = true; }

      return !noReply && !isNull && response == 1;
    }, mode+" mode: HasInhibit returns true when inhibits are present");
  }

  // TODO HasInhibitChanged signal tests
}
