#include "Inhibitor.hpp"
#include <cstdio>
#include <unistd.h>
#include "util.hpp"
#include <cstring>

#define THIS FreedesktopScreenSaverInhibitor
#define METHOD_CAST (void (DBusInhibitor::*)(DBus::Message* msg, DBus::Message* retmsg))
#define SIGNAL_CAST (void (DBusInhibitor::*)(DBus::Message* msg))
#define INTERFACE "org.freedesktop.ScreenSaver"
#define DBUS_INTERFACE "org.freedesktop.DBus"
#define INTROSPECT_INTERFACE "org.freedesktop.DBus.Introspectable"
#define PATH "/ScreenSaver"

using namespace uinhibit;

THIS::THIS(std::function<void(Inhibitor*, Inhibit)> inhibitCB,
					 std::function<void(Inhibitor*, Inhibit)> unInhibitCB)
	: DBusInhibitor
		(inhibitCB, unInhibitCB, INTERFACE,
		 {
			 {INTERFACE, "Inhibit", METHOD_CAST &THIS::handleInhibitMsg, "*"},
			 {INTERFACE, "UnInhibit", METHOD_CAST &THIS::handleUnInhibitMsg, "*"},
			 {INTROSPECT_INTERFACE, "Introspect", METHOD_CAST &THIS::handleIntrospect, INTERFACE}
		 },
		 {
			 {DBUS_INTERFACE, "NameOwnerChanged", SIGNAL_CAST &THIS::handleNameLostMsg}
		 }){}

void THIS::handleInhibitMsg(DBus::Message* msg, DBus::Message* retmsg) {
	const char* appname; const char* reason;
	msg->getArgs(DBUS_TYPE_STRING, &appname, DBUS_TYPE_STRING, &reason, DBUS_TYPE_INVALID);

	// Read monitored response or send reply
	uint32_t cookie = ++this->lastCookie;
	if (retmsg != NULL) retmsg->getArgs(DBUS_TYPE_UINT32, &cookie, DBUS_TYPE_INVALID);
	else msg->newMethodReturn().appendArgs(DBUS_TYPE_UINT32, &cookie, DBUS_TYPE_INVALID)->send();

	// Create/register our new inhibit
	Inhibit in = {InhibitType::SCREENSAVER, appname, reason, this->mkId(msg->sender(), cookie)};
	this->registerInhibit(in);

	// Track inhibit owner to allow unInhibit on crash
	this->inhibitOwners[msg->sender()].push_back(in.id);
}

void THIS::handleUnInhibitMsg(DBus::Message* msg, DBus::Message* retmsg) {
	uint32_t cookie = 0; msg->getArgs(DBUS_TYPE_UINT32, &cookie, DBUS_TYPE_INVALID);

	auto id = this->mkId(msg->sender(), cookie);
	this->registerUnInhibit(id);

	std::remove_if(inhibitOwners[msg->sender()].begin(), inhibitOwners[msg->sender()].end(),
								 [&id](InhibitID eid) { return id == eid; });
}

void THIS::handleNameLostMsg(DBus::Message* msg) {
	const char* name; msg->getArgs(DBUS_TYPE_STRING, &name, DBUS_TYPE_INVALID);
	for (auto id : this->inhibitOwners[name]) this->registerUnInhibit(id);
	this->inhibitOwners[name].clear();
}

void THIS::handleIntrospect(DBus::Message* msg, DBus::Message* retmsg) {
	if (this->monitor || std::string(msg->destination()) != INTERFACE) return;

	if (std::string(msg->path()) == "/")  {
		const char* introspectXml = DBUS_INTROSPECT_1_0_XML_DOCTYPE_DECL_NODE
			"<node name='/'>"
			"  <node name='ScreenSaver' />"
			"</node>";

		msg->newMethodReturn().appendArgs(DBUS_TYPE_STRING,&introspectXml,DBUS_TYPE_INVALID)->send();
	} 

	if (std::string(msg->path()) == "/ScreenSaver")  {
		const char* introspectXml = DBUS_INTROSPECT_1_0_XML_DOCTYPE_DECL_NODE
			"<node name='ScreenSaver'>"
			"  <interface name=\"org.freedesktop.ScreenSaver\">"
			"    <method name='Inhibit'>"
			"      <arg name='application_name' type='s' direction='in'/>"
			"      <arg name='reason_for_inhibit' type='s' direction='in'/>"
			"      <arg name='cookie' type='u' direction='out'/>"
			"    </method>"
			"    <method name='UnInhibit'>"
			"      <arg name='cookie' type='u' direction='in'/>"
			"    </method>"
			"  </interface>"
			"</node>";

		msg->newMethodReturn().appendArgs(DBUS_TYPE_STRING,&introspectXml,DBUS_TYPE_INVALID)->send();
	}
}

Inhibit THIS::doInhibit(InhibitRequest r) {
	if ((r.type & InhibitType::SCREENSAVER) == InhibitType::NONE)
		throw uinhibit::InhibitRequestUnsupportedTypeException();
	uint32_t cookie = 0;
	if (this->monitor) {
		const char* appname = r.appname.c_str();
		const char* reason = r.reason.c_str();

		try {
			auto replymsg = dbus.newMethodCall(INTERFACE, PATH, INTERFACE, "Inhibit")
				.appendArgs(DBUS_TYPE_STRING, &appname, 
										DBUS_TYPE_STRING, &reason, DBUS_TYPE_INVALID)
				->sendAwait(500);
			if(replymsg.notNull()) replymsg.getArgs(DBUS_TYPE_UINT32, &cookie, DBUS_TYPE_INVALID);
		} catch (DBus::NoReplyError& e) {
			throw InhibitNoResponseException();
		}	
	} else {
		cookie = ++this->lastCookie;
	}

	Inhibit i = {InhibitType::SCREENSAVER, r.appname, r.reason, {}}; 
	i.id = this->mkId("us", cookie);
	return i;
}

void THIS::doUnInhibit(InhibitID id) {
	auto idStruct = reinterpret_cast<_InhibitID*>(&id[0]);
	if (this->monitor) {
		dbus.newMethodCall(INTERFACE, PATH, INTERFACE, "UnInhibit")
			  .appendArgs(DBUS_TYPE_UINT32, &(idStruct->cookie), DBUS_TYPE_INVALID)
			  ->send();
	}
}

InhibitID THIS::mkId(std::string sender, uint32_t cookie) {
	_InhibitID idStruct = {this->instanceId, {}, cookie};

	uint32_t size = (sender.size()>sizeof(idStruct.sender)) ? sizeof(idStruct.sender) : sender.size();
	strncpy(idStruct.sender, sender.c_str(), size);
	if(size>0) idStruct.sender[size-1]=0; // NULL terminate

	auto ptr = reinterpret_cast<std::byte*>(&idStruct);
	InhibitID id(ptr, ptr+sizeof(idStruct));
	return id;
};
