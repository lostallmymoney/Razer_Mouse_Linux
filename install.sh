#!/bin/sh

cleanup() {
    stty sane 2>/dev/null || true
}

abort_install() {
    printf "\n\033[0;31mInstallation aborted.\033[0m\n" >&2
    cleanup
    exit "$1"
}

on_interrupt() {
    printf "\n\033[0;31mInterrupted by user.\033[0m\n" >&2
    abort_install 130
}

trap on_interrupt INT
trap cleanup EXIT

if [ "$(id -u)" = "0" ]; then
    printf "This script must not be executed as root\n"
    abort_install 1
fi

sudo sh src/nagaKillroot.sh >/dev/null

printf "Copying files...\n"

sudo mkdir -p /usr/local/bin/Naga_Linux

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

# Udev rule for Razer input devices (side/extra buttons)
sudo sh -c '> /etc/udev/rules.d/80-naga.rules'
printf 'KERNEL=="event[0-9]*",SUBSYSTEM=="input", GROUP="razerInputGroup", MODE="0660"' | sudo tee /etc/udev/rules.d/80-naga.rules >/dev/null


warn_connectivity

run_sub_install() {
    script_path="$1"
    if ! sh "$script_path"; then
        printf "\033[0;31mFailed while running %s.\033[0m\n" "$script_path" >&2
        abort_install 1
    fi
}


case "$1" in
    X11|x11)
        run_sub_install ./src/_installX11.sh
    ;;
    Wayland|wayland)
        run_sub_install ./src/_installWayland.sh
    ;;
    *)
        # shellcheck disable=SC2046
        if [ "$(loginctl show-session $(loginctl | grep "$(whoami)" | awk '{print $1}') | grep -c "Type=wayland")" -ne 0 ]; then
            WAYLANDTYPE=true
            run_sub_install ./src/_installWayland.sh
            sed -i '/alias naga=/d' ~/.bash_aliases
            grep 'alias naga=' ~/.bash_aliases || printf "alias naga='nagaWayland'" | tee -a ~/.bash_aliases >/dev/null
        else
            run_sub_install ./src/_installX11.sh
            sed -i '/alias naga=/d' ~/.bash_aliases
            grep 'alias naga=' ~/.bash_aliases || printf "alias naga='nagaX11'" | tee -a ~/.bash_aliases >/dev/null
        fi
    ;;
esac

# Add naga.service config lines
sudo cp -f ./src/naga.service /etc/systemd/system/naga.service

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
sudo rm /etc/sudoers.d/naga 2>/dev/null
echo '# Allow systemctl start naga without password for the current user' | sudo tee /etc/sudoers.d/naga >/dev/null
echo "$USER ALL=(ALL) NOPASSWD: /usr/bin/systemctl start naga" | sudo tee -a /etc/sudoers.d/naga >/dev/null
echo "$USER ALL=(ALL) NOPASSWD: /usr/bin/systemctl restart naga" | sudo tee -a /etc/sudoers.d/naga >/dev/null
echo "$USER ALL=(ALL) NOPASSWD: /usr/bin/systemctl stop naga" | sudo tee -a /etc/sudoers.d/naga >/dev/null
echo "$USER ALL=(ALL) NOPASSWD: /usr/bin/systemctl enable naga" | sudo tee -a /etc/sudoers.d/naga >/dev/null
echo "$USER ALL=(ALL) NOPASSWD: /usr/bin/systemctl disable naga" | sudo tee -a /etc/sudoers.d/naga >/dev/null

sudo chmod 0440 /etc/sudoers.d/naga
echo 'Added /etc/sudoers.d/naga for passwordless systemctl start naga.'
sudo visudo -c

sudo systemctl enable naga
sudo systemctl restart naga

printf "\033[0;32mService started !\nStop with naga service stop\nStart with naga service start\033[0m\n"
printf "Star the repo here 😁 :\n"
printf "\033[0;35mhttps://github.com/lostallmymoney/Razer_Mouse_Linux\033[0m\n\n"

(setsid xdg-open https://github.com/lostallmymoney/Razer_Mouse_Linux >/dev/null 2>&1) &
if [ "$WAYLANDTYPE" = true ]; then
    printf "\033[0;31mRELOGGING NECESSARY (for auto profiles.. Press ctrl+c to skip) \033[0m\n"
    printf "Press ENTER to log out (reboot)..."
    # shellcheck disable=SC2034
    read -r _
    sudo pkill -HUP -u "$USER"
fi