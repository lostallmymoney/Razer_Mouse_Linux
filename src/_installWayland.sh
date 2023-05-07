#!/bin/bash

sudo apt install -y g++ nano pkexec procps wget gnome-shell-extension-prefs dbus-x11 curl libdbus-1-dev

echo "Checking requirements..."

command -v g++ >/dev/null 2>&1 || {
    tput setaf 1
    echo >&2 "I require g++ but it's not installed! Aborting."
    exit 1
}

reset

echo "Compiling code..."
g++ -I/usr/lib/x86_64-linux-gnu/dbus-1.0/include -I/usr/include/dbus-1.0 ./src/nagaWayland.cpp -o ./src/nagaWayland -pthread -Ofast --std=c++2b -ldbus-1

if [ ! -f ./src/nagaWayland ]; then
    tput setaf 1
    echo "Error at compile! Ensure you have g++ installed. !!!Aborting!!!"
    exit 1; exit 1; exit 1
fi

sudo mv ./src/nagaWayland /usr/local/bin/
sudo chmod 755 /usr/local/bin/nagaWayland

echo "Installing dotool :"

wget https://git.sr.ht/~geb/dotool/archive/874650ad22dfb20ef24ab1f78666aa2e5178a0f0.tar.gz -O dotool.tar.gz
tar -xf dotool.tar.gz  > /dev/null
mv -f dotool-874650ad22dfb20ef24ab1f78666aa2e5178a0f0 dotool  > /dev/null
cd dotool
sudo sh install.sh > /dev/null
cd ..
#rm -rdf dotool
rm  -f dotool.tar.gz  > /dev/null


cp -rf ./src/window-calls-extended@hseliger.eu ~/.local/share/gnome-shell/extensions
gnome-extensions enable window-calls-extended@hseliger.eu

_dir="/home/$USER/.naga"
mkdir -p "$_dir"
sudo cp -r -n -v "keyMapWayland.txt" "$_dir"
sudo chown -R "root:root" "$_dir"/keyMapWayland.txt

sudo groupadd -f razerInputGroup

printf 'KERNEL=="uinput", GROUP="razerInputGroup"' | sudo tee /etc/udev/rules.d/80-nagaWayland.rules >/dev/null

grep 'alias naga=' ~/.bash_aliases || printf "alias naga='nagaWayland'" | tee -a ~/.bash_aliases > /dev/null
