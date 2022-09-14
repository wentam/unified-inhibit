# unified-inhibit

## The problem

Linux wakelocks/inhibitors are a fragmented nightmare.

If you're a user, you pretty much need to be running gnome or KDE for inhibitors to work correctly.
If you're an application developer, you need to support a long list of inhibitor mechanisms to get
things working reliably everywhere. It's painful for everyone.

This is the bad kind of fragmentation: These aren't unique approaches to solving a given problem
with strong pros and cons; this is a large set of standards all doing the same thing in almost the
exact same way. We're just trying to set and track a few boolean values.

## What this does about it

unified-inhibit (uinhibitd) implements/listens on as many inhibitor interfaces as possible
forwarding all inhibit events to all interfaces. If an app inhibits the screensaver via
org.gnome.ScreenSaver, we will also forward that event to org.freedesktop.ScreenSaver (and all of
the other inhibitors that accept a screensaver inhibit event).

This means all interfaces effectively share the same state. Use any interface of your choosing,
you'll get all of the events.

Currently support inhibitors |
---|---
org.freedesktop.login1 |
org.freedesktop.ScreenSaver |
org.freedesktop.PowerManager |
org.gnome.SessionManager |
org.gnome.ScreenSaver |
org.cinnamon.ScreenSaver |
Linux kernel wakelock |

More are out there. Please open issues for missing interfaces!

### D-Bus inhibitors

D-Bus inhibitors will implement their interface when nobody else is implementing the interface.

When another application has an interface implemented, we will eavesdrop via D-Bus
[BecomeMonitor](https://dbus.freedesktop.org/doc/dbus-specification.html#bus-messages-become-monitor).

### CLI actions

The daemon accepts command-line arguments to run shell commands on inhibit events. This is useful
for users of lightweight window managers that want to tie in their screen locker or manage
auto-suspend:

```
uinhibitd --inhibit-screensaver-action "xautolock -disable" \
          --uninhibit-screensaver-action "xautolock -enable"
```

Disables xautolocker while there is an active screensaver inhibit. This will capture the inhibit
state from all interfaces.

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
    <allow own="*"/>
    <allow send_type="*"/>
  </policy>
</busconfig>
```
