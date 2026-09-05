#!/bin/sh
[ -f /usr/local/bin/Naga_Linux/nagaKillroot.sh ] && sh /usr/local/bin/Naga_Linux/nagaKillroot.sh
sleep 0.3
printf '%s\n' "Deleting app"

sudo systemctl stop naga >/dev/null 2>&1
sudo systemctl disable naga >/dev/null 2>&1

#Deleting nagaDotool wrappers
sudo rm -vf /usr/local/bin/nagaDotool /usr/local/bin/nagaDotoolc /usr/local/bin/nagaDotoold
# Note: _installWayland.sh's tmpfiles.d rule creates /run/nagaProtected (no leading dot)
sudo rm -rvf /run/nagaProtected
sudo rm -vf /etc/tmpfiles.d/nagaProtected.conf

# gnome-extensions is per-user session state; running it under sudo would act
# on root's own (nonexistent) session instead of the real user's.
gnome-extensions disable window-calls-extended@hseliger.eu >/dev/null 2>&1
gnome-extensions uninstall -q window-calls-extended@hseliger.eu >/dev/null 2>&1
sudo groupdel razerInputGroup 2>/dev/null || true

sudo rm -vf /usr/local/bin/nagaX11
sudo rm -vf /usr/local/bin/nagaWayland
sudo rm -vf /etc/udev/rules.d/80-naga.rules
sudo rm -vf /etc/udev/rules.d/80-nagaWayland.rules
sudo rm -vf /etc/systemd/system/naga.service
sudo systemctl daemon-reload
sudo udevadm control --reload-rules
sudo udevadm trigger
sudo setsid rm -rvf /usr/local/bin/Naga_Linux/

# Remove Naga sudoers includes.
# /etc/sudoers.d isn't readable without root (mode 0750), so a `[ -f ... ]`
# check here as the regular user would silently always be false - just rm -f.
printf '%s\n' "Removing Naga sudoers.d entries..."
sudo rm -vf /etc/sudoers.d/naga

sudo visudo -c

# Remove alias naga from .bash_aliases
if [ -f "$HOME/.bash_aliases" ]; then
	sed -i "/alias naga=/d" "$HOME/.bash_aliases"
	printf '%s\n' "Removed alias naga from .bash_aliases."
fi

# Remove environment and autostart lines from ~/.profile
if [ -f "$HOME/.profile" ]; then
	sed -i '/env | tee \~\/\.naga\/envSetup/d' "$HOME/.profile"
	sed -i '/( sudo -n systemctl start naga > \/dev\/null 2>&1 || true ) &/d' "$HOME/.profile"
fi

# Remove ~/.naga/envSetup if present
if [ -f "$HOME/.naga/envSetup" ]; then
	rm -f "$HOME/.naga/envSetup"
	printf '%s\n' "Removed ~/.naga/envSetup."
fi
