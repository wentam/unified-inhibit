

# org.freedesktop.login1 (systemd inhibit)

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

# setuid

If the setuid bit is set with `chown root uinhibitd && chmod 4755 uinhibitd`, uinhibitd will attempt
to initialize the system bus inhibitors as root. It will then immediatly drop privileges back to the
calling user. This is probably needed on most systems to monitor to the system bus.

This occurs with a sanitized environment before user input is parsed to keep things secure.
