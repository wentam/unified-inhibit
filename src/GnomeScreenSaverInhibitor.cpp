#include "Inhibitor.hpp"
#include <cstdio>
#include <unistd.h>
#include "util.hpp"
#include <cstring>

#define THIS GnomeScreenSaverInhibitor
#define METHOD_CAST (void (DBusInhibitor::*)(DBus::Message* msg, DBus::Message* retmsg))
#define SIGNAL_CAST (void (DBusInhibitor::*)(DBus::Message* msg))
#define INTERFACE "org.gnome.ScreenSaver"
#define DBUS_INTERFACE "org.freedesktop.DBus"
#define INTROSPECT_INTERFACE "org.freedesktop.DBus.Introspectable"

using namespace uinhibit;

THIS::THIS(std::function<void(Inhibitor*, Inhibit)> inhibitCB, 
					 std::function<void(Inhibitor*, Inhibit)> unInhibitCB)
	: SimpleDBusInhibitor
		(inhibitCB, unInhibitCB,
		 {
			 {INTERFACE, "SimulateUserActivity", METHOD_CAST &THIS::handleSimActivityMsg, "*"},
		 },
		 {},
		 INTERFACE,
		 "/org/gnome/ScreenSaver",
		 InhibitType::SCREENSAVER,	
		 "<method name='SimulateUserActivity' />")
{}

void THIS::handleSimActivityMsg(DBus::Message* msg, DBus::Message* retmsg) {
	// We treat this as an inhibit that expires in 5min
	// We use a cookie of 0 to represent sim activity inhibits.
	Inhibit i = {this->inhibitType, msg->sender(), "SimulateUserActivity", this->mkId(msg->sender(), 0), time(NULL)};

	// Clear any existing simActivity inhibits from this sender	
	std::vector<InhibitID> eraseIDs;
	for (auto& [id, in] : this->activeInhibits) {
		auto idc = id;
		auto idStruct = reinterpret_cast<const _InhibitID*>(&id[0]);
		if (idStruct->cookie == 0 && idStruct->sender == msg->sender()) eraseIDs.push_back(idc);
	}
	for (auto& id : eraseIDs) this->registerUnInhibit(id);
	this->inhibitOwners[std::string(msg->sender())].clear();

	// Register new inhibit
	this->registerInhibit(i);
	this->inhibitOwners[std::string(msg->sender())].push_back(i.id);

	if (!this->monitor) msg->newMethodReturn().send();
}

void THIS::poll() {
	// Unregister any expired inhibits	
	std::vector<InhibitID> eraseIDs;
	for (auto& [id, in] : this->activeInhibits) {
		auto idc = id;
		auto idStruct = reinterpret_cast<const _InhibitID*>(&id[0]);
		if (idStruct->cookie == 0 && in.created <= (time(NULL)-(60*5))) {
			this->inhibitOwners[idStruct->sender].clear();
			eraseIDs.push_back(idc);
		}
	}

	for (auto& id : eraseIDs) this->registerUnInhibit(id);
}
