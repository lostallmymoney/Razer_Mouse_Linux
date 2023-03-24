#!/bin/sh
if [ "$(id -u)" = "0" ]; then
    echo "This script must not be executed as root"
    exit 1
fi

sudo sh src/nagaKillroot.sh >/dev/null

echo "Copying files..."

sudo mkdir -p /usr/local/bin/Naga_Linux

sudo cp -f ./src/nagaXinputStart.sh /usr/local/bin/Naga_Linux/
sudo chmod 755 /usr/local/bin/Naga_Linux/nagaXinputStart.sh

sudo cp -f ./src/nagaKillroot.sh /usr/local/bin/Naga_Linux/
sudo chmod 755 /usr/local/bin/Naga_Linux/nagaKillroot.sh

sudo cp -f ./src/nagaServerCatcher.sh /usr/local/bin/Naga_Linux/
sudo chmod 755 /usr/local/bin/Naga_Linux/nagaServerCatcher.sh

sudo cp -f ./src/nagaUninstall.sh /usr/local/bin/Naga_Linux/
sudo chmod 755 /usr/local/bin/Naga_Linux/nagaUninstall.sh
sudo groupadd -f razerInputGroup

sudo echo "Installing requirements..."

printf 'KERNEL=="event[0-9]*",SUBSYSTEM=="input",GROUP="razerInputGroup",MODE="640"' | sudo tee /etc/udev/rules.d/80-naga.rules >/dev/null

LOGINTYPE=$(loginctl show-session $(loginctl | grep $(whoami) | awk '{print $1}') -p Type)
if [ "$LOGINTYPE" = "Type=wayland" ]; then
	sh ./src/_installWayland.sh
else
	sh ./src/_installX11.sh
fi

if [ "$LOGINTYPE" = "Type=wayland" ]; then
	while true; do
		read -p "Naga for Wayland is currently installing, do you want to install for X11 too ? (recommended) y/n" yn
		case $yn in
		[Yy]*)
			sh ./src/_installX11.sh
			break
			;;
		[Nn]*) break ;;
		*) echo "Please answer yes or no." ;;
		esac
	done
else
	while true; do
		read -p "Naga for X11 is currently installing, do you want to install for Wayland too ? (recommended) y/n" yn
		case $yn in
		[Yy]*)
			sh ./src/_installWayland.sh
			break
			;;
		[Nn]*) break ;;
		*) echo "Please answer yes or no." ;;
		esac
	done
fi

sudo cp -f src/naga.service /etc/systemd/system/

env | tee ~/.naga/envSetup >/dev/null
grep -qF 'env | tee ~/.naga/envSetup' ~/.profile || printf '\n%s\n' 'env | tee ~/.naga/envSetup > /dev/null' | tee -a ~/.profile >/dev/null
grep -qF 'sudo systemctl start naga' ~/.profile || printf '\n%s\n' 'sudo systemctl start naga > /dev/null' | tee -a ~/.profile >/dev/null

printf "Environment=DISPLAY=%s\n" "$DISPLAY" | sudo tee -a /etc/systemd/system/naga.service >/dev/null
printf "User=%s\n" "$USER" | sudo tee -a /etc/systemd/system/naga.service >/dev/null
printf "EnvironmentFile=/home/%s/.naga/envSetup\n" "$USER" | sudo tee -a /etc/systemd/system/naga.service >/dev/null
printf "WorkingDirectory=%s" "~" | sudo tee -a /etc/systemd/system/naga.service >/dev/null

sudo udevadm control --reload-rules && sudo udevadm trigger

sleep 0.5
sudo cat /etc/sudoers | grep -qxF "$USER ALL=(ALL) NOPASSWD:/bin/systemctl start naga" || printf "\n%s ALL=(ALL) NOPASSWD:/bin/systemctl start naga\n" "$USER" | sudo EDITOR='tee -a' visudo >/dev/null


sudo systemctl enable naga
sudo systemctl start naga

tput setaf 2
printf "Service started !\nStop with naga service stop\nStart with naga service start\n"
tput sgr0
printf "Star the repo here 😁 :\nhttps://github.com/lostallmymoney/Razer_Mouse_Linux\n"
