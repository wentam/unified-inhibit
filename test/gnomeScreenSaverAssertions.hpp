#pragma once
#include "testutils.hpp"
#include "simpleDbusAssertions.hpp"
using namespace uinhibit;

static void gnomeScreenSaverAssertions(DBus& dbus) {
  simpleDbusAssertions(
    dbus,
    "org.gnome.ScreenSaver",
    "org.gnome.ScreenSaver",
    "/org/gnome/ScreenSaver",
    InhibitType::SCREENSAVER,
    [](auto in1, auto in2) {
      return std::shared_ptr<SimpleDBusInhibitInterface>(new GnomeScreenSaverInhibitInterface(in1, in2));
    }
  );

  // TODO: simactivity creates a lock
  // TODO: simactivity created lock is valid in object
  // TODO: simactivity releases after 5min
  // TODO: simactivity lock sticks even if sender disappears
}
