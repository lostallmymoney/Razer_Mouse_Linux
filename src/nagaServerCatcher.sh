#!/bin/sh
#Finds the visual server and starts the right one
while [ "$(who | wc -l)" -lt 2 ]; do
	sleep 1
done
# shellcheck disable=SC2046
if [ "$(loginctl show-session $(loginctl | grep "$(whoami)" | awk '{print $1}') | grep -c "Type=wayland")" -ne 0 ]; then
	echo "Starting Wayland"
	gnome-extensions enable window-calls-extended@hseliger.eu
	killall dotoold >/dev/null 2>&1
	setsid bash -c 'dotoold' &
	if [ $# -eq 0 ]; then
		nagaWayland serviceHelper
	else
		nagaWayland serviceHelper $1
	fi
else
	echo "Starting X11"
	if [ $# -eq 0 ]; then
		nagaX11 serviceHelper
	else
		nagaX11 serviceHelper $1
	fi
fi
