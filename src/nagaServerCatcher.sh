#!/bin/sh

# Short sleep to let the session fully initialize
sleep 2

print_green() {
	printf '\033[32m%s\033[0m\n' "$1"
}

# --- Session detection (Wayland vs X11) ---

SESSION="x11"

# 1. Most reliable (runtime environment)
if [ -n "$WAYLAND_DISPLAY" ]; then
	SESSION="wayland"

elif [ "$XDG_SESSION_TYPE" = "wayland" ]; then
	SESSION="wayland"

else
	# 2. Fallback: systemd session lookup (more stable than grepping loginctl)
	SESSION_ID="$(loginctl | awk -v u="$(whoami)" '$3==u {print $1; exit}')"

	if [ -n "$SESSION_ID" ]; then
		SESSION_TYPE="$(loginctl show-session "$SESSION_ID" -p Type --value 2>/dev/null)"

		if echo "$SESSION_TYPE" | grep -qi wayland; then
			SESSION="wayland"
		fi
	fi
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
