#!/bin/sh

# Short sleep to let the session fully initialize
sleep 2

print_green() {
	printf '\033[32m%s\033[0m\n' "$1"
}

# Detect Wayland explicitly
if [ "$XDG_SESSION_TYPE" = "wayland" ] || [ -n "$WAYLAND_DISPLAY" ]; then
	SESSION="wayland"
else
	SESSION="x11"
fi

# Start services
if [ "$SESSION" = "wayland" ]; then
	print_green "Starting Wayland"

	if command -v gnome-extensions >/dev/null 2>&1; then
		if ! gnome-extensions info window-calls-extended@hseliger.eu | grep -q "Enabled: Yes"; then
			gnome-extensions enable window-calls-extended@hseliger.eu
		fi
	fi

	killall nagaDotoold >/dev/null 2>&1
	setsid sh -c 'nagaDotoold' &

	if [ $# -eq 0 ]; then
		nagaWayland serviceHelper
	else
		nagaWayland serviceHelper "$1"
	fi
else
	print_green "Starting X11"

	if [ $# -eq 0 ]; then
		nagaX11 serviceHelper
	else
		nagaX11 serviceHelper "$1"
	fi
fi
