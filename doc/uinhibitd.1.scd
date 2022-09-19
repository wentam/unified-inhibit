uinhibitd(1)

# NAME

uinhibitd - make wakelocks actually work

# SYNOPSIS

*uinhibitd*++
*uinhibitd* [*--inhibit-action* "_cmd_"] [*--uninhibit-action*
"_cmd_"]

# DESCRIPTION

*uinhibitd* implements/listens on as many inhibit/wakelock interfaces as
possible forwarding all inhibit events to all interfaces. If an app inhibits the
screensaver via org.gnome.ScreenSaver, we will also forward that event to
org.freedesktop.ScreenSaver (and all of the other inhibitors that accept a
screensaver inhibit event).

This means all interfaces effectively share the same state. Use any interface of
your choosing, you'll get all of the events.

*uinhibitd* can also run shell commands on inhibit/release providing an easy way
to get wakelocks working on systems that don't otherwise have one of these
interfaces implemented. This is useful for users of lightweight window
managers/autolockers.

# FLAGS

Single-letter flags can be bundled (-abcd)

*--help*, *-h*
	Displays this man page and exits

*--version*
	Prints version information and exits

# PARAMETERS

*--inhibit-action* "_cmd_", *--ia* "_cmd_"
	Shell command to run every time the state has changed to inhibited (of any inhibit type)

*--uninhibit-action* "_cmd_", *--uia* "_cmd_"
	Shell command to run every time the state has changed to uninhibited (all inhibit types)

# AUTHOR

Written by Matt Egeler

# BUGS

https://github.com/wentam/unified-inhibit/issues
