#!/bin/bash

sudo apt install -y g++ nano pkexec procps wget gnome-shell-extension-prefs dbus-x11 curl libdbus-1-dev libxkbcommon-dev golang-go scdoc

echo "Checking requirements..."

command -v g++ >/dev/null 2>&1 || {
    tput setaf 1
    echo >&2 "I require g++ but it's not installed! Aborting."
    exit 1
}

clear -x

echo "Compiling code..."
g++ -I/usr/lib/x86_64-linux-gnu/dbus-1.0/include -I/usr/include/dbus-1.0 ./src/nagaWayland.cpp -o ./src/nagaWayland -pthread -Ofast --std=c++2b -ldbus-1

if [ ! -f ./src/nagaWayland ]; then
    tput setaf 1
    echo "Error at compile! Ensure you have g++ installed. !!!Aborting!!!"
    exit 1; exit 1; exit 1
fi

sudo mv ./src/nagaWayland /usr/local/bin/
sudo chmod 755 /usr/local/bin/nagaWayland

printf "Installing dotool :\n"

sleep 0.1
wget https://git.sr.ht/~geb/dotool/archive/b5812c001daeeaff1f259031661e47f3a612220c.tar.gz -O dotool.tar.gz
tar -xf dotool.tar.gz  > /dev/null
mv -fu dotool-b5812c001daeeaff1f259031661e47f3a612220c dotool > /dev/null
sleep 0.1
cd dotool
./build.sh
sudo ./build.sh install
cd ..
sleep 0.1
rm  -rf dotool*  > /dev/null
sleep 0.1

clear -x

sudo gnome-extensions disable window-calls-extended@hseliger.eu > /dev/null
sudo gnome-extensions uninstall -q window-calls-extended@hseliger.eu
gnome-extensions install ./src/window-calls-extended@hseliger.eu.shell-extension.zip --force

_dir="/home/$USER/.naga"
mkdir -p "$_dir"
sudo cp -r -n -v "keyMapWayland.txt" "$_dir"
sudo chown -R "root:root" "$_dir"/keyMapWayland.txt

sudo groupadd -f razerInputGroup

printf 'KERNEL=="uinput", GROUP="razerInputGroup"' | sudo tee /etc/udev/rules.d/80-nagaWayland.rules >/dev/null
