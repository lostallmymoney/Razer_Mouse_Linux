#!/bin/bash

if [ "$(id -u)" = "0" ]; then
    echo "This script must not be executed as root"
    exit 1
fi

sudo sh src/nagaKillroot.sh >/dev/null

sudo echo "Installing requirements..."

sudo apt install -y libx11-dev xdotool xinput g++ libxtst-dev libxmu-dev nano pkexec procps

echo "Checking requirements..."

command -v xdotool >/dev/null 2>&1 || {
    tput setaf 1
    echo >&2 "I require xdotool but it's not installed! Aborting."
    exit 1
}
command -v xinput >/dev/null 2>&1 || {
    tput setaf 1
    echo >&2 "I require xinput but it's not installed! Aborting."
    exit 1
}
command -v g++ >/dev/null 2>&1 || {
    tput setaf 1
    echo >&2 "I require g++ but it's not installed! Aborting."
    exit 1
}

reset

echo "Compiling code..."
cd src || exit
g++ nagaX11.cpp -o naga -pthread -Ofast --std=c++2b -lX11 -lXtst -lXmu

if [ ! -f ./naga ]; then
    tput setaf 1
    echo "Error at compile! Ensure you have g++ installed. !!!Aborting!!!"
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

_dir="/home/$USER/.naga"
mkdir -p "$_dir"
sudo cp -r -n -v "keyMap.txt" "$_dir"
sudo chown -R "root:root" "$_dir"/keyMap.txt

sudo groupadd -f razerInputGroup

printf 'KERNEL=="event[0-9]*",SUBSYSTEM=="input",GROUP="razerInputGroup",MODE="640"' | sudo tee /etc/udev/rules.d/80-naga.rules > /dev/null

sudo cp -f src/naga.service /etc/systemd/system/

env | tee ~/.naga/envSetup >/dev/null
grep -qF 'env | tee ~/.naga/envSetup' ~/.profile || printf '\n%s\n' 'env | tee ~/.naga/envSetup > /dev/null' | tee -a ~/.profile > /dev/null
grep -qF 'sudo systemctl start naga' ~/.profile || printf '\n%s\n' 'sudo systemctl start naga > /dev/null' | tee -a ~/.profile > /dev/null

printf "Environment=DISPLAY=%s\n" "$DISPLAY" | sudo tee -a /etc/systemd/system/naga.service >/dev/null
printf "User=%s\n" "$USER" | sudo tee -a /etc/systemd/system/naga.service >/dev/null
printf "EnvironmentFile=/home/%s/.naga/envSetup\n" "$USER" | sudo tee -a /etc/systemd/system/naga.service >/dev/null


sudo udevadm control --reload-rules && sudo udevadm trigger

sleep 0.5
sudo cat /etc/sudoers | grep -qxF "$USER ALL=(ALL) NOPASSWD:/bin/systemctl start naga" || printf "\n%s ALL=(ALL) NOPASSWD:/bin/systemctl start naga\n" "$USER" | sudo EDITOR='tee -a' visudo >/dev/null


sudo systemctl enable naga
sudo systemctl start naga

tput setaf 2
printf "Service started !\nStop with naga service stop\nStart with naga service start\n"
tput sgr0
printf "Star the repo here 😁 :\nhttps://github.com/lostallmymoney/Razer_Mouse_Linux\n"
cd ..
