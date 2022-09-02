#pragma once
#include <dbus/dbus.h>
#include <string>
#include <memory>
#include <vector>
#include <functional>

// Thin libdbus wrapper
//
// Adds exceptions, minor conveniences, but still operates at the low-level of libdbus to maintain
// full flexibility
//
// TODO: this could be in a separate repo/project
class DBus {
	public:
		// Made available so you can easily catch all dbus exceptions with "catch (DBus::Exception)"
		class Exception : public std::exception {};

		class Error : public Exception {
			public:
				Error(const char* message) : message(message) {};
				const char* what() const throw() override { return message; };
				const char* message;
		};

    // DBUS_ERROR_FAILED 
		class FailedError : public Error { using Error::Error; }; 
    // DBUS_ERROR_NO_MEMORY
		class NoMemoryError : public Error { using Error::Error; }; 
    // DBUS_ERROR_SERVICE_UNKNOWN
		class ServiceUnknownError : public Error { using Error::Error; }; 
    // DBUS_ERROR_NAME_HAS_NO_OWNER
		class NameHasNoOwnerError : public Error { using Error::Error; }; 
    // DBUS_ERROR_NO_REPLY
		class NoReplyError : public Error { using Error::Error; }; 
    // DBUS_ERROR_IO_ERROR
		class IoErrorError : public Error { using Error::Error; }; 
    // DBUS_ERROR_BAD_ADDRESS
		class BadAddressError : public Error { using Error::Error; }; 
    // DBUS_ERROR_NOT_SUPPORTED
		class NotSupportedError : public Error { using Error::Error; }; 
    // DBUS_ERROR_LIMITS_EXCEEDED
		class LimitsExceededError : public Error { using Error::Error; }; 
    // DBUS_ERROR_ACCESS_DENIED
		class AccessDeniedError : public Error { using Error::Error; }; 
    // DBUS_ERROR_AUTH_FAILED
		class AuthFailedError : public Error { using Error::Error; }; 
    // DBUS_ERROR_NO_SERVER
		class NoServerError : public Error { using Error::Error; }; 
    // DBUS_ERROR_TIMEOUT
		class TimeoutError : public Error { using Error::Error; }; 
    // DBUS_ERROR_NO_NETWORK
		class NoNetworkError : public Error { using Error::Error; }; 
    // DBUS_ERROR_ADDRESS_IN_USE
		class AddressInUseError : public Error { using Error::Error; }; 
    // DBUS_ERROR_DISCONNECTED
		class DisconnectedError : public Error { using Error::Error; }; 
    // DBUS_ERROR_INVALID_ARGS
		class InvalidArgsError : public Error { using Error::Error; }; 
    // DBUS_ERROR_FILE_NOT_FOUND
		class FileNotFoundError : public Error { using Error::Error; }; 
    // DBUS_ERROR_FILE_EXISTS
		class FileExistsError : public Error { using Error::Error; }; 
    // DBUS_ERROR_UNKNOWN_METHOD
		class UnknownMethodError : public Error { using Error::Error; }; 
    // DBUS_ERROR_UNKNOWN_OBJECT
		class UnknownObjectError : public Error { using Error::Error; }; 
    // DBUS_ERROR_UNKNOWN_INTERFACE
		class UnknownInterfaceError : public Error { using Error::Error; }; 
    // DBUS_ERROR_UNKNOWN_PROPERTY
		class UnknownPropertyError : public Error { using Error::Error; }; 
    // DBUS_ERROR_PROPERTY_READ_ONLY
		class PropertyReadOnlyError : public Error { using Error::Error; }; 
    // DBUS_ERROR_TIMED_OUT
		class TimedOutError : public Error { using Error::Error; }; 
    // DBUS_ERROR_MATCH_RULE_NOT_FOUND
		class MatchRuleNotFoundError : public Error { using Error::Error; }; 
    // DBUS_ERROR_MATCH_RULE_INVALID
		class MatchRuleInvalidError : public Error { using Error::Error; }; 
    // DBUS_ERROR_SPAWN_EXEC_FAILED
		class SpawnExecFailedError : public Error { using Error::Error; }; 
    // DBUS_ERROR_SPAWN_FORK_FAILED
		class SpawnForkFailedError : public Error { using Error::Error; }; 
    // DBUS_ERROR_SPAWN_CHILD_EXITED
		class SpawnChildExitedError : public Error { using Error::Error; }; 
    // DBUS_ERROR_SPAWN_CHILD_SIGNALED
		class SpawnChildSignaledError : public Error { using Error::Error; }; 
    // DBUS_ERROR_SPAWN_FAILED
		class SpawnFailedError : public Error { using Error::Error; }; 
    // DBUS_ERROR_SPAWN_SETUP_FAILED
		class SpawnSetupFailedError : public Error { using Error::Error; }; 
    // DBUS_ERROR_SPAWN_CONFIG_INVALID
		class SpawnConfigInvalidError : public Error { using Error::Error; }; 
    // DBUS_ERROR_SPAWN_SERVICE_INVALID
		class SpawnServiceInvalidError : public Error { using Error::Error; }; 
    // DBUS_ERROR_SPAWN_SERVICE_NOT_FOUND
		class SpawnServiceNotFoundError : public Error { using Error::Error; }; 
    // DBUS_ERROR_SPAWN_PERMISSIONS_INVALID
		class SpawnPermissionsInvalidError : public Error { using Error::Error; }; 
    // DBUS_ERROR_SPAWN_FILE_INVALID
		class SpawnFileInvalidError : public Error { using Error::Error; }; 
    // DBUS_ERROR_SPAWN_NO_MEMORY
		class SpawnNoMemoryError : public Error { using Error::Error; }; 
    // DBUS_ERROR_UNIX_PROCESS_ID_UNKNOWN
		class UnixProcessIdUnknownError : public Error { using Error::Error; }; 
    // DBUS_ERROR_INVALID_SIGNATURE
		class InvalidSignatureError : public Error { using Error::Error; }; 
    // DBUS_ERROR_INVALID_FILE_CONTENT
		class InvalidFileContentError : public Error { using Error::Error; }; 
    // DBUS_ERROR_SELINUX_SECURITY_CONTEXT_UNKNOWN
		class SelinuxSecurityContextUnknownError : public Error { using Error::Error; }; 
    // DBUS_ERROR_ADT_AUDIT_DATA_UNKNOWN
		class AdtAuditDataUnknownError : public Error { using Error::Error; }; 
    // DBUS_ERROR_OBJECT_PATH_IN_USE
		class ObjectPathInUseError : public Error { using Error::Error; }; 
    // DBUS_ERROR_INCONSISTENT_MESSAGE
		class InconsistentMessageError : public Error { using Error::Error; }; 
    // DBUS_ERROR_INTERACTIVE_AUTHORIZATION_REQUIRED
		class InteractiveAuthorizationRequiredError : public Error { using Error::Error; }; 
    // DBUS_ERROR_NOT_CONTAINER
		class NotContainerError : public Error { using Error::Error; }; 

		void throwErrAndFree();

		// Wraps a DBusMessage to have a constructor->destructor lifecycle
		class UniqueMessage {
			public:
				DBusMessage* msg;
				UniqueMessage(DBusMessage* msg) : msg(msg) {};
				~UniqueMessage() { if (this->msg != nullptr) dbus_message_unref(this->msg); }
		};

		class Message {
			public:
				bool isNull();
				bool notNull();
				void getArgs(int32_t firstArgType, ...);
				int32_t type();
				const char* sender();
				const char* destination();
				const char* interface();
				const char* member();
				uint32_t serial();
				uint32_t replySerial();
				Message newMethodReturn();
				Message* appendArgs(int32_t firstArgType, ...);
				void send();
				void send(uint32_t serial);
				Message sendAwait(int32_t timeout);

				std::shared_ptr<UniqueMessage> msg;
				DBus* dbus;

				Message(std::shared_ptr<UniqueMessage> msg, DBus* dbus);
		};

		DBus(DBusBusType type);
		~DBus();
		bool nameHasOwner(const char* name);
		int32_t requestName(const char* name, uint32_t flags);
		void addMatch(const char* rule);
		void readWrite(int32_t timeoutMS);
		void readWriteDispatch(int32_t timeoutMS);
		void flush();
		Message popMessage();
		Message newMethodCall(const char* destination, const char* path, const char* interface, const char* method);
		Message newSignal(const char* path, const char* interface, const char* name);
		void becomeMonitor(std::vector<const char*> filters);

		// TODO: support unregister function?
		void registerObjectPath(
			const char* path,
			void* userData,
			std::function<DBusHandlerResult(void* userData, DBus::Message msg)> handler
		);

		DBusConnection* conn;
		DBusError err;
};
