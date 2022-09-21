#pragma once
#include "testutils.hpp"
#include "simpleDbusAssertions.hpp"
using namespace uinhibit;

static void mateScreenSaverAssertions(DBus& dbus) {
  simpleDbusAssertions(
    dbus,
    "org.mate.ScreenSaver",
    "org.mate.ScreenSaver",
    "/org/mate/ScreenSaver",
    InhibitType::SCREENSAVER,
    [](auto in1, auto in2) {
      return std::shared_ptr<SimpleDBusInhibitor>(new MateScreenSaverInhibitor(in1, in2));
    }
    );
}
