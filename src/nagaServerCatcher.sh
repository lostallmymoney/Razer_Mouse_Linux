#!/bin/sh

# Short sleep to let the session fully initialize
sleep 2

print_green() {
	printf '\033[32m%s\033[0m\n' "$1"
}

# --- Session detection ---

SESSION="x11"

if [ -n "$WAYLAND_DISPLAY" ] || [ "$XDG_SESSION_TYPE" = "wayland" ]; then
	SESSION="wayland"
else
	SESSION_ID="$(loginctl | awk -v u="$(whoami)" '$3==u {print $1; exit}')"

	if [ -n "$SESSION_ID" ] &&
		loginctl show-session "$SESSION_ID" -p Type --value 2>/dev/null | grep -qi wayland; then
		SESSION="wayland"
	fi
fi

# --- Pick binary ---
# On X11, fall back to Wayland if nagaX11 is missing. (This is due to wayland sometimes failing at setting some env variables due to some mystery reasons. 
# Wayland detection could stop working out of the blue.

if [ "$SESSION" = "wayland" ]; then
	NAGA="nagaWayland"
else
	if command -v nagaX11 >/dev/null 2>&1; then
		NAGA="nagaX11"
	else
		NAGA="nagaWayland"
	fi
fi

if ! command -v "$NAGA" >/dev/null 2>&1; then
	printf 'Error: %s was not found.\n' "$NAGA"
	exit 1
fi

# --- Start services ---

if [ "$NAGA" = "nagaWayland" ]; then
	print_green "Starting Wayland"

	if command -v gnome-extensions >/dev/null 2>&1; then
		if ! gnome-extensions info window-calls-extended@hseliger.eu | grep -q "Enabled: Yes"; then
			gnome-extensions enable window-calls-extended@hseliger.eu
		fi
	fi

	killall nagaDotoold >/dev/null 2>&1
	setsid sh -c 'nagaDotoold' &
else
	print_green "Starting X11"
fi

if [ $# -eq 0 ]; then
	"$NAGA" serviceHelper
else
	"$NAGA" serviceHelper "$1"
fi
