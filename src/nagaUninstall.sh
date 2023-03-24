#!/bin/sh
sh /usr/local/bin/Naga_Linux/nagaKillroot.sh
sleep 0.3 > /dev/null 2>&1 &
echo "Deleting files"
sudo rm -f /usr/local/bin/nagaX11
sudo rm -f /usr/local/bin/nagaWayland
sudo rm -f /etc/udev/rules.d/80-naga.rules
sudo rm -f /etc/systemd/system/naga.service
sudo setsid rm -rf /usr/local/bin/Naga_Linux/
