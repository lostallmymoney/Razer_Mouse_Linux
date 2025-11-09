#!/bin/sh

if [ "$(id -u)" = "0" ]; then
    printf "\033[0;31mThis script must not be executed as root\033[0m\n"
    exit 1
fi

printf "Installing requirements...\n"


# Try to detect and use the available package manager
if command -v apt >/dev/null 2>&1; then
    sudo apt install -y libx11-dev xdotool xinput g++ libxtst-dev libxmu-dev nano pkexec procps libglib2.0-dev scdoc || { printf "\033[0;31mFailed while installing apt packages.\033[0m\n" >&2; exit 1; }
elif command -v zypper >/dev/null 2>&1; then
    sudo zypper --non-interactive install gcc-c++ nano polkit procps wget gnome-shell-extension-prefs dbus-1 curl dbus-1-devel libxkbcommon-devel go scdoc || { printf "\033[0;31mFailed while installing zypper packages.\033[0m\n" >&2; exit 1; }
elif command -v dnf >/dev/null 2>&1; then
    sudo dnf install -y libX11-devel xdotool xinput gcc-c++ libXtst-devel libXmu-devel nano polkit procps-ng glib2-devel scdoc || { printf "\033[0;31mFailed while installing dnf packages.\033[0m\n" >&2; exit 1; }
elif command -v pacman >/dev/null 2>&1; then
    sudo pacman -Sy --noconfirm libx11 xdotool xorg-xinput gcc libxtst libxmu nano polkit procps-ng glib2 scdoc || { printf "\033[0;31mFailed while installing pacman packages.\033[0m\n" >&2; exit 1; }
else
    printf "\033[0;31mNo supported package manager found (apt, dnf, zypper, pacman).\033[0m\n" >&2
    exit 1
fi

printf "Checking requirements...\n"

command -v xdotool >/dev/null 2>&1 || {
    printf "\033[0;31mI require xdotool but it's not installed! Aborting.\033[0m\n"
    exit 1
}
command -v xinput >/dev/null 2>&1 || {
    printf "\033[0;31mI require xinput but it's not installed! Aborting.\033[0m\n"
    exit 1
}
command -v g++ >/dev/null 2>&1 || {
    printf "\033[0;31mI require g++ but it's not installed! Aborting.\033[0m\n"
    exit 1
}

clear -x

printf "Compiling code...\n"
g++ ./src/nagaX11.cpp -o ./src/nagaX11 -pthread -Ofast --std=c++23 -lX11 -lXtst -lXmu

if [ ! -f ./src/nagaX11 ]; then
    printf "\033[0;31mError at compile! Ensure you have g++ installed. !!!Aborting!!!\033[0m\n"
    exit 1
fi
printf "Compiled nagaX11...\n"

sudo mv ./src/nagaX11 /usr/local/bin/
sudo chmod 755 /usr/local/bin/nagaX11

sudo cp -f ./src/nagaXinputStart.sh /usr/local/bin/Naga_Linux/
sudo chmod 755 /usr/local/bin/Naga_Linux/nagaXinputStart.sh

_dir="/home/$USER/.naga"
mkdir -p "$_dir"
sudo cp -r --update=none -v "keyMap.txt" "$_dir"
sudo chown -R "root:root" "$_dir"/keyMap.txt