#!/bin/sh

printf "Installing requirements...\n"

if ! sudo apt install -y g++ nano pkexec procps wget gnome-shell-extension-prefs dbus-x11 curl libdbus-1-dev libxkbcommon-dev golang-go scdoc; then
    printf "\033[0;31mFailed while installing apt packages.\033[0m\n" >&2
    exit 1
fi

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
printf "Compiled nagaWayland...\n"

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
dotool_stage="$(mktemp -d)"
if [ ! -d "$dotool_stage" ]; then
    printf "\033[0;31mFailed to create staging directory for dotool.\033[0m\n" >&2
    exit 1
fi
cleanup_dotool_stage() {
    rm -rf "$dotool_stage"
}
trap cleanup_dotool_stage EXIT INT TERM

if ! DOTOOL_DESTDIR="$dotool_stage" DOTOOL_BINDIR=bin DOTOOL_UDEV_RULES_DIR=udev ./build.sh install; then
    printf "\033[0;31mFailed to stage dotool binaries.\033[0m\n" >&2
    exit 1
fi

if [ ! -x "$dotool_stage/bin/dotool" ] || [ ! -x "$dotool_stage/bin/dotoolc" ] || [ ! -x "$dotool_stage/bin/dotoold" ]; then
    printf "\033[0;31mStaged dotool binaries are missing.\033[0m\n" >&2
    exit 1
fi

#Path/Convert dotool into nagaDotool

sed -i 's/dotool "\$@"/nagaDotool "\$@"/' "$dotool_stage/bin/dotoold"
# shellcheck disable=SC2016
sed -i 's/\${DOTOOL_PIPE:-\/tmp\/dotool-pipe}/\${DOTOOL_PIPE:-\/run\/.nagaProtected\/nagadotool-pipe}/g' "$dotool_stage/bin/dotoold"
# shellcheck disable=SC2016
sed -i 's/\/tmp\/dotool-pipe/\/run\/.nagaProtected\/nagadotool-pipe/g' "$dotool_stage/bin/dotoolc"

sudo install -Dm750 -o root -g razerInputGroup "$dotool_stage/bin/dotool" /usr/local/bin/nagaDotool
sudo install -Dm750 -o root -g razerInputGroup "$dotool_stage/bin/dotoolc" /usr/local/bin/nagaDotoolc
sudo install -Dm750 -o root -g razerInputGroup "$dotool_stage/bin/dotoold" /usr/local/bin/nagaDotoold

# Create systemd-tmpfiles.d config for persistent /run/.nagaProtected
cat <<EOF | sudo tee /etc/tmpfiles.d/nagaProtected.conf >/dev/null
d /run/.nagaProtected 0770 root razerInputGroup -
EOF
sudo systemd-tmpfiles --create


cleanup_dotool_stage
trap - EXIT INT TERM
dotool_stage=""

cd ..
sleep 0.1
rm -rf dotool* >/dev/null
sleep 0.1


sudo gnome-extensions disable window-calls-extended@hseliger.eu >/dev/null
sudo gnome-extensions uninstall -q window-calls-extended@hseliger.eu

EXT_ZIP="./src/window-calls-extended@hseliger.eu.shell-extension.zip"
EXT_DIR="$HOME/.local/share/gnome-shell/extensions/window-calls-extended@hseliger.eu"

# 1️⃣ Create the extension folder
mkdir -p "$EXT_DIR"

# 2️⃣ Unzip extension into the folder
unzip -o "$EXT_ZIP" -d "$EXT_DIR"


# 4️⃣ Fix permissions
chown -R "$USER:$USER" "$EXT_DIR"
chmod -R 644 "$EXT_DIR"/*.js "$EXT_DIR"/metadata.json 2>/dev/null || true
find "$EXT_DIR" -type d -exec chmod 755 {} \;

# 5️⃣ Enable the extension
gnome-extensions enable window-calls-extended@hseliger.eu


_dir="/home/$USER/.naga"
mkdir -p "$_dir"
mkdir -p "$_dir/protected"
sudo chown root:razerInputGroup "$_dir/protected"
sudo chmod 770 "$_dir/protected"
sudo cp -r --update=none -v "keyMapWayland.txt" "$_dir"
sudo chown -R "root:root" "$_dir"/keyMapWayland.txt

sudo sh -c '> /etc/udev/rules.d/80-nagaWayland.rules'
printf 'KERNEL=="uinput", GROUP="razerInputGroup", MODE="0620", OPTIONS+="static_node=uinput"' | sudo tee /etc/udev/rules.d/80-nagaWayland.rules >/dev/null


clear -x
