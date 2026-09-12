#pragma once

#include <dbus/dbus.h>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <string>

namespace
{
	std::string currentSessionBusAddress()
	{
		if (const char *runtime = getenv("XDG_RUNTIME_DIR"); runtime && *runtime)
			return std::string("unix:path=") + runtime + "/bus";
		return {};
	}

	DBusConnection *openSessionBusConnection(DBusError &error)
	{
		DBusConnection *connection = nullptr;

		const std::string busAddress = currentSessionBusAddress();
		if (!busAddress.empty())
		{
			connection = dbus_connection_open_private(busAddress.c_str(), &error);
			if (connection)
			{
				dbus_connection_set_exit_on_disconnect(connection, FALSE);
				if (!dbus_bus_register(connection, &error))
				{
					dbus_connection_close(connection);
					dbus_connection_unref(connection);
					dbus_error_free(&error);
					dbus_error_init(&error);
					connection = nullptr;
				}
			}
		}

		if (!connection)
		{
			connection = dbus_bus_get_private(DBUS_BUS_SESSION, &error);
			if (connection)
				dbus_connection_set_exit_on_disconnect(connection, FALSE);
		}

		return connection;
	}

	void initDbusThreads()
	{
		static std::once_flag once;
		std::call_once(once, []
						{ dbus_threads_init_default(); });
	}

	// Single process-wide connection, reused until it dies. A dead or missing
	// bus is detected and a replacement is opened at the current address, so a
	// session restart (gnome-shell crash, logout/login, resume) is self-healed.
	std::mutex busMutex;
	DBusConnection *cachedBus = nullptr;

	DBusConnection *acquireSessionBusConnection(DBusError &error)
	{
		initDbusThreads();
		std::lock_guard<std::mutex> lock(busMutex);

		if (cachedBus && dbus_connection_get_is_connected(cachedBus))
		{
			dbus_connection_ref(cachedBus);
			return cachedBus;
		}

		if (cachedBus)
		{
			dbus_connection_close(cachedBus);
			dbus_connection_unref(cachedBus);
			cachedBus = nullptr;
		}

		cachedBus = openSessionBusConnection(error);
		if (cachedBus)
			dbus_connection_ref(cachedBus);

		return cachedBus;
	}

	void releaseSessionBusConnection(DBusConnection *connection)
	{
		dbus_connection_unref(connection);
	}

	void invalidateSessionBusConnection(DBusConnection *connection)
	{
		std::lock_guard<std::mutex> lock(busMutex);
		if (cachedBus == connection)
		{
			dbus_connection_close(cachedBus);
			dbus_connection_unref(cachedBus);
			cachedBus = nullptr;
		}
	}

	void logDbusMessage(const char *color, const char *tag, const std::string &message)
	{
		static std::string lastMessage;
		if (lastMessage == message)
			return;
		lastMessage = message;
		std::cerr << "\033[" << color << "m" << tag << message << "\033[0m" << '\n';
	}

	void logDbusError(const char *tag, const DBusError &error)
	{
		logDbusMessage("91", tag, error.message);
	}

	void logDbusWarning(const char *message)
	{
		logDbusMessage("93", "Warning : ", message);
	}
}

inline std::string getActiveWindowTitle()
{
	constexpr const char *DB_INTERFACE = "org.gnome.Shell.Extensions.WindowsExt";
	constexpr const char *DB_DESTINATION = "org.gnome.Shell";
	constexpr const char *DB_PATH = "/org/gnome/Shell/Extensions/WindowsExt";
	constexpr const char *DB_METHOD = "FocusClass";

	DBusError error;
	dbus_error_init(&error);

	DBusConnection *connection = acquireSessionBusConnection(error);
	if (!connection)
	{
		logDbusError("Error : Connecting to bus: ", error);
		dbus_error_free(&error);
		return {};
	}

	DBusMessage *message = dbus_message_new_method_call(
		DB_DESTINATION,
		DB_PATH,
		DB_INTERFACE,
		DB_METHOD);

	if (!message)
	{
		std::cerr << "\033[91mError : Creating DBus message\033[0m\n";
		releaseSessionBusConnection(connection);
		return {};
	}

	DBusMessage *reply = dbus_connection_send_with_reply_and_block(
		connection,
		message,
		-1,
		&error);

	dbus_message_unref(message);

	bool wasDisconnected = dbus_error_is_set(&error) && dbus_error_has_name(&error, DBUS_ERROR_DISCONNECTED);
	releaseSessionBusConnection(connection);
	if (wasDisconnected)
		invalidateSessionBusConnection(connection);

	if (dbus_error_is_set(&error))
	{
		if (wasDisconnected)
			logDbusWarning("DBus session connection was lost, attempting to re-establish session");
		else
			logDbusError("Error : Calling DBus method: ", error);
		dbus_error_free(&error);
		return {};
	}

	char *result = nullptr;

	if (!dbus_message_get_args(
			reply,
			&error,
			DBUS_TYPE_STRING,
			&result,
			DBUS_TYPE_INVALID))
	{
		logDbusError("Error : Reading DBus reply: ", error);
		dbus_error_free(&error);
		dbus_message_unref(reply);
		return {};
	}

	std::string output = result ? result : "";

	dbus_message_unref(reply);

	return output;
}