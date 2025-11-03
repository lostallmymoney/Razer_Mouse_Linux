#!/bin/sh
sh /usr/local/bin/Naga_Linux/nagaKillroot.sh
sleep 0.3 >/dev/null 2>&1 &
echo "Deleting app"
#Deleting nagaDotool wrappers
sudo rm -vf /usr/local/bin/nagaDotool /usr/local/bin/nagaDotoolc /usr/local/bin/nagaDotoold
sudo rm -vf /tmp/naga/nagadotool-pipe
sudo rmdir --ignore-fail-on-non-empty /tmp/naga 2>/dev/null || true

sudo gnome-extensions disable window-calls-extended@hseliger.eu
sudo gnome-extensions uninstall -q window-calls-extended@hseliger.eu
sudo groupdel razerInputGroup

sudo rm -vf /usr/local/bin/nagaX11
sudo rm -vf /usr/local/bin/nagaWayland
sudo rm -vf /etc/udev/rules.d/80-naga.rules
sudo rm -vf /etc/udev/rules.d/80-nagaWayland.rules
sudo rm -vf /etc/systemd/system/naga.service
sudo setsid rm -rvf /usr/local/bin/Naga_Linux/

# Attempt to remove Naga sudoers includes
if [ -f /etc/sudoers.d/naga ] ; then
	echo "Removing Naga sudoers.d entries..."
	sudo rm -vf /etc/sudoers.d/naga
fi

sudo visudo -c

# Remove alias naga from .bash_aliases
if [ -f "$HOME/.bash_aliases" ]; then
    sed -i "/alias naga=/d" "$HOME/.bash_aliases"
    echo "Removed alias naga from .bash_aliases."
fi

# Remove environment and autostart lines from ~/.profile
if [ -f "$HOME/.profile" ]; then
	sed -i '/env | tee \~\/\.naga\/envSetup/d' "$HOME/.profile"
	sed -i '/sudo systemctl start naga/d' "$HOME/.profile"
fi

# Remove ~/.naga/envSetup if present
if [ -f "$HOME/.naga/envSetup" ]; then
	rm -f "$HOME/.naga/envSetup"
	echo "Removed ~/.naga/envSetup."
fi
