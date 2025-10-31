# Razer Mouse Linux – Easy Macro Utility
A lightweight tool to map and manage macros for Razer Naga and other devices on Linux.  

✅ Works on **Wayland** and **X11**
✅ Supports **Ubuntu, its flavors, and most other distros** (with dependencies installed)  
✅ Includes **per-app auto profiles**  

Want support for another device? Just add the files or [contact me](#).  

---

## 🚀 Quick Install
Run this one-liner to install:  
```
sudo apt install unzip wget -y && cd ~ && wget https://codeload.github.com/lostallmymoney/Razer_Mouse_Linux/zip/refs/heads/master -O ~/Razer_Mouse_Linux.zip && unzip -o ~/Razer_Mouse_Linux.zip -d ~ && rm -rf ~/Razer_Mouse_Linux.zip && cd ~/Razer_Mouse_Linux-master && ./install.sh
```

Or, clone the repo and run:
```bash
sh install.sh
```

Then configure:
```bash
naga edit
```

---

## 🖱️ Supported Devices
Tested & confirmed 99% compatible with:

- **Naga Epic Chroma** – CentOS 7  
- **Naga Epic (pre-2014)** – Ubuntu 14.04–15.10  
- **Naga (RZ01-0028)** – Ubuntu 14.04 (thanks [khornem](https://github.com/khornem))  
- **Naga Chroma** – Manjaro (thanks [felipeacsi](https://github.com/felipeacsi))  
- **Naga Molten** – Linux Mint 17.02 (thanks noobxgockel)  
- **Naga 2012 (RZ01-0058)** – Arch, Ubuntu 16.04 (thanks [violet-fish](https://github.com/violet-fish), [brianfreytag](https://github.com/brianfreytag))  
- **Naga Chroma** – Linux Mint KDE 18.1 (thanks [ipsod](https://github.com/ipsod))  
- **Naga Trinity** – (thanks [haringsrob](https://github.com/haringsrob), [ws141](https://github.com/ws141))  
- **Naga Pro Wireless** – (thanks [Stibax](https://github.com/Stibax))  
- **Naga V2** – (thanks [ibarrick](https://github.com/ibarrick))  
- **Naga Left-Handed** – (thanks [Izaya-San](https://github.com/Izaya-San))  
- **Naga X** – (thanks [bgrabow](https://github.com/bgrabow))  
- **Naga 2014** – Ubuntu  

> **Disclaimer:** Make sure to have at least **300KB of free RAM** to run this daemon seamlessly.
---

## ⚙️ Commands
| Command | Description |
|---------|-------------|
| `naga start` | Start the daemon |
| `naga stop` | Stop the daemon |
| `naga edit ($EDITOR)` | Edit config (auto-restarts daemon). Example: `naga edit vim` |
| `naga debug` | Show logs (realtime, pass args to override `journalctl`) |
| `naga fix` | Restart USB services |
| `naga uninstall` | Remove the tool completely |
| `naga` | Show help |
| `naga serviceHelper ($CONFIG)` | Manual config switching (requires udev rule + disabled service) |

Easily map keys with:
```bash
naga edit
```

---

## 📦 Dependencies
### X11  
```bash
sudo apt install libx11-dev xdotool xinput g++ libxtst-dev libxmu-dev nano pkexec procps
```

### Wayland  
```bash
sudo apt install g++ nano pkexec procps wget gnome-shell-extension-prefs dbus-x11 curl libdbus-1-dev golang-go
```

If something fails to compile on your distro → install equivalents of these packages.  

---

## 🔧 Configuration
The configuration file is stored in `~/.naga/keyMap.txt`.  

**Basic syntax:**
```
config=<configName>
<keyNumber> - <option>=<command>
<keyNumber> - <option>=<command>
<keyNumber> - <option>=<command>
configEnd
```

- `<keyNumber>`: 1–14 (12 keypad buttons + 2 top buttons)  
- `<option>`: action type (see full list below)  
- `<command>`: string, key, or shell command  

---

### 📚 Full Option Reference
- `chmap` – Switch to another config  
- `chmapRelease` – Switch config on key release  
- `unlockChmap` – Unlocks auto window-based configs  
- `sleep` / `sleepRelease` – Put system to sleep  
- `string` / `stringRelease` – Type a literal string (no xdotool)  
- `xdotoolType` / `xdotoolTypeRelease` – Type a string via xdotool (fallback)  
- `key` – Press + release a key  
- `specialKey` – Press + release a special key  
- `keyPressOnPress` – Hold key on button press  
- `keyReleaseOnRelease` – Release key on button release  
- `keyPressOnRelease` – Hold key on button release  
- `keyReleaseOnPress` – Release key on button press  
- `run` – Run a shell command asynchronously  
- `run2` – Run a shell command synchronously  
- `runRelease` – Run command on release  
- `runRelease2` – Same as above, synchronous  
- `runAndWrite` – Run a command and write its output live to screen  
- `runAndWrite2` – Same as above, synchronous  
- `setWorkspace` – Switch workspace (via xdotool)  
- `mousePosition` – Move mouse to `<x> <y>` position  
- `keyClick` – Press key once on press  
- `keyClickRelease` – Press key once on release  
- `click` – Press mouse click (left | center | right) (Wayland only)
- `clickRelease` – Press mouse click (left | center | right) on release (Wayland only)

**Special single-character press/release actions** (faster than xdotool, but no ctrl/media support yet, ONLY ON X11):  
- `specialPressOnPress`  
- `specialPressOnRelease`  
- `specialReleaseOnPress`  
- `specialReleaseOnRelease`  

👉 For valid key names, see [X11 keysym list](https://cgit.freedesktop.org/xorg/proto/x11proto/plain/keysymdef.h) (remove `XK_` prefix).  

---

### Example Config
```txt
# Default config
config=defaultConfig
1 - key=XF86AudioPlay
3 - chmap=WoWConfig
4 - run=notify-send 'Button #4' 'Pressed'
configEnd

# Custom WoW profile
config=WoWConfig
1 - run=sh ~/hacks.sh
2 - chmap=defaultConfig
configEnd
```

📌 Notes:  
- If `~/.naga/keyMap.txt` is missing, the daemon **won’t start** (installer copies an example).  
- Multiple actions per key are allowed; they run sequentially.  
- Use `naga edit` to reload configs.  

---

## 🔄 Autorun
- Uses **systemd** for service management  
- Adds persistence lines to `~/.profile` and sudoers  

---

## ❌ Uninstall
```bash
naga uninstall
```
