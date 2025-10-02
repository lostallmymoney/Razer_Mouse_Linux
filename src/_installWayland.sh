#!/bin/sh

printf "Installing requirements...\n"

sudo apt install -y g++ nano pkexec procps wget gnome-shell-extension-prefs dbus-x11 curl libdbus-1-dev libxkbcommon-dev golang-go scdoc

printf "Checking requirements...\n"

command -v g++ >/dev/null 2>&1 || {
    printf "\033[0;31mI require g++ but it's not installed! Aborting.\033[0m\n"
    exit 1
}

clear -x

printf "Compiling code...\n"
g++ -I/usr/lib64/dbus-1.0/include -I/usr/lib/x86_64-linux-gnu/dbus-1.0/include -I/usr/include/dbus-1.0 ./src/nagaWayland.cpp -o ./src/nagaWayland -pthread -Ofast --std=c++23 -ldbus-1

if [ ! -f ./src/nagaWayland ]; then

    printf "\033[0;31mError at compile! Ensure you have g++ installed. !!!Aborting!!!\033[0m\n"
    exit 1
fi

sudo mv ./src/nagaWayland /usr/local/bin/
sudo chmod 755 /usr/local/bin/nagaWayland

printf "Installing dotool :\n"

sleep 0.1
wget https://git.sr.ht/~geb/dotool/archive/1fea2c210fcb25522c4ff5c900ef7522c197f5ed.tar.gz -O dotool.tar.gz
tar -xf dotool.tar.gz >/dev/null
mv -fu dotool-1fea2c210fcb25522c4ff5c900ef7522c197f5ed dotool >/dev/null
sleep 0.1
cd dotool || exit 1
./build.sh
sudo ./build.sh install
cd ..
sleep 0.1
rm -rf dotool* >/dev/null
sleep 0.1

clear -x

sudo gnome-extensions disable window-calls-extended@hseliger.eu >/dev/null
sudo gnome-extensions uninstall -q window-calls-extended@hseliger.eu
gnome-extensions install ./src/window-calls-extended@hseliger.eu.shell-extension.zip --force

_dir="/home/$USER/.naga"
mkdir -p "$_dir"
sudo cp -r --update=none -v "keyMapWayland.txt" "$_dir"
sudo chown -R "root:root" "$_dir"/keyMapWayland.txt

sudo groupadd -f razerInputGroup

printf 'KERNEL=="uinput", GROUP="razerInputGroup"' | sudo tee /etc/udev/rules.d/80-nagaWayland.rules >/dev/null
