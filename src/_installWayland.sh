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


sudo gnome-extensions disable window-calls-extended@hseliger.eu >/dev/null
sudo gnome-extensions uninstall -q window-calls-extended@hseliger.eu

#BYPASS to enable the extension on Gnome Shell 49 (Ubuntu 25.10 only for now)

EXT_ZIP="./src/window-calls-extended@hseliger.eu.shell-extension.zip"
EXT_DIR="$HOME/.local/share/gnome-shell/extensions/window-calls-extended@hseliger.eu"

# 1️⃣ Create the extension folder
mkdir -p "$EXT_DIR"

# 2️⃣ Unzip extension into the folder
unzip -o "$EXT_ZIP" -d "$EXT_DIR"

# 3️⃣ Patch metadata.json to include GNOME Shell 49
METADATA="$EXT_DIR/metadata.json"
if [ -f "$METADATA" ]; then
    # Add 49 to shell-version if not present
    if ! grep -q '"49"' "$METADATA"; then
        sed -i '/"shell-version": \[/ {:a; N; /\]/!ba; /"49"/! s/\("\)\n  \]/\1,\n    "49"\n  ]/}' \
            ~/.local/share/gnome-shell/extensions/window-calls-extended@hseliger.eu/metadata.json
    fi
else
    echo "Error: metadata.json not found in $EXT_DIR"
    exit 1
fi

# 4️⃣ Fix permissions
chown -R "$USER:$USER" "$EXT_DIR"
chmod -R 644 "$EXT_DIR"/*.js "$EXT_DIR"/metadata.json 2>/dev/null || true
find "$EXT_DIR" -type d -exec chmod 755 {} \;

# 5️⃣ Enable the extension
gnome-extensions enable window-calls-extended@hseliger.eu

#end of BYPASS


_dir="/home/$USER/.naga"
mkdir -p "$_dir"
sudo cp -r --update=none -v "keyMapWayland.txt" "$_dir"
sudo chown -R "root:root" "$_dir"/keyMapWayland.txt

sudo groupadd -f razerInputGroup

printf 'KERNEL=="uinput", GROUP="razerInputGroup"' | sudo tee /etc/udev/rules.d/80-nagaWayland.rules >/dev/null


clear -x
