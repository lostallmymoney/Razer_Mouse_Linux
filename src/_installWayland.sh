#!/bin/sh

printf "Installing requirements...\n"

CXX="${NAGA_CXX:-g++}"

# The clang package is named "clang" on apt, zypper, dnf and pacman.
# Empty when building with g++, so the install lines below stay unchanged.
clang_pkg=""
if [ "$CXX" = "clang++" ]; then
    clang_pkg="clang"
fi

# Try to detect and use the available package manager
if command -v apt >/dev/null 2>&1; then
    # shellcheck disable=SC2086
    sudo apt install -y \
        g++ nano pkexec procps wget gnome-shell-extension-manager curl \
        libdbus-1-dev libxkbcommon-dev golang-go scdoc $clang_pkg || {
        printf "\033[0;31mAPT install failed. Aborting.\033[0m\n" >&2
        exit 1
    }
elif command -v zypper >/dev/null 2>&1; then
    # shellcheck disable=SC2086
    for pkg in gcc-c++ nano polkit procps wget gnome-extensions-app dbus-1 curl libdbus-1-devel libxkbcommon-devel go scdoc $clang_pkg; do
        sudo zypper --non-interactive install "$pkg" >/dev/null 2>&1 || {
            printf "\033[0;33mSkipping (zypper): %s\033[0m\n" "$pkg"
        }
    done
elif command -v dnf >/dev/null 2>&1; then
    # shellcheck disable=SC2086
    for pkg in gcc-c++ nano polkit procps-ng wget gnome-extensions-app curl dbus-devel libxkbcommon-devel golang scdoc $clang_pkg; do
        sudo dnf install -y "$pkg" >/dev/null 2>&1 || {
            printf "\033[0;33mSkipping (dnf): %s\033[0m\n" "$pkg"
        }
    done
elif command -v pacman >/dev/null 2>&1; then
    # shellcheck disable=SC2086
    for pkg in base-devel nano polkit procps-ng wget gnome-extensions-app dbus curl libxkbcommon go scdoc $clang_pkg; do
        sudo pacman -S --noconfirm "$pkg" >/dev/null 2>&1 || {
            printf "\033[0;33mSkipping (pacman): %s\033[0m\n" "$pkg"
        }
    done
else
    printf "\033[0;31mNo supported package manager found (apt, dnf, zypper, pacman).\033[0m\n" >&2
    exit 1
fi

printf "Checking requirements...\n"

command -v "$CXX" >/dev/null 2>&1 || {
    printf "\033[0;31mI require %s but it's not installed! Aborting.\033[0m\n" "$CXX"
    exit 1
}

clear -x

printf "Compiling code with %s...\n" "$CXX"
"$CXX" -I/usr/lib64/dbus-1.0/include -I/usr/lib/x86_64-linux-gnu/dbus-1.0/include -I/usr/include/dbus-1.0 ./src/nagaWayland.cpp -o ./src/nagaWayland -pthread -O3 -march=native -s --std=c++23 -ldbus-1

if [ ! -f ./src/nagaWayland ]; then

    printf "\033[0;31mError at compile! Ensure you have %s installed. !!!Aborting!!!\033[0m\n" "$CXX"
    exit 1
fi
printf "Compiled nagaWayland...\n"

sudo mv ./src/nagaWayland /usr/local/bin/
sudo chmod 755 /usr/local/bin/nagaWayland

# Everything this installer downloads (dotool, Focus Class Fetcher) goes into
# ./temp and is kept after the install; the folder is purged before each install.
rm -rf temp
mkdir -p temp

printf "Installing dotool :\n"

sleep 0.1
wget https://git.sr.ht/~geb/dotool/archive/27ec57e52012ffcb1e8c6419278fbf4b6311e2c2.tar.gz -O temp/dotool.tar.gz
tar -xf temp/dotool.tar.gz -C temp >/dev/null
mv -fu temp/dotool-27ec57e52012ffcb1e8c6419278fbf4b6311e2c2 temp/dotool >/dev/null
sleep 0.1
cd temp/dotool || exit 1
GOFLAGS="${GOFLAGS:+$GOFLAGS }-buildvcs=false" ./build.sh
dotool_stage="$(mktemp -d)"
if [ ! -d "$dotool_stage" ]; then
    printf "\033[0;31mFailed to create staging directory for dotool.\033[0m\n" >&2
    exit 1
fi
cleanup_dotool_stage() {
    rm -rf "$dotool_stage"
}
trap cleanup_dotool_stage EXIT INT TERM

if ! GOFLAGS="${GOFLAGS:+$GOFLAGS }-buildvcs=false" DOTOOL_DESTDIR="$dotool_stage" DOTOOL_BINDIR=bin DOTOOL_UDEV_RULES_DIR=udev ./build.sh install; then
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
sed -i 's/\${DOTOOL_PIPE:-\/tmp\/dotool-pipe}/\${DOTOOL_PIPE:-\/run\/nagaProtected\/nagadotool-pipe}/g' "$dotool_stage/bin/dotoold"
# shellcheck disable=SC2016
sed -i 's/\/tmp\/dotool-pipe/\/run\/nagaProtected\/nagadotool-pipe/g' "$dotool_stage/bin/dotoolc"

sudo install -Dm750 -o root -g razerInputGroup "$dotool_stage/bin/dotool" /usr/local/bin/nagaDotool
sudo install -Dm750 -o root -g razerInputGroup "$dotool_stage/bin/dotoolc" /usr/local/bin/nagaDotoolc
sudo install -Dm750 -o root -g razerInputGroup "$dotool_stage/bin/dotoold" /usr/local/bin/nagaDotoold

# Create systemd-tmpfiles.d config for persistent /run/nagaProtected
cat <<EOF | sudo tee /etc/tmpfiles.d/nagaProtected.conf >/dev/null
d /run/nagaProtected 0770 root razerInputGroup -
EOF
sudo systemd-tmpfiles --create

cleanup_dotool_stage
trap - EXIT INT TERM
dotool_stage=""

cd ../..
sleep 0.1

# Window class detection comes from the standalone Focus Class Fetcher
# extension. It is optional: on platforms where GNOME extensions do not
# work, a warning is printed and the install continues without it.
install_focus_class_fetcher() {
	fetcher_dir="temp/focus-class-fetcher"
	if command -v git >/dev/null 2>&1; then
		git clone --depth 1 https://github.com/lostallmymoney/focus-class-fetcher.git "$fetcher_dir" || return 1
	else
		# No git: download the tarball, like dotool above.
		for branch in main master; do
			if wget "https://github.com/lostallmymoney/focus-class-fetcher/archive/refs/heads/$branch.tar.gz" -O "$fetcher_dir.tar.gz"; then
				break
			fi
			rm -f "$fetcher_dir.tar.gz"
		done
		[ -f "$fetcher_dir.tar.gz" ] || return 1
		tar -xf "$fetcher_dir.tar.gz" -C temp >/dev/null || return 1
		mv -fu "$fetcher_dir"-*/ "$fetcher_dir" || return 1
	fi
	sh "$fetcher_dir/install.sh" || return 1
}

printf "Installing Focus Class Fetcher extension:\n"
if ! install_focus_class_fetcher; then
	printf "\033[0;33m"
	printf "================================================================\n"
	printf "WARNING: the Focus Class Fetcher extension could not be installed.\n"
	printf "Window-class detection is unavailable on this system: per-window\n"
	printf "keymaps will not work. You can install it manually later from\n"
	printf "https://github.com/lostallmymoney/focus-class-fetcher\n"
	printf "================================================================\n"
	printf "\033[0m\n"
fi

_dir="/home/$USER/.naga"
mkdir -p "$_dir"
sudo cp -r --update=none -v "keyMapWayland.txt" "$_dir"
sudo chown -R "root:root" "$_dir"/keyMapWayland.txt

sudo sh -c '> /etc/udev/rules.d/80-nagaWayland.rules'
printf 'KERNEL=="uinput", GROUP="razerInputGroup", MODE="0620", OPTIONS+="static_node=uinput"' | sudo tee /etc/udev/rules.d/80-nagaWayland.rules >/dev/null

clear -x
