#!/bin/bash

sudo sh src/nagaKillroot.sh

sudo echo "Installing requirements..."

sudo apt install libx11-dev xdotool xinput g++ libxtst-dev libxmu-dev nano pkexec

echo "Checking requirements..."

command -v xdotool >/dev/null 2>&1 || { tput setaf 1; echo >&2 "I require xdotool but it's not installed! Aborting."; exit 1; }
command -v xinput >/dev/null 2>&1 || { tput setaf 1; echo >&2 "I require xinput but it's not installed! Aborting."; exit 1; }
command -v g++ >/dev/null 2>&1 || { tput setaf 1; echo >&2 "I require g++ but it's not installed! Aborting."; exit 1; }

reset

echo "Compiling code..."
cd src || exit
g++ nagaX11.cpp -o naga -pthread -Ofast --std=c++2b -lX11 -lXtst -lXmu

if [ ! -f ./naga ]; then
	tput setaf 1; echo "Error at compile! Ensure you have g++ installed. !!!Aborting!!!"
	exit 1
fi

echo "Copying files..."
sudo mv naga /usr/local/bin/
sudo chmod 755 /usr/local/bin/naga

cd ..

sudo groupadd -f razer

sudo mkdir -p /usr/local/bin/Naga_Linux

sudo cp -f ./src/nagaXinputStart.sh /usr/local/bin/Naga_Linux/
sudo chmod 755 /usr/local/bin/Naga_Linux/nagaXinputStart.sh

sudo cp -f ./src/nagaKillroot.sh /usr/local/bin/Naga_Linux/
sudo chmod 755 /usr/local/bin/Naga_Linux/nagaKillroot.sh

sudo cp -f ./src/nagaUninstall.sh /usr/local/bin/Naga_Linux/
sudo chmod 755 /usr/local/bin/Naga_Linux/nagaUninstall.sh

for u in $(sudo awk -F'[/:]' '{if ($3 >= 1000 && $3 != 65534) print $1}' /etc/passwd)
do
	sudo gpasswd -a "$u" razer
	_dir="/home/${u}/.naga"
	sudo mkdir -p "$_dir"
	if [ -d "$_dir" ]
	then
		sudo cp -r -n -v "keyMap.txt" "$_dir"
		sudo chown -R "root:root" "$_dir"
	fi
done
if [ -d "/root" ];
then
	sudo gpasswd -a "root" razer
	sudo mkdir -p /root/.naga
	sudo cp -r -n -v "keyMap.txt" "/root/.naga"
	sudo chown -R "root:root" "/root/.naga"
fi

echo 'KERNEL=="event[0-9]*",SUBSYSTEM=="input",GROUP="razer",MODE="640"' > /tmp/80-naga.rules

sudo mv /tmp/80-naga.rules /etc/udev/rules.d/80-naga.rules

#udev reload so no need to reboot
sudo udevadm control --reload-rules && sudo udevadm trigger

sleep 0.5
naga start

tput setaf 2; printf "Please add (naga.desktop or a script with naga start) to be executed\nwhen your window manager starts.\n" ; tput sgr0;
cd ..