#!/bin/sh
if [ "$(id -u)" = "0" ]; then
	printf "This script must not be executed as root\n"
	exit 1
fi

sudo sh src/nagaKillroot.sh >/dev/null

printf "Copying files...\n"

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

WAYLANDTYPE=false

touch ~/.bash_aliases

check_connectivity() {
	ping -c 1 -W 2 8.8.8.8 >/dev/null 2>&1 && return 0
	command -v nslookup >/dev/null 2>&1 && nslookup google.com >/dev/null 2>&1 && return 0
	command -v nc >/dev/null 2>&1 && nc -z 8.8.8.8 53 >/dev/null 2>&1 && return 0
	return 1
}

warn_connectivity() {
	if ! check_connectivity; then
		printf "\033[0;33mWarning: No internet connection detected. Installation may fail.\033[0m\n"
		printf "Press ENTER to continue without internet or Ctrl+C to abort... "
		# shellcheck disable=SC2034
		read -r _
	fi
}

printf "Installing requirements...\n"

printf 'KERNEL=="event[0-9]*",SUBSYSTEM=="input",GROUP="razerInputGroup",MODE="640"' | sudo tee /etc/udev/rules.d/80-naga.rules >/dev/null

warn_connectivity

# shellcheck disable=SC2046
if [ "$(loginctl show-session $(loginctl | grep "$(whoami)" | awk '{print $1}') | grep -c "Type=wayland")" -ne 0 ]; then
	WAYLANDTYPE=true
	sh ./src/_installWayland.sh
	sed -i '/alias naga=/d' ~/.bash_aliases
	grep 'alias naga=' ~/.bash_aliases || printf "alias naga='nagaWayland'" | tee -a ~/.bash_aliases >/dev/null

	while true; do
		printf "\033[38;2;255;165;0mNaga for Wayland is currently installing,\n	do you want to install for X11 too ? (recommended) y/n : \033[0m"
		read -r yn
		case $yn in
		[Yy]*)
			sh ./src/_installX11.sh
			break
			;;
		[Nn]*) break ;;
		*) printf "Please answer yes or no.\n" ;;
		esac
	done
else
	sh ./src/_installX11.sh
	sed -i '/alias naga=/d' ~/.bash_aliases
	grep 'alias naga=' ~/.bash_aliases || printf "alias naga='nagaX11'" | tee -a ~/.bash_aliases >/dev/null

	while true; do
		printf "\033[38;2;255;165;0mNaga for X11 is currently installing,\n	do you want to install for Wayland too ? (recommended) y/n : \033[0m"
		read -r yn
		case $yn in
		[Yy]*)
			sh ./src/_installWayland.sh
			break
			;;
		[Nn]*) break ;;
		*) printf "Please answer yes or no.\n" ;;
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

# Add sudoers.d drop-in for passwordless systemctl start naga (best practice)
echo '# Allow systemctl start naga without password for the current user' | sudo tee /etc/sudoers.d/naga >/dev/null
echo "$USER ALL=(ALL) NOPASSWD: /bin/systemctl start naga" | sudo tee -a /etc/sudoers.d/naga >/dev/null
sudo chmod 0440 /etc/sudoers.d/naga
echo 'Added /etc/sudoers.d/naga for passwordless systemctl start naga.'

sudo systemctl enable naga
sudo systemctl restart naga

printf "\033[0;32mService started !\nStop with naga service stop\nStart with naga service start\033[0m\n"
printf "Star the repo here 😁 :\n"
printf "\033[0;35mhttps://github.com/lostallmymoney/Razer_Mouse_Linux\033[0m\n\n"

xdg-open https://github.com/lostallmymoney/Razer_Mouse_Linux >/dev/null 2>&1

if [ "$WAYLANDTYPE" = true ]; then
	printf "\033[0;31mRELOGGING NECESSARY\033[0m\n"
	printf "Press ENTER to log out..."
	# shellcheck disable=SC2034
	read -r _
	sudo pkill -HUP -u "$USER"
fi
