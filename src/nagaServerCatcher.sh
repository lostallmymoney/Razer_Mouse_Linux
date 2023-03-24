#!/bin/sh
#Finds the visual server and starts the right one
if [ "$(loginctl show-session $(loginctl | grep $(whoami) | awk '{print $1}') -p Type)" = "Type=wayland" ]; then
	killall dotoold
	setsid bash -c 'dotoold'&
	if [ $# -eq 0 ]; then
		nagaWayland serviceHelper
	else
		nagaWayland serviceHelper $1
	fi
else
	if [ $# -eq 0 ]; then
		nagaX11 serviceHelper
	else
		nagaX11 serviceHelper $1
	fi
fi