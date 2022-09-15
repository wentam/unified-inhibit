## Linux wakelocks/inhibitors are a fragmented nightmare.

If you're a user, you pretty much need to be running gnome or KDE for inhibitors to work correctly and reliably.
If you're an application developer, you need to support a long list of inhibitor mechanisms to get
things working reliably everywhere. It's painful for everyone.

This is the bad kind of fragmentation: These aren't unique approaches to solving a given problem
with strong pros and cons; this is a large set of standards all doing the same thing in almost the
exact same way. We're just trying to set and track a few boolean values.

## What unified-inhibit does about it

unified-inhibit (uinhibitd) implements/listens on as many inhibitor interfaces as possible
forwarding all inhibit events to all interfaces. If an app inhibits the screensaver via
org.gnome.ScreenSaver, we will also forward that event to org.freedesktop.ScreenSaver (and all of
the other inhibitors that accept a screensaver inhibit event).

This means all interfaces effectively share the same state. Use any interface of your choosing,
you'll get all of the events.

uinhibitd can also run shell commands on inhibit/release providing an easy way to get wakelocks working on systems that don't otherwise have one of these interfaces implemented.

## Currently supported inhibitors
Inhibitor | Protocol
---|---
org.freedesktop.login1 | D-Bus
org.freedesktop.ScreenSaver | D-Bus
org.freedesktop.PowerManager | D-Bus
org.gnome.SessionManager | D-Bus
org.gnome.ScreenSaver | D-Bus
org.cinnamon.ScreenSaver | D-Bus
Linux kernel wakelock | sysfs

More are out there. Please open issues for missing interfaces!

D-Bus inhibitors will implement their interface when nobody else is implementing the interface.

When another application has an interface implemented, we will eavesdrop via D-Bus
[BecomeMonitor](https://dbus.freedesktop.org/doc/dbus-specification.html#bus-messages-become-monitor).

### CLI actions

The daemon accepts command-line arguments to run shell commands on inhibit events. This is useful
for users of lightweight window managers that want to tie in their screen locker or manage
auto-suspend:

```
uinhibitd --inhibit-action "xautolock -disable" \
          --uninhibit-action "xautolock -enable"
```

Disables xautolocker while there is an active inhibit. This will capture the inhibit state from all
interfaces.

(inhibit-type specific flags are planned)

## Donations
Much of my time is volunteered towards open-source projects to improve the free software ecosystem
for all.

[You can support my work here](https://liberapay.com/wentam) :+1:.

## setuid

Linux kernel wakelocks and org.freedesktop.login1 will probably require that uinhibitd has setuid:

```shell
chown root uinhibitd
chmod 4755 uinhibitd
```

The setuid requirement is not ideal, but necessary as we need to perform actions as both the session
user and root.

The necessary precautions to avoid privilage escalation (such as a clean environment) have been
taken.

## org.freedesktop.login1 (systemd inhibit)

Because this interface resides on the system bus, it requires special consideration.

In monitoring mode (on a systemd system), uinhibitd probably requires the setuid bit to gain access.

In implementation mode (on a non-systemd system), uinhibitd needs to have permission to claim the
name org.freedesktop.login1 on the system bus. What is needed here depends on your existing
configuration. I've accomplished this on my system by giving uinhibitd setuid and adding the
following D-Bus rules in /etc/dbus-1/system.d/root-can-own-things.conf:

```xml
<busconfig>
  <policy user="root">
    <allow own="org.freedesktop.login1"/>
    <allow send_type="*"/>
  </policy>
</busconfig>
```

## Try it out (nix)

```
nix run github:wentam/unified-inhibit
```
(inhibitors requiring setuid will not work like this)

## Building

```
make
```
or
```
nix build
```
