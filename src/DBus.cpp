#include "DBus.hpp"
#include <stdexcept>
#include <cstring>

DBus::DBus(DBusBusType type) {
	dbus_error_init(&err);
	dbus_threads_init_default();
	this->conn = dbus_bus_get_private(type, &err);
	this->throwErrAndFree();
}

DBus::~DBus() {
	dbus_connection_close(this->conn);
	dbus_connection_unref(this->conn);
};

void DBus::throwErrAndFree() {
	if (!dbus_error_is_set(&err)) return;

	std::string name = err.name;
	std::string message = err.message;

	dbus_error_free(&err);

	if (name == DBUS_ERROR_FAILED) {
	  throw FailedError(message.c_str());
	} else if (name == DBUS_ERROR_NO_MEMORY) {
	  throw NoMemoryError(message.c_str());
	} else if (name == DBUS_ERROR_SERVICE_UNKNOWN) {
	  throw ServiceUnknownError(message.c_str());
	} else if (name == DBUS_ERROR_NAME_HAS_NO_OWNER) {
	  throw NameHasNoOwnerError(message.c_str());
	} else if (name == DBUS_ERROR_NO_REPLY) {
	  throw NoReplyError(message.c_str());
	} else if (name == DBUS_ERROR_IO_ERROR) {
	  throw IoErrorError(message.c_str());
	} else if (name == DBUS_ERROR_BAD_ADDRESS) {
	  throw BadAddressError(message.c_str());
	} else if (name == DBUS_ERROR_NOT_SUPPORTED) {
	  throw NotSupportedError(message.c_str());
	} else if (name == DBUS_ERROR_LIMITS_EXCEEDED) {
	  throw LimitsExceededError(message.c_str());
	} else if (name == DBUS_ERROR_ACCESS_DENIED) {
	  throw AccessDeniedError(message.c_str());
	} else if (name == DBUS_ERROR_AUTH_FAILED) {
	  throw AuthFailedError(message.c_str());
	} else if (name == DBUS_ERROR_NO_SERVER) {
	  throw NoServerError(message.c_str());
	} else if (name == DBUS_ERROR_TIMEOUT) {
	  throw TimeoutError(message.c_str());
	} else if (name == DBUS_ERROR_NO_NETWORK) {
	  throw NoNetworkError(message.c_str());
	} else if (name == DBUS_ERROR_ADDRESS_IN_USE) {
	  throw AddressInUseError(message.c_str());
	} else if (name == DBUS_ERROR_DISCONNECTED) {
	  throw DisconnectedError(message.c_str());
	} else if (name == DBUS_ERROR_INVALID_ARGS) {
	  throw InvalidArgsError(message.c_str());
	} else if (name == DBUS_ERROR_FILE_NOT_FOUND) {
	  throw FileNotFoundError(message.c_str());
	} else if (name == DBUS_ERROR_FILE_EXISTS) {
	  throw FileExistsError(message.c_str());
	} else if (name == DBUS_ERROR_UNKNOWN_METHOD) {
	  throw UnknownMethodError(message.c_str());
	} else if (name == DBUS_ERROR_UNKNOWN_OBJECT) {
	  throw UnknownObjectError(message.c_str());
	} else if (name == DBUS_ERROR_UNKNOWN_INTERFACE) {
	  throw UnknownInterfaceError(message.c_str());
	} else if (name == DBUS_ERROR_UNKNOWN_PROPERTY) {
	  throw UnknownPropertyError(message.c_str());
	} else if (name == DBUS_ERROR_PROPERTY_READ_ONLY) {
	  throw PropertyReadOnlyError(message.c_str());
	} else if (name == DBUS_ERROR_TIMED_OUT) {
	  throw TimedOutError(message.c_str());
	} else if (name == DBUS_ERROR_MATCH_RULE_NOT_FOUND) {
	  throw MatchRuleNotFoundError(message.c_str());
	} else if (name == DBUS_ERROR_MATCH_RULE_INVALID) {
	  throw MatchRuleInvalidError(message.c_str());
	} else if (name == DBUS_ERROR_SPAWN_EXEC_FAILED) {
	  throw SpawnExecFailedError(message.c_str());
	} else if (name == DBUS_ERROR_SPAWN_FORK_FAILED) {
	  throw SpawnForkFailedError(message.c_str());
	} else if (name == DBUS_ERROR_SPAWN_CHILD_EXITED) {
	  throw SpawnChildExitedError(message.c_str());
	} else if (name == DBUS_ERROR_SPAWN_CHILD_SIGNALED) {
	  throw SpawnChildSignaledError(message.c_str());
	} else if (name == DBUS_ERROR_SPAWN_FAILED) {
	  throw SpawnFailedError(message.c_str());
	} else if (name == DBUS_ERROR_SPAWN_SETUP_FAILED) {
	  throw SpawnSetupFailedError(message.c_str());
	} else if (name == DBUS_ERROR_SPAWN_CONFIG_INVALID) {
	  throw SpawnConfigInvalidError(message.c_str());
	} else if (name == DBUS_ERROR_SPAWN_SERVICE_INVALID) {
	  throw SpawnServiceInvalidError(message.c_str());
	} else if (name == DBUS_ERROR_SPAWN_SERVICE_NOT_FOUND) {
	  throw SpawnServiceNotFoundError(message.c_str());
	} else if (name == DBUS_ERROR_SPAWN_PERMISSIONS_INVALID) {
	  throw SpawnPermissionsInvalidError(message.c_str());
	} else if (name == DBUS_ERROR_SPAWN_FILE_INVALID) {
	  throw SpawnFileInvalidError(message.c_str());
	} else if (name == DBUS_ERROR_SPAWN_NO_MEMORY) {
	  throw SpawnNoMemoryError(message.c_str());
	} else if (name == DBUS_ERROR_UNIX_PROCESS_ID_UNKNOWN) {
	  throw UnixProcessIdUnknownError(message.c_str());
	} else if (name == DBUS_ERROR_INVALID_SIGNATURE) {
	  throw InvalidSignatureError(message.c_str());
	} else if (name == DBUS_ERROR_INVALID_FILE_CONTENT) {
	  throw InvalidFileContentError(message.c_str());
	} else if (name == DBUS_ERROR_SELINUX_SECURITY_CONTEXT_UNKNOWN) {
	  throw SelinuxSecurityContextUnknownError(message.c_str());
	} else if (name == DBUS_ERROR_ADT_AUDIT_DATA_UNKNOWN) {
	  throw AdtAuditDataUnknownError(message.c_str());
	} else if (name == DBUS_ERROR_OBJECT_PATH_IN_USE) {
	  throw ObjectPathInUseError(message.c_str());
	} else if (name == DBUS_ERROR_INCONSISTENT_MESSAGE) {
	  throw InconsistentMessageError(message.c_str());
	} else if (name == DBUS_ERROR_INTERACTIVE_AUTHORIZATION_REQUIRED) {
	  throw InteractiveAuthorizationRequiredError(message.c_str());
	} else if (name == DBUS_ERROR_NOT_CONTAINER) {
	  throw NotContainerError(message.c_str());
	} else {
	  throw Error(message.c_str());
	}
};

bool DBus::nameHasOwner(const char* name) {
	bool ret = dbus_bus_name_has_owner(this->conn, name, &err);
	this->throwErrAndFree();

	return ret;
}

int32_t DBus::requestName(const char* name, uint32_t flags) {
	int32_t ret = dbus_bus_request_name(this->conn, name, flags, &err);
	this->throwErrAndFree();

	return ret;
}

void DBus::addMatch(const char* rule) {
	dbus_bus_add_match(this->conn, rule, &err);
	this->throwErrAndFree();
}

void DBus::readWrite(int32_t timeoutMS) {
	if (!dbus_connection_read_write(this->conn, timeoutMS)) throw DisconnectedError("Lost D-Bus connection");
}

void DBus::readWriteDispatch(int32_t timeoutMS) {
	if (!dbus_connection_read_write_dispatch(this->conn, timeoutMS)) throw DisconnectedError("Lost D-Bus connection");
}

void DBus::flush() {
	dbus_connection_flush(this->conn);
};

DBus::Message DBus::popMessage() {
	DBusMessage* msg = dbus_connection_pop_message(this->conn);

	auto ptr = std::shared_ptr<DBus::UniqueMessage>(new DBus::UniqueMessage(msg));
	DBus::Message ret(ptr, this);
	return ret;
};

DBus::Message DBus::newMethodCall(const char* destination, const char* path, const char* interface, const char* method) {
	DBusMessage* msg = dbus_message_new_method_call(destination, path, interface, method);

	auto ptr = std::shared_ptr<DBus::UniqueMessage>(new DBus::UniqueMessage(msg));
	DBus::Message ret(ptr, this);
	return ret;
};

DBus::Message DBus::newSignal(const char* path, const char* interface, const char* name) {
	DBusMessage* msg = dbus_message_new_signal(path, interface, name);

	auto ptr = std::shared_ptr<DBus::UniqueMessage>(new DBus::UniqueMessage(msg));
	DBus::Message ret(ptr, this);
	return ret;
};

void DBus::becomeMonitor(std::vector<const char*> filters) {
	auto callMsg = this->newMethodCall(DBUS_SERVICE_DBUS, DBUS_PATH_DBUS, DBUS_INTERFACE_MONITORING, "BecomeMonitor");

	if (callMsg.isNull()) { throw std::bad_alloc(); }

	dbus_uint32_t zero = 0;
	const char** array = &filters[0];
	callMsg.appendArgs(DBUS_TYPE_ARRAY, DBUS_TYPE_STRING, &array, filters.size(), 
										 DBUS_TYPE_UINT32, &zero,
										 DBUS_TYPE_INVALID);

	auto retMsg = callMsg.sendAwait(-1);
	if (retMsg.isNull()) { throw std::bad_alloc(); }
};

// TODO better names for these? thread safety? (perhaps shouldn't be static)
struct UserDataWrap {	
	std::function<DBusHandlerResult(void* userData, DBus::Message msg)> handler;
	DBus* dbus;
	void* realUserData;
};

static DBusHandlerResult registerObjectPathFuncWrap(
	DBusConnection *conn,
	DBusMessage *msg,
	void *user_data
) {
	printf("other yo\n");
	UserDataWrap* data = (UserDataWrap*)(user_data);

	auto ptr = std::shared_ptr<DBus::UniqueMessage>(new DBus::UniqueMessage(msg));
	DBus::Message dmsg(ptr, data->dbus);

	return data->handler(data->realUserData, dmsg);
}

static std::vector<UserDataWrap> wrappedUserData; // TODO: remove elements on unRegister

void DBus::registerObjectPath(
	const char* path,
	void* userData,
	std::function<DBusHandlerResult(void* userData, DBus::Message msg)> handler
) {
	DBusObjectPathVTable vtable;
	vtable.unregister_function = NULL;
	vtable.message_function = registerObjectPathFuncWrap;
	/*vtable.message_function = [&handler, this](
		DBusConnection *conn,
		DBusMessage *msg,
		void *user_data
	) -> DBusHandlerResult {

		auto ptr = std::shared_ptr<DBus::UniqueMessage>(new DBus::UniqueMessage(msg));
		DBus::Message dmsg(ptr, this);
		return handler(user_data, dmsg);
	};*/

	UserDataWrap s = {handler, this, userData};
	wrappedUserData.push_back(s);

	printf("registering it yo\n");
	dbus_connection_try_register_object_path(this->conn, path, &vtable, &(wrappedUserData.back()), &err);
	this->throwErrAndFree();
};

DBus::Message::Message(std::shared_ptr<DBus::UniqueMessage> msg, DBus* dbus) : msg(msg), dbus(dbus) {};

bool DBus::Message::isNull() {
	return (this->msg.get()->msg == nullptr);
};

bool DBus::Message::notNull() {
	return !this->isNull();
};

void DBus::Message::getArgs(int32_t firstArgType, ...) {
	va_list args;
	va_start(args, firstArgType);
	dbus_message_get_args_valist(this->msg.get()->msg, &(this->dbus->err), firstArgType, args);
	va_end(args);

	this->dbus->throwErrAndFree();
};

int32_t DBus::Message::type() {
	return dbus_message_get_type(this->msg.get()->msg);
};

const char* DBus::Message::sender() {
	return dbus_message_get_sender(this->msg.get()->msg);
};

const char* DBus::Message::destination() {
	return dbus_message_get_destination(this->msg.get()->msg);
};

const char* DBus::Message::interface() {
	return dbus_message_get_interface(this->msg.get()->msg);
};

const char* DBus::Message::member() {
	return dbus_message_get_member(this->msg.get()->msg);
};

uint32_t DBus::Message::serial() {
	return dbus_message_get_serial(this->msg.get()->msg);
};

uint32_t DBus::Message::replySerial() {
	return dbus_message_get_reply_serial(this->msg.get()->msg);
};

DBus::Message DBus::Message::newMethodReturn() {
	DBusMessage* reply = dbus_message_new_method_return(this->msg.get()->msg);
	auto ptr = std::shared_ptr<DBus::UniqueMessage>(new DBus::UniqueMessage(reply));
	DBus::Message ret(ptr, this->dbus);
	return ret;
};

DBus::Message* DBus::Message::appendArgs(int32_t firstArgType, ...) {	
	va_list args;
	va_start(args, firstArgType);
	dbus_message_append_args_valist(this->msg.get()->msg, firstArgType, args);
	va_end(args);

	return this;
};

void DBus::Message::send() {
	dbus_connection_send(this->dbus->conn, this->msg.get()->msg, NULL);
	this->dbus->flush();
};

void DBus::Message::send(uint32_t serial) {
	dbus_connection_send(this->dbus->conn, this->msg.get()->msg, &serial);
};

DBus::Message DBus::Message::sendAwait(int32_t timeout) {
	auto r = dbus_connection_send_with_reply_and_block(this->dbus->conn, this->msg.get()->msg, timeout, &(this->dbus->err));
	this->dbus->throwErrAndFree();

	auto ptr = std::shared_ptr<DBus::UniqueMessage>(new DBus::UniqueMessage(r));
	DBus::Message ret(ptr, this->dbus);
	return ret;
};

