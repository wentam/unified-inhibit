#include "Inhibitor.hpp"
#include <cstdio>
#include <unistd.h>
#include "util.hpp"
#include <cstring>

#define THIS SimpleDBusInhibitor
#define METHOD_CAST (void (DBusInhibitor::*)(DBus::Message* msg, DBus::Message* retmsg))
#define SIGNAL_CAST (void (DBusInhibitor::*)(DBus::Message* msg))
#define DBUS_INTERFACE "org.freedesktop.DBus"
#define INTROSPECT_INTERFACE "org.freedesktop.DBus.Introspectable"

using namespace uinhibit;

template<typename T>
static std::vector<T> catVec(std::vector<T> a, std::vector<T> b) {
		std::vector c(a);
		c.insert(c.end(), b.begin(), b.end());
		return c;
};

THIS::THIS(std::function<void(Inhibitor*, Inhibit)> inhibitCB,
					 std::function<void(Inhibitor*, Inhibit)> unInhibitCB,
					 std::vector<DBusMethodCB> myMethods,
					 std::vector<DBusSignalCB> mySignals,
					 std::string interface,
					 std::string path,
					 InhibitType inhibitType,
					 std::string extraIntrospect)
	: DBusInhibitor
		(inhibitCB, unInhibitCB, interface, DBUS_BUS_SESSION,
		 catVec<DBusMethodCB>(
		 {
			 {interface, "Inhibit", METHOD_CAST &THIS::handleInhibitMsg, "*"},
			 {interface, "UnInhibit", METHOD_CAST &THIS::handleUnInhibitMsg, "*"},
			 {INTROSPECT_INTERFACE, "Introspect", METHOD_CAST &THIS::handleIntrospect, interface}
		 }, myMethods),
		 catVec<DBusSignalCB>(
		 {
			 {DBUS_INTERFACE, "NameOwnerChanged", SIGNAL_CAST &THIS::handleNameLostMsg}
		 }, mySignals)),
		interface(interface),
		path(path),
		inhibitType(inhibitType),
		extraIntrospect(extraIntrospect)
{
	// Remove any leading / from path	
	if (this->path.size() > 0 && this->path.front() == '/') this->path.erase(0,1);
}

void THIS::handleInhibitMsg(DBus::Message* msg, DBus::Message* retmsg) {
	const char* appname; const char* reason;
	msg->getArgs(DBUS_TYPE_STRING, &appname, DBUS_TYPE_STRING, &reason, DBUS_TYPE_INVALID);

	// Read monitored response or send reply
	this->lastCookie++;
	if (this->lastCookie == 0) this->lastCookie = 1;

	uint32_t cookie = this->lastCookie;
	if (retmsg != NULL) retmsg->getArgs(DBUS_TYPE_UINT32, &cookie, DBUS_TYPE_INVALID);
	else msg->newMethodReturn().appendArgs(DBUS_TYPE_UINT32, &cookie, DBUS_TYPE_INVALID)->send();

	// Create/register our new inhibit
	Inhibit in = {
		this->inhibitType,
		appname,
		reason,
		this->mkId(msg->sender(), cookie),
		(uint64_t)time(NULL)
	};
	this->registerInhibit(in);

	// Track inhibit owner to allow unInhibit on crash
	this->inhibitOwners[msg->sender()].push_back(in.id);
}

void THIS::handleUnInhibitMsg(DBus::Message* msg, DBus::Message* retmsg) {
	uint32_t cookie = 0; msg->getArgs(DBUS_TYPE_UINT32, &cookie, DBUS_TYPE_INVALID);

	auto id = this->mkId(msg->sender(), cookie);
	this->registerUnInhibit(id);

	// TODO I don't think this is actually cleaning up properly
	std::remove_if(inhibitOwners[msg->sender()].begin(), inhibitOwners[msg->sender()].end(),
								 [&id](InhibitID eid) { return id == eid; });
}

void THIS::handleNameLostMsg(DBus::Message* msg) {
	const char* name; msg->getArgs(DBUS_TYPE_STRING, &name, DBUS_TYPE_INVALID);
	for (auto id : this->inhibitOwners[name]) this->registerUnInhibit(id);
	this->inhibitOwners[name].clear();
}

void THIS::handleIntrospect(DBus::Message* msg, DBus::Message* retmsg) {
	if (this->monitor || std::string(msg->destination()) != this->interface) return;

	if (std::string(msg->path()) == "/")  {
		std::string xml = DBUS_INTROSPECT_1_0_XML_DOCTYPE_DECL_NODE
			"<node name='/'>"
			"  <node name='"+this->path+"' />"
			"</node>";

		const char* introspectXml	= xml.c_str();
		msg->newMethodReturn().appendArgs(DBUS_TYPE_STRING,&introspectXml,DBUS_TYPE_INVALID)->send();
	} 

	if (std::string(msg->path()) == "/"+this->path)  {
		std::string xml = DBUS_INTROSPECT_1_0_XML_DOCTYPE_DECL_NODE
			"<node name='"+this->path+"'>"
			"  <interface name=\""+this->interface+"\">"
			"    <method name='Inhibit'>"
			"      <arg name='application_name' type='s' direction='in'/>"
			"      <arg name='reason_for_inhibit' type='s' direction='in'/>"
			"      <arg name='cookie' type='u' direction='out'/>"
			"    </method>"
			"    <method name='UnInhibit'>"
			"      <arg name='cookie' type='u' direction='in'/>"
			"    </method>"
			+this->extraIntrospect+
			"  </interface>"
			"</node>";

		const char* introspectXml	= xml.c_str();
		msg->newMethodReturn().appendArgs(DBUS_TYPE_STRING,&introspectXml,DBUS_TYPE_INVALID)->send();
	}
}

Inhibit THIS::doInhibit(InhibitRequest r) {
	if ((r.type & this->inhibitType) == InhibitType::NONE)
		throw uinhibit::InhibitRequestUnsupportedTypeException();
	uint32_t cookie = 0;
	if (this->monitor) {
		const char* appname = r.appname.c_str();
		const char* reason = r.reason.c_str();

		try {
			auto replymsg = callDbus->newMethodCall(this->interface.c_str(), ("/"+this->path).c_str(), this->interface.c_str(), "Inhibit")
				.appendArgs(DBUS_TYPE_STRING, &appname, 
										DBUS_TYPE_STRING, &reason, DBUS_TYPE_INVALID)
				->sendAwait(500);
			if(replymsg.notNull()) replymsg.getArgs(DBUS_TYPE_UINT32, &cookie, DBUS_TYPE_INVALID);
		} catch (DBus::NoReplyError& e) {
			throw InhibitNoResponseException();
		}	
	} else {
		this->lastCookie++;
		if (this->lastCookie == 0) this->lastCookie = 1;

		cookie = this->lastCookie;
	}

	Inhibit i = {this->inhibitType, r.appname, r.reason, {}, (uint64_t)time(NULL)}; 
	i.id = this->mkId("us", cookie);
	return i;
}

void THIS::doUnInhibit(InhibitID id) {
	auto idStruct = reinterpret_cast<_InhibitID*>(&id[0]);
	if (this->monitor) {
		callDbus->newMethodCall(this->interface.c_str(), ("/"+this->path).c_str(), this->interface.c_str(), "UnInhibit")
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
