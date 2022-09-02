#include "Inhibitor.hpp"
#include <cstdio>
#include <unistd.h>
#include "util.hpp"
#include <cstring>

#define THIS GnomeSessionManagerInhibitor
#define METHOD_CAST (void (DBusInhibitor::*)(DBus::Message* msg, DBus::Message* retmsg))
#define SIGNAL_CAST (void (DBusInhibitor::*)(DBus::Message* msg))
#define INTERFACE "org.gnome.SessionManager"
#define DBUS_INTERFACE "org.freedesktop.DBus"
#define INTROSPECT_INTERFACE "org.freedesktop.DBus.Introspectable"
#define PATH "/org/gnome/SessionManager"

using namespace uinhibit;

THIS::THIS(std::function<void(Inhibitor*, Inhibit)> inhibitCB, 
					 std::function<void(Inhibitor*, Inhibit)> unInhibitCB)
	: DBusInhibitor
		(inhibitCB, unInhibitCB, INTERFACE,
		 {
			 {INTERFACE, "Inhibit", METHOD_CAST &THIS::handleInhibitMsg, "*"},
			 {INTERFACE, "Uninhibit", METHOD_CAST &THIS::handleUnInhibitMsg, "*"},
			 {INTERFACE, "IsInhibited", METHOD_CAST &THIS::handleIsInhibitedMsg, "*"},
			 {INTROSPECT_INTERFACE, "Introspect", METHOD_CAST &THIS::handleIntrospect, INTERFACE}
		 },
		 {
			// TODO
			// {DBUS_INTERFACE, "NameOwnerChanged", SIGNAL_CAST &THIS::handleNameLostMsg}
		 }){}

void THIS::handleInhibitMsg(DBus::Message* msg, DBus::Message* retmsg) {
	const char* appname;
	uint32_t toplevel_xid;
	const char* reason;
	uint32_t flags;
	msg->getArgs(DBUS_TYPE_STRING, &appname,
							 DBUS_TYPE_UINT32, &toplevel_xid,
							 DBUS_TYPE_STRING, &reason, 
							 DBUS_TYPE_UINT32, &flags,
							 DBUS_TYPE_INVALID);

	// Read monitored response or send reply
	uint32_t cookie = ++this->lastCookie;
	if (retmsg != NULL) retmsg->getArgs(DBUS_TYPE_UINT32, &cookie, DBUS_TYPE_INVALID);
	else msg->newMethodReturn().appendArgs(DBUS_TYPE_UINT32, &cookie, DBUS_TYPE_INVALID)->send();

	// Create/register our new inhibit
	InhibitType t = gnomeType2us((GnomeInhibitType)flags);
	Inhibit in = {t, appname, reason, this->mkId(msg->sender(), cookie)};
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

void THIS::handleIsInhibitedMsg(DBus::Message* msg, DBus::Message* retmsg) {
	if (this->monitor) return;
	uint32_t flags = 0; msg->getArgs(DBUS_TYPE_UINT32, &flags, DBUS_TYPE_INVALID);

	bool ret = ((gnomeType2us((GnomeInhibitType)flags) & this->inhibited()) > 0);
	msg->newMethodReturn().appendArgs(DBUS_TYPE_BOOLEAN, &ret, DBUS_TYPE_INVALID);
}

void THIS::handleIntrospect(DBus::Message* msg, DBus::Message* retmsg) {	
	if (this->monitor || std::string(msg->destination()) != INTERFACE) return;

	const char* introspectXml = DBUS_INTROSPECT_1_0_XML_DOCTYPE_DECL_NODE
		"<node>"
		"  <interface name='" INTERFACE "'>"
		"    <method name='Inhibit'>"
		"      <arg type='s' name='app_id' direction='in' />"
		"      <arg type='u' name='toplevel_xid' direction='in' />"
		"      <arg type='s' name='reason' direction='in' />"
		"      <arg type='u' name='flags' direction='in' / >"
		"      <arg type='u' name='inhibit_cookie' direction='out' />"
    "    </method>"
		"    <method name='Uninhibit'>"
		"      <arg type='u' name='inhibit_cookie' direction='in' />"
		"    </method>"
		"    <method name='IsInhibited'>"
		"      <arg type='u' name='flags' direction='in' />"
		"      <arg type='b' name='is_inhibited' direction='out' />"
		"    </method>"
		// TODO
		"    <method name='GetInhibitors'>"
		"      <arg name='inhibitors' direction='out' type='ao' />"
		"    </method>"
		// TODO
		"    <signal name='InhibitorAdded'>"
		"      <arg name='id' type='o' />"
		"    </signal>"
		// TODO
		"    <signal name='InhibitorRemoved'>"
		"      <arg name='id' type='o' />"
		"    </signal>"
		// TODO
		"    <property name='InhibitedActions' type='u' access='read' />"
		"  </interface>"
		"</node>";

	msg->newMethodReturn().appendArgs(DBUS_TYPE_STRING,&introspectXml,DBUS_TYPE_INVALID)->send();
}

Inhibit THIS::doInhibit(InhibitRequest r) {
	if (us2gnomeType(r.type) == GnomeInhibitType::NONE)	
		throw uinhibit::InhibitRequestUnsupportedTypeException();

	uint32_t cookie = 0;
	if (this->monitor) {	
		const char* appname = r.appname.c_str();
		const char* reason = r.reason.c_str();
		uint32_t zero = 0;
		uint32_t flags = (uint32_t)us2gnomeType(r.type);

		try {
			auto replymsg = dbus
				.newMethodCall(INTERFACE, PATH, INTERFACE, "Inhibit")
				.appendArgs(DBUS_TYPE_STRING, &appname, 
										DBUS_TYPE_UINT32, &zero,
										DBUS_TYPE_STRING, &reason, 
										DBUS_TYPE_UINT32, &flags,
										DBUS_TYPE_INVALID)
				->sendAwait(500);
			if(replymsg.notNull()) replymsg.getArgs(DBUS_TYPE_UINT32, &cookie, DBUS_TYPE_INVALID);
		} catch (DBus::NoReplyError& e) {
			throw InhibitNoResponseException();
		}	
	} else {
		cookie = ++this->lastCookie;
	}

	Inhibit i = {gnomeType2us(us2gnomeType(r.type)), r.appname, r.reason, {}}; 
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

InhibitType THIS::gnomeType2us(GnomeInhibitType gnomeInhibitType) {
	InhibitType t = InhibitType::NONE;

	if ((gnomeInhibitType & GnomeInhibitType::SUSPEND) > 0) 
		t = static_cast<InhibitType>(t | InhibitType::SUSPEND);
	// TODO should SESSIONIDLE also inhibit suspend?
	if ((gnomeInhibitType & GnomeInhibitType::SESSIONIDLE) > 0) 
		t = static_cast<InhibitType>(t | InhibitType::SCREENSAVER);

	// TODO should we support others?

	return t;
}

THIS::GnomeInhibitType THIS::us2gnomeType(InhibitType us) {
	GnomeInhibitType ret = GnomeInhibitType::NONE;

	if ((us & InhibitType::SUSPEND) > 0)
		ret = static_cast<GnomeInhibitType>(ret | GnomeInhibitType::SUSPEND);
	if ((us & InhibitType::SCREENSAVER) > 0)
		ret = static_cast<GnomeInhibitType>(ret | GnomeInhibitType::SESSIONIDLE);

	return ret;
}
