#!/bin/sh
# shellcheck disable=SC2046
# shellcheck disable=SC2009

if [ "$(id -u)" -ne 0 ]; then
	if [ $# -eq 0 ]; then
		#X11
		kill $(pgrep -f "nagaX11 serviceHelper") >/dev/null 2>&1
		if [ "$(pgrep -f "nagaX11 serviceHelper" -G 0 -c)" -ne 0 ]; then
			pkexec --user root kill $(pgrep -f "nagaX11 serviceHelper" -G 0)
		fi
		#Wayland
		kill $(pgrep -f "nagaWayland serviceHelper") >/dev/null 2>&1
		if [ "$(pgrep -f "nagaWayland serviceHelper" -G 0 -c)" -ne 0 ]; then
			pkexec --user root kill $(pgrep -f "nagaWayland serviceHelper" -G 0)
		fi
	else
		#X11
		kill $(pgrep -f "nagaX11 serviceHelper" | grep -v "$1") >/dev/null 2>&1
		if [ "$(pgrep -f "nagaX11 serviceHelper" -G 0 | grep -v "$1" -c)" -ne 0 ]; then
			pkexec --user root kill $(pgrep -f "nagaX11 serviceHelper" -G 0 | grep -v "$1")
		fi
		#Wayland
		kill $(pgrep -f "nagaWayland serviceHelper" | grep -v "$1") >/dev/null 2>&1
		if [ "$(pgrep -f "nagaWayland serviceHelper" -G 0 | grep -v "$1" -c)" -ne 0 ]; then
			pkexec --user root kill $(pgrep -f "nagaWayland serviceHelper" -G 0 | grep -v "$1")
		fi
	fi
else
	if [ $# -eq 0 ]; then
		#X11
		kill $(ps aux | grep "nagaX11 serviceHelper" | grep -v root | grep -v grep | awk '{print $2}') >/dev/null 2>&1
		if [ "$(pgrep -f "nagaX11 serviceHelper" -G 0 -c)" -ne 0 ]; then
			kill $(pgrep -f "nagaX11 serviceHelper" -G 0)
		fi
		#Wayland
		kill $(ps aux | grep "nagaWayland serviceHelper" | grep -v root | grep -v grep | awk '{print $2}') >/dev/null 2>&1
		if [ "$(pgrep -f "nagaWayland serviceHelper" -G 0 -c)" -ne 0 ]; then
			kill $(pgrep -f "nagaWayland serviceHelper" -G 0)
		fi
	else
		#X11
		kill $(ps aux | grep "nagaX11 serviceHelper" | grep -v root | grep -v "$1" | grep -v grep | awk '{print $2}') >/dev/null 2>&1
		if [ "$(pgrep -f "nagaX11 serviceHelper" -G 0 | grep -v "$1" -c)" -ne 0 ]; then
			kill $(pgrep -f "nagaX11 serviceHelper" -G 0 | grep -v "$1")
		fi
		#Wayland
		kill $(ps aux | grep "nagaWayland serviceHelper" | grep -v root | grep -v "$1" | grep -v grep | awk '{print $2}') >/dev/null 2>&1
		if [ "$(pgrep -f "nagaWayland serviceHelper" -G 0 | grep -v "$1" -c)" -ne 0 ]; then
			kill $(pgrep -f "nagaWayland serviceHelper" -G 0 | grep -v "$1")
		fi
	fi
fi
