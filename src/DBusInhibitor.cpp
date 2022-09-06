#include "Inhibitor.hpp"
#include <cstdio>
#include <unistd.h>
#include "util.hpp"
#include "DBus.hpp"

// TODO: if we started in monitoring mode but the interface becomes no longer implemented,
// we should probably take it over (implement at DBusInhibitor level)
//
// TODO: if we are implementing, allow someone to take over the name and implement in our place.
// if this happens, we need to fall back to monitoring

namespace uinhibit {
	DBusInhibitor::DBusInhibitor(
		std::function<void(Inhibitor*, Inhibit)> inhibitCB,
		std::function<void(Inhibitor*, Inhibit)> unInhibitCB,
		std::string interface,
		DBusBusType busType,
		std::vector<DBusMethodCB> myMethods,
		std::vector<DBusSignalCB> mySignals
	) 
	: 
		Inhibitor(inhibitCB, unInhibitCB),
		dbus(busType),
		myMethods(myMethods),
		mySignals(mySignals),
		interface(interface)
	{	
		this->monitor = dbus.nameHasOwner(interface.c_str());

		if (this->monitor) {
			try {
				std::vector<std::string> rules;

				for (auto method : myMethods) {
					std::string dest = "";
					if (method.destination != "*") dest = ",destination="+method.destination;
					rules.push_back("type='method_call',interface='"+method.interface+"',member='"+method.member+"'"+dest);
				}

				for (auto signal : mySignals)
					rules.push_back("type='signal',interface='"+signal.interface+"',member='"+signal.member+"'");

				// TODO: it may be possible to monitor only the method returns going to the sender that
				// happens to be implementing this interface (destination=org.freedesktop.ScreenSaver)
				rules.push_back("type='method_return'");

				std::vector<const char*> Crules;
				Crules.reserve(rules.size());

				for (auto& r : rules) {
					Crules.push_back(r.c_str());
				}

				// TODO: becomeMonitor should provide an overloaded version taking a vector of std::string
				dbus.becomeMonitor(Crules);

				printf("%s[" ANSI_COLOR_YELLOW "-" ANSI_COLOR_RESET "]: Someone "
							 "else has this dbus interface implemented. Became a monitor and will eavesdrop.\n",
							 interface.c_str());
			} catch (DBus::UnknownInterfaceError& e) {
				printf("%s[" ANSI_COLOR_RED "x" ANSI_COLOR_RESET "]: "
							 "UNSUPPORTED: Someone else has this dbus interface implemented. Tried to become a monitor in"
							 " order to eavesdrop but your dbus daemon doesn't appear to support that (it's "
							 "probably too old).\n", interface.c_str());
			} catch (DBus::AccessDeniedError& e) {
				printf("%s[" ANSI_COLOR_RED "x" ANSI_COLOR_RESET "]: "
							 "ACCESS DENIED: Someone else has this dbus interface implemented. Tried to become a monitor to"
							 " eavesdrop but was denied access. \n", interface.c_str());
			}
		} else {
			int ret = 0;

			try {
				ret = dbus.requestName(interface.c_str(), DBUS_NAME_FLAG_REPLACE_EXISTING);
			} catch (const DBus::AccessDeniedError& e) {
				printf("%s[" ANSI_COLOR_RED "x" ANSI_COLOR_RESET "]: ACCESS DENIED: We tried to implement this interface but dbus denied our name request\n", interface.c_str());
				return;
			}

			if (ret != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) {
				printf("%s[" ANSI_COLOR_RED "x" ANSI_COLOR_RESET "]: Need to "
							 "implement this interface, but failed to obtain the interface name.\n",
							 interface.c_str());
			} else {

				std::vector<std::string> signalStrings;

				for (auto signal : mySignals) 
					signalStrings.push_back("type='signal',interface='"+signal.interface+"',member='"+signal.member+"'");

				for (auto& str : signalStrings) dbus.addMatch(str.c_str());

				printf("%s[" ANSI_COLOR_GREEN "✓" ANSI_COLOR_RESET "]: "
							 "Implementing interface\n", interface.c_str());
			}
		}
	}

	Inhibitor::ReturnObject DBusInhibitor::start() {
		while(1) try {
			dbus.readWriteDispatch(40);
			while (1) try {
				auto msg = dbus.popMessage();
				if (msg.isNull()) break;

				if (msg.type() == DBUS_MESSAGE_TYPE_METHOD_CALL) {
					for (auto& method : myMethods) {
						if ((method.member == std::string(msg.member())) && 
								(method.interface == std::string(msg.interface()))) {
							if (this->monitor) this->methodCalls.insert({msg.serial(), msg});
							else (this->*method.callback)(&msg, nullptr);

							break;
						}
					}
				}

				if (msg.type() == DBUS_MESSAGE_TYPE_METHOD_RETURN &&
						this->methodCalls.contains(msg.replySerial())) {
					auto callMsg = this->methodCalls.at(msg.replySerial());
					// TODO myMethods should probably be a map
					for (auto method : myMethods) {
						if (method.member == callMsg.member()) {
							(this->*method.callback)(&callMsg, &msg);
							this->methodCalls.erase(msg.replySerial());
							break;
						}
					}
				}

				if (msg.type() == DBUS_MESSAGE_TYPE_SIGNAL) {
					// TODO mySignals should probably be a map
					for (auto& signal : mySignals) {
						if ((signal.member == std::string(msg.member())) &&
								(signal.interface == std::string(msg.interface()))) {
							(this->*signal.callback)(&msg);
							break;
						}
					}
				}
			} catch (DBus::InvalidArgsError& e) {
				printf("Got invalid args for a method call, ignoring.\n");
				// TODO should we respond to the bad request in some way in this situation? Some apps
				// could hang waiting for a response.
			}
			this->poll();
			co_await std::suspend_always();
		}
		catch (std::exception &e) { printf("Unhandled exception: %s\n", e.what()); }
		catch (...) { std::terminate(); }
	}
}; // End namespace uinhibit
