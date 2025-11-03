#!/bin/sh

# Short sleep to let the session fully initialize
sleep 2

# Detect Wayland explicitly
if [ "$XDG_SESSION_TYPE" = "wayland" ] || [ -n "$WAYLAND_DISPLAY" ]; then
    SESSION="wayland"
else
    SESSION="x11"
fi
# Start services
if [ "$SESSION" = "wayland" ]; then
    echo "Starting Wayland"
    if command -v gnome-extensions >/dev/null 2>&1; then
        gnome-extensions enable window-calls-extended@hseliger.eu
    fi
    killall dotoold >/dev/null 2>&1
    killall nagaDotoold >/dev/null 2>&1
    setsid bash -c 'nagaDotoold' &
    if [ $# -eq 0 ]; then
        nagaWayland serviceHelper
    else
        nagaWayland serviceHelper "$1"
    fi
else
    echo "Starting X11"
    if [ $# -eq 0 ]; then
        nagaX11 serviceHelper
    else
        nagaX11 serviceHelper "$1"
    fi
fi
