#include <cstdio>
#include "Inhibitor.hpp"
#include <thread>
#include <mutex>

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

// TODO TMP
const char* func1;
const char* func2;

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
	}
}

static void inhibitCB(Inhibitor* inhibitor, Inhibit inhibit) {
	printf("Inhibit event type=%d appname='%s' reason='%s'\n", inhibit.type, inhibit.appname.c_str(), inhibit.reason.c_str());
	if (func1!=NULL)system(func1);
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
	if (func2!=NULL)system(func2);

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

int main([[maybe_unused]]int argc, [[maybe_unused]]char *argv[]) {
	func1 = argv[1];
	func2 = argv[2];

	uinhibit::FreedesktopScreenSaverInhibitor i1(inhibitCB, unInhibitCB); inhibitors.push_back(&i1);
	uinhibit::FreedesktopPowerManagerInhibitor i2(inhibitCB, unInhibitCB); inhibitors.push_back(&i2);
	uinhibit::GnomeSessionManagerInhibitor i3(inhibitCB, unInhibitCB); inhibitors.push_back(&i3);
	
	std::vector<Inhibitor::ReturnObject> ros;
	for (auto& inhibitor : inhibitors) ros.push_back(inhibitor->start());
	while(1) for (auto& r : ros) {r.handle.resume(); fflush(stdout); }
}
