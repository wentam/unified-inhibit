bool except = false;
try {
  Quiet q;
  FreedesktopScreenSaverInhibitor i1([](auto a, auto b){}, [](auto a, auto b){});
} catch(...) { except = true; }
assert(!except, "Constructor generates no exceptions with valid setup");

except = false;
try {
  Quiet q;
  FreedesktopScreenSaverInhibitor i1([](auto a, auto b){}, [](auto a, auto b){});
  FreedesktopScreenSaverInhibitor i2([](auto a, auto b){}, [](auto a, auto b){});
} catch(...) { except = true; }
assert(!except, "Constructor generates no exceptions with valid setup when someone else is"
       " implementing this interface");

for (int j = 0; j < 2; j++) {
  Quiet q;
  bool monitor = (j == 1);

  std::string mode = "Implementation";

  std::unique_ptr<FreedesktopScreenSaverInhibitor> impl_i;
  std::unique_ptr<InhibitorSession> impl_session;
  if (monitor) {
    mode = "Monitoring";

    impl_i = std::unique_ptr<FreedesktopScreenSaverInhibitor>(
      new FreedesktopScreenSaverInhibitor([](auto a, auto b){}, [](auto a, auto b){}));
    impl_session = std::unique_ptr<InhibitorSession>(new InhibitorSession(impl_i.get()));
    usleep(50*1000); // Give D-Bus a chance to ensure name is fully claimed
  }

  uint64_t inhibitCB_calls = 0;
  uint64_t uninhibitCB_calls = 0;
  Inhibit lastCBInhibit;
  Inhibit lastCBUnInhibit;
  FreedesktopScreenSaverInhibitor i(
    [&inhibitCB_calls, &lastCBInhibit](auto a, Inhibit in){
      inhibitCB_calls++;
      lastCBInhibit = in;
    },
    [&uninhibitCB_calls, &lastCBUnInhibit](auto a, Inhibit in){
      uninhibitCB_calls++;
      lastCBUnInhibit = in;
    }
  );
  InhibitorSession session(&i);

  if (monitor) {
    assert(i.monitor,
           "If someone else has our interface implemented, we drop into monitoring mode");
  }

  uint64_t inhibitCB_callsBefore = inhibitCB_calls;

  const char* appname = "appname";
  const char* reason = "reason";
  auto r = dbus.newMethodCall("org.freedesktop.ScreenSaver", "/ScreenSaver",
                              "org.freedesktop.ScreenSaver", "Inhibit")
    .appendArgs(DBUS_TYPE_STRING, &appname, DBUS_TYPE_STRING, &reason, DBUS_TYPE_INVALID)
    ->sendAwait(200);

  bool hasResult = assert(!r.isNull(), mode+" mode: Returns a result when calling Inhibit"
                          " over D-Bus");

  uint32_t cookie = 0;
  bool validCookie = assert(hasResult, [&r, &cookie]() {
    r.getArgs(DBUS_TYPE_UINT32, &cookie, DBUS_TYPE_INVALID);

    return cookie > 0;
  }, mode+" mode: Returns a valid cookie (> 0) when calling Inhibit over D-Bus");

  int64_t size = -1;
  session.runInThread([&size, &i](){ size = i.activeInhibits.size(); });

  bool inhibitExists = assert(size == 1, mode+" mode: Calling Inhibit over D-Bus results"
                              " in 1 active inhibit in object state");

  assert(inhibitExists, [&i, &appname, &reason]() {
    Inhibit in;
    for (auto& [id, lin] : i.activeInhibits) { in = lin; break; }
    return ((in.appname == std::string(appname))
            && (in.reason == std::string(reason))
            && (in.type == InhibitType::SCREENSAVER));
  }, mode+" mode: Calling Inhibit over D-Bus results in an Inhibit object stored that"
  " looks like the inhibit we requested");

  usleep(100*1000); // Give D-Bus some time
  bool cb = assert(inhibitCB_calls-inhibitCB_callsBefore == 1, mode+" mode: Inhibit call over D-Bus calls our inhibit callback");

  assert(cb, [&lastCBInhibit, &appname, &reason]() {
    return ((lastCBInhibit.appname == std::string(appname))
            && (lastCBInhibit.reason == std::string(reason))
            && (lastCBInhibit.type == InhibitType::SCREENSAVER));
  },mode+" mode: Inhibit object passed into inhibit callback looks like the inhibit we requested");

  uint64_t uninhibitCB_callsBefore = uninhibitCB_calls;

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
  }, mode+" mode: Replies to a call to UnInhibit");

  int64_t size2 = -1;
  session.runInThread([&size2, &i](){ size2 = i.activeInhibits.size(); });
  assert(size2 == 0, mode+" mode: UnInhibit removes the active inhibit from object state");

  usleep(100*1000); // Give D-Bus some time
  cb = assert(uninhibitCB_calls-uninhibitCB_callsBefore == 1,
              mode+" mode: UnInhibit call over D-Bus calls our uninhibit callback");

  assert(cb, [&lastCBUnInhibit, &appname, &reason]() {
    return ((lastCBUnInhibit.appname == std::string(appname))
            && (lastCBUnInhibit.reason == std::string(reason))
            && (lastCBUnInhibit.type == InhibitType::SCREENSAVER));
  },mode+" mode: Inhibit object passed into uninhibit callback looks like the inhibit we released");


  // --- inhibit() ---

  InhibitRequest req = {
    .type = InhibitType::SCREENSAVER,
    .appname = "appname-req",
    .reason = "reason-req",
  };

  Inhibit inhibit;

  int64_t size3 = -1;
  if (monitor)
    impl_session->runInThread([&size3, &impl_i](){ size3 = impl_i->activeInhibits.size(); });

  inhibitCB_callsBefore = inhibitCB_calls;
  bool except = false;
  try { inhibit = i.inhibit(req); } catch (...) { except = true; }

  if(monitor) usleep(100*1000); // Give D-Bus some time

  assert(!except, mode+" mode: inhibit() with a valid request generates no exceptions");
  assert((inhibitCB_calls-inhibitCB_callsBefore) == 0,
         mode+" mode: inhibit() with a valid request doesn't call inhibit callback");

  session.runInThread([&size, &i](){ size = i.activeInhibits.size(); });

  bool haveInhibit = assert(size == 1, mode+" mode: inhibit() with a valid request results in 1"
                            " active inhibit in object state");

  assert(haveInhibit, [&i, &req]() {
    Inhibit in;
    for (auto& [id, lin] : i.activeInhibits) { in = lin; break; }
    return ((in.appname == std::string(req.appname))
            && (in.reason == std::string(req.reason))
            && (in.type == InhibitType::SCREENSAVER));
  },mode+" mode: active inhibit in our state from call to inhibit() is valid");

  int64_t size4 = -1;
  bool haveImplInhibit = false;
  if (monitor) {
    impl_session->runInThread([&size4, &impl_i](){ size4 = impl_i->activeInhibits.size(); });

    haveImplInhibit = assert(size4-size3 == 1, mode+" mode: inhibit() results in 1 new"
                             " activeInhibit on the implementor side");
  }

  // --- unInhibit() ---

  uninhibitCB_callsBefore = uninhibitCB_calls;
  except = false;
  try {
    i.unInhibit(inhibit.id);
  } catch (...) {
    except = true;
  }

  if(monitor) usleep(100*1000); // Give D-Bus some time

  assert(!except, mode+" mode: unInhibit() with a valid inhibit id for an active inhibit generates no exceptions");
  assert((uninhibitCB_calls-uninhibitCB_callsBefore) == 0,
         mode+" mode: unInhibit() with a valid inhibit id for an active inhibit doesn't call inhibit callback");

  session.runInThread([&size, &i](){ size = i.activeInhibits.size(); });

  assert(haveInhibit, size == 0,
         mode+" mode: unInhibit() after inhibit() with a valid request results in 0"
         " active inhibits in object state");

  if (monitor) {
    size4 = -1;
    impl_session->runInThread([&size4, &impl_i](){ size4 = impl_i->activeInhibits.size(); });

    assert(haveImplInhibit, size4 == size3,
           mode+" mode: unInhibit() after inhibit() with a valid request results in the removal of"
           " the inhibit from the implementor's object state");
  }

  // --- inhibit() ---

  session.runInThread([&size, &i](){ size = i.activeInhibits.size(); });
  if(monitor)
    impl_session->runInThread([&size3, &impl_i](){ size3 = impl_i->activeInhibits.size(); });

  except = false;
  try {
    req.type = InhibitType::SUSPEND;
    i.inhibit(req);
  } catch (InhibitRequestUnsupportedTypeException& e) {
    except = true;
  }

  assert(except,
         mode+" mode: inhibit() with the wrong inhibit type throws InhibitRequestUnsupportedTypeException");

  session.runInThread([&size2, &i](){ size2 = i.activeInhibits.size(); });

  assert(size2 == size, mode+" mode: inhibit() with the wrong inhibit type doesn't add any active"
         " inhibits to object state");

  if (monitor) {
    impl_session->runInThread([&size4, &impl_i](){ size4 = impl_i->activeInhibits.size(); });
    assert(size4 == size3, mode+" mode: inhibit with the wrong inhibit type dosen't add any"
           " active inhibits to the implementor's object state");
  }

  // Application crash/sender dissappear
  {
    int64_t size = -1;
    int64_t size2 = -1;
    int64_t size3 = -1;
    session.runInThread([&size, &i](){ size = i.activeInhibits.size(); });

    {
      DBus dbus2(DBUS_BUS_SESSION);
      const char* appname = "appname";
      const char* reason = "reason";
      auto r = dbus2.newMethodCall("org.freedesktop.ScreenSaver", "/ScreenSaver",
                                  "org.freedesktop.ScreenSaver", "Inhibit")
        .appendArgs(DBUS_TYPE_STRING, &appname, DBUS_TYPE_STRING, &reason, DBUS_TYPE_INVALID)
        ->sendAwait(200);

      usleep(100*1000); // Give D-Bus some time

      session.runInThread([&size2, &i](){ size2 = i.activeInhibits.size(); });
    }

    usleep(100*1000); // Give D-Bus some time
    session.runInThread([&size3, &i](){ size3 = i.activeInhibits.size(); });

    assert((size2 == size+1) && (size3 == size),mode+" mode: If a sender inhibits but dissappears"
           " (ie. application crashes), the inhibit gets automatically released");
  }
}
