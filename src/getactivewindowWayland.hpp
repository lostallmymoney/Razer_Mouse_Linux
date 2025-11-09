#pragma once

#include <dbus/dbus.h>
#include <iostream>
#include <string>

namespace nagaWaylandWindowExt
{
	inline constexpr const char *DB_INTERFACE = "org.gnome.Shell.Extensions.WindowsExt";
	inline constexpr const char *DB_DESTINATION = "org.gnome.Shell";
	inline constexpr const char *DB_PATH = "/org/gnome/Shell/Extensions/WindowsExt";
	inline constexpr const char *DB_METHOD = "FocusClass";

	inline std::string getTitle()
	{
		DBusError error;
		dbus_error_init(&error);

		DBusConnection *connection = dbus_bus_get(DBUS_BUS_SESSION, &error);
		if (dbus_error_is_set(&error))
		{
			std::cerr << "Error connecting to bus: " << error.message << std::endl;
			dbus_error_free(&error);
			return "";
		}

		DBusMessage *message = dbus_message_new_method_call(DB_DESTINATION, DB_PATH, DB_INTERFACE, DB_METHOD);
		if (message == nullptr)
		{
			std::cerr << "Error creating message" << std::endl;
			return "";
		}

		DBusMessage *reply = dbus_connection_send_with_reply_and_block(connection, message, -1, &error);
		if (dbus_error_is_set(&error))
		{
			std::cerr << "Error calling method: " << error.message << std::endl;
			dbus_error_free(&error);
			return "";
		}

		char *result;
		if (!dbus_message_get_args(reply, &error, DBUS_TYPE_STRING, &result, DBUS_TYPE_INVALID))
		{
			std::cerr << "Error reading reply: " << error.message << std::endl;
			dbus_error_free(&error);
			return "";
		}

		dbus_message_unref(message);
		dbus_message_unref(reply);
		return result;
	}
}
