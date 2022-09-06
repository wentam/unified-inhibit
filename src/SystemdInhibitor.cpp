#include "Inhibitor.hpp"
#include <cstdio>
#include <unistd.h>
#include "util.hpp"
#include <cstring>

namespace cpoll {
#include <poll.h>
}

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <thread>
#include <dirent.h>

#define THIS SystemdInhibitor
#define METHOD_CAST (void (DBusInhibitor::*)(DBus::Message* msg, DBus::Message* retmsg))
#define SIGNAL_CAST (void (DBusInhibitor::*)(DBus::Message* msg))
#define INTERFACE "org.freedesktop.login1.Manager"
#define DBUSNAME "org.freedesktop.login1"
#define DBUS_INTERFACE "org.freedesktop.DBus"
#define INTROSPECT_INTERFACE "org.freedesktop.DBus.Introspectable"
#define PATH "/org/freedesktop/login1"

using namespace uinhibit;

THIS::THIS(std::function<void(Inhibitor*, Inhibit)> inhibitCB, 
					 std::function<void(Inhibitor*, Inhibit)> unInhibitCB)
	: DBusInhibitor
		(inhibitCB, unInhibitCB, DBUSNAME, DBUS_BUS_SYSTEM,
		 {
			 {INTERFACE, "Inhibit", METHOD_CAST &THIS::handleInhibitMsg, "*"},
			 {INTROSPECT_INTERFACE, "Introspect", METHOD_CAST &THIS::handleIntrospect, INTERFACE}
		 },
		 {}){}

void THIS::handleIntrospect(DBus::Message* msg, DBus::Message* retmsg) {
	if (this->monitor || std::string(msg->destination()) != DBUSNAME) return;

	if (std::string(msg->path()) == "/")  {
		const char* introspectXml = DBUS_INTROSPECT_1_0_XML_DOCTYPE_DECL_NODE
			"<node name='/'>"
			"  <node name='org/freedesktop/login1' />"
			"</node>";

		msg->newMethodReturn().appendArgs(DBUS_TYPE_STRING,&introspectXml,DBUS_TYPE_INVALID)->send();
	}


	if (std::string(msg->path()) == "/org/freedesktop/login1")  {
		const char* introspectXml = DBUS_INTROSPECT_1_0_XML_DOCTYPE_DECL_NODE
			"<node name='org/freedesktop/login1'>"
			"  <interface name='" INTERFACE "'>"
			"    <method name='Inhibit'>"
			"      <arg name='what' type='s' direction='in'/>"
			"      <arg name='who' type='s' direction='in'/>"
			"      <arg name='why' type='s' direction='in'/>"
			"      <arg name='mode' type='s' direction='in'/>"
			"      <arg name='fd' type='h' direction='out'/>"
			"    </method>"
			"  </interface>"
			"</node>";

		msg->newMethodReturn().appendArgs(DBUS_TYPE_STRING,&introspectXml,DBUS_TYPE_INVALID)->send();
	}
}

void THIS::releaseThread(const char* path, Inhibit in) {
	// We don't have read access to wait for EOF, so we just need to wait until the file goes away
	while (1) {
		int r = access(path, F_OK);
		if (r != 0) {
			{
				std::unique_lock<std::mutex> lk(this->releaseQueueMutex);
				this->releaseQueue.push_back(in);
			}
			break;
		}
		usleep(100*1000);
	}
}

void THIS::releaseThreadOurFd(int32_t fd, std::string path, Inhibit in) {
	struct cpoll::pollfd pollfd = {
		.fd = fd,
		.events = POLLIN,
	};
	cpoll::poll(&pollfd, 1, -1);
	unlink(path.c_str());
	this->releaseQueue.push_back(in);
}

void THIS::handleInhibitMsg(DBus::Message* msg, DBus::Message* retmsg) {
	const char* what; const char* who; const char* why; const char* mode;
	msg->getArgs(DBUS_TYPE_STRING, &what,
							 DBUS_TYPE_STRING, &who,
							 DBUS_TYPE_STRING, &why,
							 DBUS_TYPE_STRING, &mode,
							 DBUS_TYPE_INVALID);

	if (retmsg != nullptr && this->monitor) {
		int32_t fd = -1;
		retmsg->getArgs(DBUS_TYPE_UNIX_FD, &fd, DBUS_TYPE_INVALID);

		Inhibit in = { this->systemdType2us(what), who, why, this->mkId(fd) };
		this->registerInhibit(in);

		char filePath[1024*10];
		std::string fdpath = "/proc/self/fd/";
		fdpath += std::to_string(fd);
		readlink(fdpath.c_str(), filePath, 1024*10);
		close(fd);

		std::thread t(&THIS::releaseThread, this, filePath, in);
		t.detach();
	} else {
		auto lockRef = this->newLockRef();

		msg->newMethodReturn().appendArgs(DBUS_TYPE_UNIX_FD, &lockRef.wfd, DBUS_TYPE_INVALID)->send();

		close(lockRef.wfd); // Close our end so we can watch for EOF on release

		Inhibit in = { this->systemdType2us(what), who, why, this->mkId(lockRef.wfd) };
		this->registerInhibit(in);

		std::thread t(&THIS::releaseThreadOurFd, this, lockRef.rfd, lockRef.file, in);
		t.detach();
	}
}

THIS::lockRef THIS::newLockRef() {
		mkdir("/tmp/uinhibitd-systemd-inhibits/", 0777);
		std::string file = "/tmp/uinhibitd-systemd-inhibits/inhibit" + std::to_string(++this->lastLockRef) + ".ref";
		unlink(file.c_str());

		int32_t fifo_r = mkfifo(file.c_str(), 0600);
		int32_t rfd = open(file.c_str(), O_RDONLY | O_NONBLOCK);
	 	int32_t wfd = open(file.c_str(), O_WRONLY | O_NONBLOCK);
		if (rfd < 0 || wfd < 0 || fifo_r < 0) throw std::runtime_error("Failed to set up fifo.");

		return { rfd, wfd, file };
}

void THIS::poll() {
	std::unique_lock<std::mutex> lk(this->releaseQueueMutex);

	for (auto r : this->releaseQueue) {
		this->registerUnInhibit(r.id);
	}

	this->releaseQueue.clear();
}

Inhibit THIS::doInhibit(InhibitRequest r) {
	if ((r.type & InhibitType::SUSPEND) == InhibitType::NONE)
		throw uinhibit::InhibitRequestUnsupportedTypeException();

	int32_t fd = -1;
	if (this->monitor) {
		// TODO us2systemdType
		const char* what = "idle";
		const char* who = r.appname.c_str();
		const char* why = r.reason.c_str();
		const char* mode = "block";
		try {
			auto replymsg = dbus
				.newMethodCall("org.freedesktop.login1", PATH, "org.freedesktop.login1.Manager", "Inhibit")
				.appendArgs(DBUS_TYPE_STRING, &what,
										DBUS_TYPE_STRING, &who,
										DBUS_TYPE_STRING, &why,
										DBUS_TYPE_STRING, &mode,
										DBUS_TYPE_INVALID)
				->sendAwait(500);
			if(replymsg.notNull()) replymsg.getArgs(DBUS_TYPE_UNIX_FD, &fd, DBUS_TYPE_INVALID);
		} catch (DBus::NoReplyError& e) {
			throw InhibitNoResponseException();
		}
	} else {
		auto lockRef = this->newLockRef();
		close(lockRef.wfd);
		fd = lockRef.rfd;
	}

	// TODO not hardcoded type
	Inhibit i = {InhibitType::SUSPEND, r.appname, r.reason, {}};
	i.id = this->mkId(fd);
	return i;
}

void THIS::doUnInhibit(InhibitID id) {
	int32_t fd = reinterpret_cast<_InhibitID*>(&id[0])->fd;
	if (this->monitor) {
		close(fd);
	} else {
		char filePath[1024*10];
		std::string fdpath = "/proc/self/fd/";
		fdpath += std::to_string(fd);
		if (readlink(fdpath.c_str(), filePath, 1024*10) > 0) {
			unlink(filePath);
		}
	}
}

InhibitID THIS::mkId(uint32_t fd) {
	_InhibitID idStruct = {this->instanceId, fd};
	auto ptr = reinterpret_cast<std::byte*>(&idStruct);
	InhibitID id(ptr, ptr+sizeof(idStruct));
	return id;
}

InhibitType THIS::systemdType2us(std::string what) {
	std::vector<std::string> types;
	types.emplace_back();

	for (auto c : what) {
		if (c == ':') types.emplace_back();
		else types.back().push_back(c);
	}

	InhibitType t = InhibitType::NONE;

	for (auto str : types) {
		if (str == "idle") t = static_cast<InhibitType>(t | InhibitType::SUSPEND);
		// TODO support others
	}

	return t;
}
