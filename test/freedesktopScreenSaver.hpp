bool except = false;
try {
  Quiet q;
  FreedesktopScreenSaverInhibitor i1([](auto a, auto b){}, [](auto a, auto b){});
} catch(...) { except = true; }
assert(!except, "Constructor generates no exceptions with valid setup");

{
  Quiet q;
  FreedesktopScreenSaverInhibitor i([](auto a, auto b){}, [](auto a, auto b){});
  InhibitorSession session(&i);

  const char* appname = "appname";
  const char* reason = "reason";
  auto r = dbus.newMethodCall("org.freedesktop.ScreenSaver", "/ScreenSaver",
                              "org.freedesktop.ScreenSaver", "Inhibit")
    .appendArgs(DBUS_TYPE_STRING, &appname, DBUS_TYPE_STRING, &reason, DBUS_TYPE_INVALID)
    ->sendAwait(200);

  bool hasResult = assert(!r.isNull(), "Implementation mode: Returns a result when calling Inhibit"
                          " over D-Bus");

  uint32_t cookie = 0;
  bool validCookie = assert(hasResult, [&r, &cookie]() {
    r.getArgs(DBUS_TYPE_UINT32, &cookie, DBUS_TYPE_INVALID);

    return cookie > 0;
  }, "Implementation mode: Returns a valid cookie (> 0) when calling Inhibit over D-Bus");

  int64_t size = -1;
  session.runInThread([&size, &i](){ size = i.activeInhibits.size(); });

  bool inhibitExists = assert(size == 1, "Implementation mode: Calling Inhibit over D-Bus results"
                              " in 1 active inhibit");

  assert(inhibitExists, [&i, &appname, &reason]() {
    Inhibit in;
    for (auto& [id, lin] : i.activeInhibits) { in = lin; break; }
    return ((in.appname == std::string(appname))
            && (in.reason == std::string(reason))
            && (in.type == InhibitType::SCREENSAVER));
  }, "Implementation mode: Calling Inhibit over D-Bus results in an Inhibit object stored that"
  " looks like the inhibit we requested");

  assert(validCookie && inhibitExists, [&dbus, &cookie]() {
    bool noreply = false;
    bool null = false;
    try {
      auto r = dbus.newMethodCall("org.freedesktop.ScreenSaver", "/ScreenSaver",
                             "org.freedesktop.ScreenSaver", "UnInhibit")
        .appendArgs(DBUS_TYPE_UINT32, &cookie, DBUS_TYPE_INVALID)
        ->sendAwait(200);
      null = r.isNull();
    } catch (DBus::NoReplyError& e) { noreply = true; }
    return !null && !noreply;
  }, "Implementation mode: Replies to a call to UnInhibit");

  // TODO inhibit/uninhibit handlers get called in the right situations
}
