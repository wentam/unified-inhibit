#include <cstdio>
#include "Inhibitor.hpp"
#include <thread>
#include <mutex>
#include "util.hpp"

// TODO
// * command line argument to disable specific inhibitors
// * command line argument to force specific D-Bus inhibitors to monitoring mode (to ensure some other
//   application can implement/restart freely)
// * command line argument to disable takeover for specific (or all) inhibitors (monitoring
//   inhibitors won't try to implement if existing implementer disappears)
// * if another application tries to take over our D-Bus interface implementation,
//   step back to monitoring mode?
// * when logging inhibit state change and there are active inhibits, list the inhibitors 
//   responsible
// * log levels
// * CLI argument parsing
// * man page
// * optional ability to write the current inhibit state to a file

// A good set of tests for each inhibitor:
// * When something else implements, we start monitoring
// * When something else dosen't implement, we do
// * inhibits/uninhibits in both monitor/implement mode
// * application crash with kill -9 releases the inhibit in both monitor and implement mode
// * when in monitor mode and the existing implementation dissappears, we jump in

using namespace uinhibit;

const char* func1 = NULL;
const char* func2 = NULL;

static std::vector<Inhibitor*> inhibitors;
static InhibitType lastInhibitType = InhibitType::NONE;
static std::map<InhibitID, std::vector<std::pair<Inhibitor*, InhibitID>>> releasePlan;

static InhibitType inhibited() {
	InhibitType i = InhibitType::NONE;
	for (auto& in : inhibitors) {
		for (auto& [in, inhibit] : in->activeInhibits) i = static_cast<InhibitType>(i | inhibit.type);
	}
	return i;
}

static void printInhibited() {
	auto i = inhibited();
	if (lastInhibitType != i) {
		printf("Inhibit state changed to: screensaver=%d suspend=%d\n",
					 ((i & InhibitType::SCREENSAVER) > 0),
					 ((i & InhibitType::SUSPEND) > 0));
		lastInhibitType = i;
		if (i != InhibitType::NONE && func1!=NULL) { printf("Running inhibit command: %s\n", func1); system(func1); };
		if (i == InhibitType::NONE && func2!=NULL) { printf("Running uninhibit command: %s\n", func2);system(func2); };
	}
}

static void inhibitCB(Inhibitor* inhibitor, Inhibit inhibit) {
	printf("Inhibit event type=%d appname='%s' reason='%s'\n", inhibit.type, inhibit.appname.c_str(), inhibit.reason.c_str());
	// Forward to all active inhibitors (other than the originator)
	try {
		for (auto& ai : inhibitors) {
			InhibitRequest r = {inhibit.type, inhibit.appname, inhibit.reason};
			if (ai->instanceId != inhibitor->instanceId) {
				auto newInhibit = ai->inhibit(r);
				releasePlan[inhibit.id].push_back({ai, newInhibit.id});
			}
		}
	}
	catch (uinhibit::InhibitRequestUnsupportedTypeException& e) {}
	catch (uinhibit::InhibitNoResponseException& e) {}

	// Output our global inhibit state to STDOUT if it's changed
	printInhibited();
}

static void unInhibitCB(Inhibitor* inhibitor, Inhibit inhibit) {
	printf("UnInhibit event type=%d appname='%s'\n", inhibit.type, inhibit.appname.c_str());
	// Forward to all active inhibitors (other than the originator)
	try {
		if (releasePlan.contains(inhibit.id)) {
			for (auto& release : releasePlan.at(inhibit.id)) {
				release.first->unInhibit(release.second);
			}
			releasePlan.erase(inhibit.id);
		}
	}
	catch (uinhibit::InhibitRequestUnsupportedTypeException& e) {}
	catch (uinhibit::InhibitNoResponseException& e) {}
	// Output our global inhibit state to STDOUT if it's changed
	printInhibited();
}

static std::string cleanDisplayEnv(std::string display) {
	std::string cleanDisplay;
	uint32_t i = 0;
	for (auto c : display) { 
		if (isdigit(c) || (c == ':' && i == 0)) cleanDisplay.push_back(c); 
		i++;
	}

	return cleanDisplay;
}

int main([[maybe_unused]]int argc, [[maybe_unused]]char *argv[]) {
	// Security: We might be setuid. Clean up environment.
	const char* sessionBusEnv = getenv("DBUS_SESSION_BUS_ADDRESS");
	const char* display = getenv("DISPLAY");
	std::string cleanDisplay = "";
	if (display != NULL) cleanDisplay = cleanDisplayEnv(display);

	if (clearenv() != 0) { printf("Failed to clear environment\n"); exit(1); }

	setenv("DISPLAY", cleanDisplay.c_str(), 0);
	setenv("DBUS_SESSION_BUS_ADDRESS", sessionBusEnv, 0);

	// D-Bus tries to prevent usage of setuid binaries by checking if euid != ruid.
	// We need setuid, but we can just set both euid *and* ruid to root to get our root stuff done,
	// then switch to our user for the rest.
	//
	// Can't go back to root after that, but this covers our needs.
	auto ruid = getuid();

	// D-Bus inhibitors that need the system bus should be constructed as root
	// ---------------------------- SETUID ROOT --------------------------
	if (setresuid(0,0,0) != 0) {
		// All good, we just might get access denied depending on user config.
	}

	// Security note: we're root, always ensure these constructors are safe and don't touch raw
	// user input in any way. Our user input may be unprivileged.
	uinhibit::SystemdInhibitor i4(inhibitCB, unInhibitCB); inhibitors.push_back(&i4);	

	// D-Bus inhibitors that need the session bus should be constructed as the user
	if (setresuid(ruid,ruid,ruid) != 0) { printf("Failed to drop privileges\n"); exit(1); }
	if (getuid() != ruid) exit(1); // Should never happen
	// ---------------------------- SETUID SAFE --------------------------

	// Parse args/user input
	if (argc > 2) {
		func1 = argv[1];
		func2 = argv[2];
	}

	uinhibit::FreedesktopScreenSaverInhibitor i1(inhibitCB, unInhibitCB); inhibitors.push_back(&i1);
	uinhibit::FreedesktopPowerManagerInhibitor i2(inhibitCB, unInhibitCB); inhibitors.push_back(&i2);
	uinhibit::GnomeSessionManagerInhibitor i3(inhibitCB, unInhibitCB); inhibitors.push_back(&i3);

	// Run inhibitors
	// Security note: it is critical we have dropped privileges before this point, as we will be
	// running user-inputted commands.
	std::vector<Inhibitor::ReturnObject> ros;
	for (auto& inhibitor : inhibitors) ros.push_back(inhibitor->start());
	while(1) for (auto& r : ros) {r.handle.resume(); fflush(stdout); }
}
