#pragma once
#include "testutils.hpp"
#include "simpleDbusAssertions.hpp"
using namespace uinhibit;

static void freedesktopScreenSaverAssertions(DBus& dbus) {
  simpleDbusAssertions(
    dbus,
    "org.freedesktop.ScreenSaver",
    "org.freedesktop.ScreenSaver",
    "/ScreenSaver",
    InhibitType::SCREENSAVER,
    [](auto in1, auto in2) {
      return std::shared_ptr<SimpleDBusInhibitor>(new FreedesktopScreenSaverInhibitor(in1, in2));
    }
    );
}
