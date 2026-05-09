#pragma once

#include <dbus/dbus.h>
#include <iostream>
#include <string>

inline std::string getActiveWindowTitle()
{
    constexpr const char* DB_INTERFACE  = "org.gnome.Shell.Extensions.WindowsExt";
    constexpr const char* DB_DESTINATION = "org.gnome.Shell";
    constexpr const char* DB_PATH        = "/org/gnome/Shell/Extensions/WindowsExt";
    constexpr const char* DB_METHOD      = "FocusClass";

    DBusError error;
    dbus_error_init(&error);

    DBusConnection* connection = dbus_bus_get(DBUS_BUS_SESSION, &error);
    if (dbus_error_is_set(&error))
    {
        std::cerr << "Error connecting to bus: " << error.message << '\n';
        dbus_error_free(&error);
        return {};
    }

    DBusMessage* message = dbus_message_new_method_call(
        DB_DESTINATION,
        DB_PATH,
        DB_INTERFACE,
        DB_METHOD
    );

    if (!message)
    {
        std::cerr << "Error creating message\n";
        return {};
    }

    DBusMessage* reply = dbus_connection_send_with_reply_and_block(
        connection,
        message,
        -1,
        &error
    );

    dbus_message_unref(message);

    if (dbus_error_is_set(&error))
    {
        std::cerr << "Error calling method: " << error.message << '\n';
        dbus_error_free(&error);
        return {};
    }

    char* result = nullptr;

    if (!dbus_message_get_args(
            reply,
            &error,
            DBUS_TYPE_STRING,
            &result,
            DBUS_TYPE_INVALID))
    {
        std::cerr << "Error reading reply: " << error.message << '\n';
        dbus_error_free(&error);
        dbus_message_unref(reply);
        return {};
    }

    std::string output = result ? result : "";

    dbus_message_unref(reply);

    return output;
}
