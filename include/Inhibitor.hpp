#pragma once
#include <functional>
#include <dbus/dbus.h>
#include <map>
#include <cstdint>
#include <string>
#include <memory>
#include <charconv>
#include <cstddef>
#include "DBus.hpp"
#include <atomic>
#include <mutex>
#include <coroutine>

namespace uinhibit {
	class InhibitRequestUnsupportedTypeException : std::exception {};
	class InhibitNoResponseException : std::exception {};

	template<typename T>
	class LockWrap {
		public:
			LockWrap() {};
			LockWrap(T startValue) { data = startValue; };
			//T* get() { std::unique_lock<std::mutex> lk(lock); return &data; };
		
			

			//T* operator()() { return this->get(); };
			void operator=(T& a) { std::unique_lock<std::mutex> lk(lock); data = a; };
		private:
			T data;
			std::mutex lock;
	};

	// Unique ID for an Inhibit. Data within is Inhibitor-specific.
	// Unique among all Inhibitors.
	typedef std::vector<std::byte> InhibitID; 

	enum InhibitType {
		NONE = 0,
		SCREENSAVER = 0b00000001,
		SUSPEND     = 0b00000010,
	};

	struct InhibitRequest {
		InhibitType type = InhibitType::NONE;
		std::string appname = "";
		std::string reason = "";
	};

	struct Inhibit : InhibitRequest {
		InhibitID id;
	};

	class Inhibitor {
		public:
			Inhibitor(std::function<void(Inhibitor*,Inhibit)> inhibitCB,
								std::function<void(Inhibitor*,Inhibit)> unInhibitCB);

			struct ReturnObject {
				ReturnObject(std::coroutine_handle<> h) : handle{h} {}
				std::coroutine_handle<> handle;

				struct promise_type {
					ReturnObject get_return_object() {
						return {std::coroutine_handle<promise_type>::from_promise(*this)};
					}
					std::suspend_never initial_suspend() { return {}; }
					std::suspend_never final_suspend() noexcept { return {}; }
					void unhandled_exception() { }

					auto getHandle(){
						return std::coroutine_handle<promise_type>::from_promise(*this);
					}
				};
			};

			// Start operating/listening for inhibit events
			virtual ReturnObject start() = 0;

			// Bitflags of all currently inhibited types
			InhibitType inhibited();

			// Call do(Un)Inhibit for subclasses (TODO) and adds/removes activeInhibits
			// Also calls handle(un)InhibitEvent handleInhibitStateChanged()
			// Must be awaiting for these to function
			Inhibit inhibit(InhibitRequest);
			void unInhibit(InhibitID);

			uint64_t instanceId = 0; // Uniquely identifies this Inhibitor instance
		  std::map<InhibitID, Inhibit> activeInhibits;
		protected:
			// Implementation of (un)inhibit action. Do not register the inhibit, as this was a
			// user-requested action and they don't need to be called back about it (this could result in
			// infinite loops).
			//
			// Throw InhibitRequestUnsupportedTypeException if you don't support this type.
			virtual Inhibit doInhibit(InhibitRequest) = 0;
			virtual void doUnInhibit(InhibitID) = 0;

			// Add/remove from activeInhibits and call the inhibit/uninhibit callbacks
			void registerInhibit(Inhibit& i);
			void registerUnInhibit(InhibitID&);

			// Called any time register(un)Inhibit or (un)inhibit is called	
			//
			// ie. This event may have been generated by your implementation, or the user of this 
			// Inhibitor asked for the inhibit
			//
			// Useful to implement Inhibitors that actively 'push' their state on these events,
			// such as the org.freedesktop.PowerManager.HasInhibitChanged signal
			virtual void handleInhibitEvent() = 0;
			virtual void handleUnInhibitEvent() = 0;
			virtual void handleInhibitStateChanged(InhibitType inhibited) = 0;

			std::function<void(Inhibitor*, Inhibit)> inhibitCB;
			std::function<void(Inhibitor*, Inhibit)> unInhibitCB;
		private:
			void callEvent(bool isInhibit);
			InhibitType lastInhibitState = InhibitType::NONE;
	};

	class DBusInhibitor : public Inhibitor {
		public:
			struct DBusMethodCB {
				std::string interface;
				std::string member;
				void (DBusInhibitor::*callback)(DBus::Message* msg, DBus::Message* retmsg);
				std::string destination;
			};

			struct DBusSignalCB {
				std::string interface;
				std::string member;
				void (DBusInhibitor::*callback)(DBus::Message* msg);
			};

			DBusInhibitor(std::function<void(Inhibitor*, Inhibit)> inhibitCB,
										std::function<void(Inhibitor*, Inhibit)> unInhibitCB,
										std::string interface,
										std::vector<DBusMethodCB> myMethods,
										std::vector<DBusSignalCB> mySignals);

			ReturnObject start() override;
		protected:
			DBus dbus;
			bool monitor;

			std::vector<DBusMethodCB> myMethods;
			std::vector<DBusSignalCB> mySignals;

			std::map<uint32_t, DBus::Message> methodCalls; // serial, message
			std::string interface;
	};

	// TODO exception if a suspend inhibit is requested
	class FreedesktopScreenSaverInhibitor : public DBusInhibitor {
		public:
			FreedesktopScreenSaverInhibitor(std::function<void(Inhibitor*, Inhibit)> inhibitCB,
																			std::function<void(Inhibitor*, Inhibit)> unInhibitCB);
		protected:
			struct _InhibitID {
				uint64_t instanceID;
				char sender[1024];
				uint32_t cookie;
			};	

			void handleInhibitMsg(DBus::Message* msg, DBus::Message* retmsg);
			void handleUnInhibitMsg(DBus::Message* msg, DBus::Message* retmsg);
			void handleNameLostMsg(DBus::Message* msg);
			void handleIntrospect(DBus::Message* msg, DBus::Message* retmsg);
			InhibitID mkId(std::string sender, uint32_t cookie);
			std::map<std::string, std::vector<InhibitID>> inhibitOwners; // sender, {ids}
			uint32_t lastCookie = 0;

			void handleInhibitEvent() {};
			void handleUnInhibitEvent() {};
			void handleInhibitStateChanged(InhibitType inhibited) {};

			Inhibit doInhibit(InhibitRequest r) override;
			void doUnInhibit(InhibitID id) override;
	};



	class FreedesktopPowerManagerInhibitor : public DBusInhibitor {
		public:
			FreedesktopPowerManagerInhibitor(std::function<void(Inhibitor*, Inhibit)> inhibitCB,
																			std::function<void(Inhibitor*, Inhibit)> unInhibitCB);
		protected:
			struct _InhibitID {
				uint64_t instanceID;
				char sender[1024];
				uint32_t cookie;
			};	

			void handleInhibitMsg(DBus::Message* msg, DBus::Message* retmsg);
			void handleUnInhibitMsg(DBus::Message* msg, DBus::Message* retmsg);
			void handleHasInhibitMsg(DBus::Message* msg, DBus::Message* retmsg);
			void handleNameLostMsg(DBus::Message* msg);
			void handleIntrospect(DBus::Message* msg, DBus::Message* retmsg);
			InhibitID mkId(std::string sender, uint32_t cookie);
			std::map<std::string, std::vector<InhibitID>> inhibitOwners; // sender, {ids}
			uint32_t lastCookie = 0;

			void handleInhibitEvent() {};
			void handleUnInhibitEvent() {};
			Inhibit doInhibit(InhibitRequest r) override;
			void doUnInhibit(InhibitID id) override;


			void handleInhibitStateChanged(InhibitType inhibited);
	};

	class GnomeSessionManagerInhibitor : public DBusInhibitor {
			struct _InhibitID {
				uint64_t instanceID;
				char sender[1024];
				uint32_t cookie;
			};	

		public:
			GnomeSessionManagerInhibitor(std::function<void(Inhibitor*, Inhibit)> inhibitCB,
																				std::function<void(Inhibitor*, Inhibit)> unInhibitCB);
		protected:
			enum GnomeInhibitType {
				NONE = 0,
				LOGOUT = 1,
				USERSWITCH = 2,
				SUSPEND = 4,
				SESSIONIDLE = 8,
				AUTOMOUNT = 16
			};

			void handleInhibitMsg(DBus::Message* msg, DBus::Message* retmsg);
			void handleUnInhibitMsg(DBus::Message* msg, DBus::Message* retmsg);
			void handleIsInhibitedMsg(DBus::Message* msg, DBus::Message* retmsg);
			void handleIntrospect(DBus::Message* msg, DBus::Message* retmsg);
			std::map<std::string, std::vector<InhibitID>> inhibitOwners; // sender, {ids}
			uint32_t lastCookie = 0;

			void handleInhibitEvent() {};
			void handleUnInhibitEvent() {};
			Inhibit doInhibit(InhibitRequest r) override;
			void doUnInhibit(InhibitID id) override;
			void handleInhibitStateChanged(InhibitType inhibited) override {};

			InhibitType gnomeType2us(GnomeInhibitType t);
      GnomeInhibitType us2gnomeType(InhibitType us);
			InhibitID mkId(std::string sender, uint32_t cookie);
	};

	class GnomeScreenSaverInhibitor : public Inhibitor {
		// TODO
	};

	class SystemdInhibitor : public Inhibitor {
		// TODO
	};

	class LinuxInhibitor : public Inhibitor {
		// TODO
	};

	class CinnamonScreenSaverInhibitor : public Inhibitor {
		// TODO
	};

	// wayland inhibit
	// xscreensaver inhibit?
	// XFCE inhibit
	// freedesktop portal
}
