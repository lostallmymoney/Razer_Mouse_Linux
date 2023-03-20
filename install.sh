#!/bin/bash

if [ "$(id -u)" = "0" ]; then
    echo "This script must not be run as root"
    exit 1
fi

sudo systemctl stop naga
sudo sh src/nagaKillroot.sh

sudo echo "Installing requirements..."

sudo apt install -y libx11-dev xdotool xinput g++ libxtst-dev libxmu-dev nano pkexec procps

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

sudo mkdir -p /usr/local/bin/Naga_Linux

sudo cp -f ./src/nagaXinputStart.sh /usr/local/bin/Naga_Linux/
sudo chmod 755 /usr/local/bin/Naga_Linux/nagaXinputStart.sh

sudo cp -f ./src/nagaKillroot.sh /usr/local/bin/Naga_Linux/
sudo chmod 755 /usr/local/bin/Naga_Linux/nagaKillroot.sh

sudo cp -f ./src/nagaUninstall.sh /usr/local/bin/Naga_Linux/
sudo chmod 755 /usr/local/bin/Naga_Linux/nagaUninstall.sh

_dir="/home/razerInput/.naga"
sudo mkdir -p "$_dir"
sudo cp -r -n -v "keyMap.txt" "$_dir"
sudo chown -R "root:root" "$_dir"

printf "%s\n%s" "$USER" "$(id -u "$USER")"| sudo tee /home/razerInput/.naga/user.txt > /dev/null

sudo groupadd -f razerInputGroup
sudo bash -c "useradd razerInput > /dev/null 2>&1"
sudo usermod -a -G razerInputGroup razerInput > /dev/null
#sudo usermod -a -G "$USER" razerInput > /dev/null
sudo chown -R razerInput:razerInputGroup /home/razerInput

echo 'KERNEL=="event[0-9]*",SUBSYSTEM=="input",GROUP="razerInputGroup",MODE="640"' > /tmp/80-naga.rules

sudo mv /tmp/80-naga.rules /etc/udev/rules.d/80-naga.rules

sudo cp -f src/naga.service /etc/systemd/system/
sudo mkdir -p /etc/systemd/system/naga.service.d
sudo cp -f src/naga.conf /etc/systemd/system/naga.service.d/
echo "$DISPLAY" | sudo tee -a /etc/systemd/system/naga.service.d/naga.conf > /dev/null

sudo chown -R razerInput:razerInputGroup /home/razerInput/


#udev reload so no need to reboot
sudo udevadm control --reload-rules && sudo udevadm trigger

sleep 0.5
sudo cat /etc/sudoers | grep -qxF "razerInput ALL=($USER) NOPASSWD:ALL" || printf "\nrazerInput ALL=(%s) NOPASSWD:ALL\n" "$USER" | sudo EDITOR='tee -a' visudo > /dev/null
#sudo cat /etc/sudoers | grep -qxF "$USER ALL=(ALL) NOPASSWD:/bin/systemctl start naga" || printf "\n%s ALL=(ALL) NOPASSWD:/bin/systemctl start naga\n" "$USER" | sudo EDITOR='tee -a' visudo > /dev/null


sudo systemctl enable naga
sudo systemctl start naga

tput setaf 2; printf "Service started !\nStop with naga service stop\nStart with naga service start\n" ; tput sgr0;
printf "Star the repo here 😁 :\nhttps://github.com/lostallmymoney/Razer_Mouse_Linux\n"
cd ..
