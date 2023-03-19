#!/bin/bash

sudo systemctl stop naga
sudo sh src/nagaKillroot.sh

sudo echo "Installing requirements..."

sudo apt install libx11-dev xdotool xinput g++ libxtst-dev libxmu-dev nano pkexec procps

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

grep -qF "user=$USER" /home/razerInput/.naga/keyMap.txt || sudo sed -i "1i user=$USER" /home/razerInput/.naga/keyMap.txt

sudo groupadd -f razerInputGroup
sudo bash -c "useradd razerInput > /dev/null 2>&1"
sudo usermod -aG razerInputGroup razerInput
sudo usermod -s /sbin/nologin razerInput

echo 'KERNEL=="event[0-9]*",SUBSYSTEM=="input",GROUP="razerInputGroup",MODE="640"' > /tmp/80-naga.rules

sudo mv /tmp/80-naga.rules /etc/udev/rules.d/80-naga.rules

sudo cp -f src/naga.service /etc/systemd/system/
sudo mkdir -p /etc/systemd/system/naga.service.d
sudo cp -f src/naga.conf /etc/systemd/system/naga.service.d/
echo "$DISPLAY" | sudo tee -a /etc/systemd/system/naga.service.d/naga.conf > /dev/null


#udev reload so no need to reboot
sudo udevadm control --reload-rules && sudo udevadm trigger

sleep 0.5
sudo cat /etc/sudoers | grep -qxF "razerInput ALL=($USER) ALL" || echo -e "\nrazerInput ALL=($USER) ALL\n" | sudo EDITOR='tee -a' visudo > /dev/null
grep -qF 'xhost +SI:localuser:razerInput' ~/.profile || echo -e "\nxhost +SI:localuser:razerInput\n" >> ~/.profile
sudo touch /home/razerInput/.profile && grep -qxF "export PATH=\`sudo -u $USER echo \$PATH\`" /home/razerInput/.profile || echo "export PATH=\`sudo -u $USER echo \$PATH\`" | sudo tee -a /home/razerInput/.profile > /dev/null
sudo -u razerInput bash -c "export PATH=$PATH"
xhost +SI:localuser:razerInput

sudo systemctl enable naga
sudo systemctl start naga

tput setaf 2; printf "Service started !\nStop with naga service stop\nStart with naga service start\n" ; tput sgr0;
printf "Star the repo here 😁 :\nhttps://github.com/lostallmymoney/Razer_Mouse_Linux\n"
cd ..
