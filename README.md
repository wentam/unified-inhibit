# unified-inhibit - Make wakelocks actually work

* [The problem](#the-problem)
  * [What about caffeine-ng/xidlehook?](#what-about-caffeine-ngxidlehook)
* [What unified-inhibit does](#what-unified-inhibit-does)
* [Setup/configuration](#setupconfiguration)
  * [setuid](#setuid)
  * [org.freedesktop.login1 (systemd inhibit)](#orgfreedesktoplogin1-systemd-inhibit)
* [Supported inhibit interfaces](#supported-inhibit-interfaces)
* [Try it out](#try-it-out-nix)
* [Usage](#usagecli-actions)
* [Dependencies](#dependencies)
* [Building](#building)
* [Donations](#donations)

## The problem

Linux wakelocks/inhibitors are a fragmented nightmare.

When an app like VLC is playing a video, it needs to let your system know not to show the screensaver/lock the screen/suspend the system.
These requests are sometimes called inhibits or wakelocks. The linux desktop has no single agreed-upon standard for how
applications should send this message. Instead there are more than 10 different solutions that may or may not
be present on your system.

If you're a user, you pretty much need to be running gnome or KDE for inhibitors to work correctly and reliably.
If you're an application developer, you need to support a long list of inhibit mechanisms to get
things working reliably everywhere. It's painful for everyone.

This is the bad kind of fragmentation: These aren't unique approaches to solving a given problem
with strong pros and cons; this is a large set of standards all doing the same thing in almost the
exact same way. We're just trying to set and track a few boolean values.

### What about [caffeine-ng](https://codeberg.org/WhyNotHugo/caffeine-ng)/[xidlehook](https://github.com/jD91mZM2/xidlehook)?

These tools can inhibit based on any applications that are fullscreen or playing audio. This is a useful heuristic to
work around the issue for some use-cases, but does not cover this problem nearly as flexibly/durably as explicit wakelocks.

Applications have more context information regarding when a wakelock is actually needed.
Sometimes you may want wakelocks that have nothing to do with the graphical session/audio. Sometimes you have audio or a fullscreen
application that shouldn't prevent sleep.

Consider:
* A full screen game at it's title screen/menu playing audio. No reason to inhibit.
* An ssh server that inhibits sleep while there are actively connected sessions.
* Notification audio that could pointlessly keep the system awake.

Applications are already telling us when we need these inhibits. Just not in a nice standard way.

## What unified-inhibit does

unified-inhibit (uinhibitd) implements/listens on as many inhibit interfaces as possible
forwarding all inhibit events to all interfaces. If an app inhibits the screensaver via
org.gnome.ScreenSaver, we will also forward that event to org.freedesktop.ScreenSaver (and all of
the other interfaces that accept a screensaver inhibit event).

This means all interfaces effectively share the same state. Use any interface of your choosing,
you'll get all of the events.

uinhibitd can also run shell commands on inhibit/release providing an easy way to get wakelocks working on systems that don't otherwise have one of these interfaces implemented. This is useful for users of lightweight window managers/autolockers.

## Setup/configuration

### setuid

Linux kernel wakelocks and org.freedesktop.login1 will probably require that uinhibitd has setuid:

```
chown root uinhibitd
chmod 4755 uinhibitd
```

The setuid requirement is not ideal, but necessary as we need to perform actions as both the session
user and root.

The necessary precautions to avoid privilage escalation (such as a clean environment) have been
taken.

### org.freedesktop.login1 (systemd inhibit)

Because this interface resides on the system bus, it requires special consideration.

In monitoring mode (on a systemd system), uinhibitd probably requires the setuid bit to gain access.
You might need to configure D-Bus as well depending on how your system is set up.

In implementation mode (on a non-systemd system), uinhibitd needs to have permission to claim the
name org.freedesktop.login1 on the system bus. What is needed here depends on how your system is
configured. I've accomplished this on my system by giving uinhibitd setuid and adding the
following D-Bus rules in /etc/dbus-1/system.d/root-can-own-login1.conf:

```xml
<busconfig>
  <policy user="root">
    <allow own="org.freedesktop.login1"/>
    <allow send_type="*"/>
  </policy>
</busconfig>
```

## Supported inhibit interfaces
Interface | Protocol | Direction | Notes
---|---|---|---
org.freedesktop.login1 | D-Bus | <-> | Probably need to chown root && chmod 4775 uinhibitd (setuid). Might need to configure D-Bus.
org.freedesktop.ScreenSaver | D-Bus | <-> |
org.freedesktop.PowerManager | D-Bus | <-> |
org.gnome.SessionManager | D-Bus | <-> |
org.gnome.ScreenSaver | D-Bus | <-> |
org.cinnamon.ScreenSaver | D-Bus | <-> |
org.mate.ScreenSaver | D-Bus | <-> |
Linux kernel wakelock | sysfs | <-> | Probably need to chown root && chmod 4775 uinhibitd (setuid)
X11 dpms+xscreensaver | libX11+libXss | -> |
xautolock | shell | -> |
xidlehook | shell | -> | Start xidlehook with --socket /tmp/xidlehook.sock
Sxmo      | shell | <-> | If in ssh/tty: export DBUS_SESSION_BUS_ADDRESS=$(cat $XDG_RUNTIME_DIR/dbus.bus) before running uinhibitd

More are out there. Please open issues for missing interfaces!

D-Bus interfaces will implement their interface when nobody else is.

When another application has an interface implemented, we will eavesdrop via D-Bus
[BecomeMonitor](https://dbus.freedesktop.org/doc/dbus-specification.html#bus-messages-become-monitor).

## Try it out (nix)

```
nix run github:wentam/unified-inhibit
```
(interfaces requiring setuid will not work like this)

## Dependencies

* libdbus
* [pkgconf](https://github.com/pkgconf/pkgconf) (to find D-Bus headers at build time)
* libX11 (optional, for dpms and xscreensaver, set X11=0 when running make to disable)
* libXss/libXScrnSaver (optional, for dpms and xscreensaver, set X11=0 when running make to disable)

## Building

Release build:
```
make -j12 release test
```
or with debugging symbols:
```
make -j12 debug test
```
or with nix (release build):
```
nix build
```

## Usage/CLI actions

Start the daemon with:
```
uinhibitd
```

The daemon accepts optional command-line arguments to run shell commands on inhibit events. This is
useful for users of lightweight window managers that want to tie in their screen locker or manage
auto-suspend:

```
uinhibitd --inhibit-action "some-auto-locker --disable" \
          --uninhibit-action "some-auto-locker --enable"
```

Disables the auto locker while there is an active inhibit. This will capture the inhibit state from
all interfaces.

--ia/--uia are shorthand for these flags. You can also specify something like
--screensaver-inhibit-action to only act on screensaver inhibits. See man page for details.


## Donations
Much of my time is volunteered towards open-source projects to improve the free software ecosystem
for all.

[You can support my work here](https://liberapay.com/wentam) :+1:.
