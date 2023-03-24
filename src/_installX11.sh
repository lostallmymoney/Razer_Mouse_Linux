#!/bin/bash

if [ "$(id -u)" = "0" ]; then
    echo "This script must not be executed as root"
    exit 1
fi

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
g++ ./src/nagaX11.cpp -o ./src/nagaX11 -pthread -Ofast --std=c++2b -lX11 -lXtst -lXmu

if [ ! -f ./src/nagaX11 ]; then
    tput setaf 1
    echo "Error at compile! Ensure you have g++ installed. !!!Aborting!!!"
    exit 1
fi

sudo mv ./src/nagaX11 /usr/local/bin/
sudo chmod 755 /usr/local/bin/nagaX11


_dir="/home/$USER/.naga"
mkdir -p "$_dir"
sudo cp -r -n -v "keyMap.txt" "$_dir"
sudo chown -R "root:root" "$_dir"/keyMap.txt


grep 'alias naga=' ~/.bash_aliases  || printf "alias naga='nagaX11'" | tee -a ~/.bash_aliases > /dev/null