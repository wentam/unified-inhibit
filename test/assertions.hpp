#pragma once
#include "freedesktopScreenSaver.hpp"
#include "freedesktopPowerManager.hpp"
#include "gnomeScreenSaverAssertions.hpp"
#include "DBus.hpp"
using namespace uinhibit;

static void assertions(DBus& dbus) {
  puts(ANSI_COLOR_BOLD_YELLOW "org.freedesktop.ScreenSaver:" ANSI_COLOR_RESET);
  freedesktopScreenSaverAssertions(dbus);

  puts(ANSI_COLOR_BOLD_YELLOW "\norg.freedesktop.PowerManager:" ANSI_COLOR_RESET);
  freedesktopPowerManagerAssertions(dbus);

  puts(ANSI_COLOR_BOLD_YELLOW "\norg.gnome.ScreenSaver:" ANSI_COLOR_RESET);
  gnomeScreenSaverAssertions(dbus);
}
