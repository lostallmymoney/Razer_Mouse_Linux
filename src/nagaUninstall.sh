#!/bin/sh
sh /usr/local/bin/Naga_Linux/nagaKillroot.sh
sleep 0.3 >/dev/null 2>&1 &
echo "Deleting app"
#Deleting dotool
sudo rm -vf /usr/local/bin/dotool /usr/local/bin/dotoolc /usr/local/bin/dotoold
sudo rm -vf /etc/udev/rules.d/80-dotool.rules
sudo rm -vf /usr/share/man/man1/dotool.1

sudo gnome-extensions disable window-calls-extended@hseliger.eu
sudo gnome-extensions uninstall -q window-calls-extended@hseliger.eu
sudo groupdel razerInputGroup

sudo rm -vf /usr/local/bin/nagaX11
sudo rm -vf /usr/local/bin/nagaWayland
sudo rm -vf /etc/udev/rules.d/80-naga.rules
sudo rm -vf /etc/systemd/system/naga.service
sudo setsid rm -rvf /usr/local/bin/Naga_Linux/

# Attempt to remove Naga sudoers includes
if [ -f /etc/sudoers.d/naga ] ; then
	echo "Removing Naga sudoers.d entries..."
	sudo rm -vf /etc/sudoers.d/naga
fi
