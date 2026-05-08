# Razer Mouse Linux – Easy Macro Utility
A lightweight tool to map and manage macros for Razer Naga and other devices on Linux.

✅ Works on **Wayland** and **X11**
✅ Supports **Ubuntu, its flavors, and most other distros** (with dependencies installed)  
✅ Includes **per-app auto profiles**  

Want support for another device? Just [add the files](https://github.com/lostallmymoney/Razer_Mouse_Linux/issues/55) or [contact me](https://github.com/lostallmymoney/Razer_Mouse_Linux/discussions).  

---

## 🚀 Quick Install
Run this one-liner to install:  
```
sudo apt install unzip wget -y && cd ~ && rm -rf ~/Razer_Mouse_Linux.zip && wget https://codeload.github.com/lostallmymoney/Razer_Mouse_Linux/zip/refs/heads/master -O ~/Razer_Mouse_Linux.zip && unzip -o ~/Razer_Mouse_Linux.zip -d ~ && rm -rf ~/Razer_Mouse_Linux.zip && cd ~/Razer_Mouse_Linux-master && sh install.sh
```

Or, clone the repo and run:
```bash
sh install.sh
```

Then configure:
```bash
naga edit
```

If wayland or X11 aren't properly detected or if you want to manually install them you can :
```bash
sh install.sh X11
```
```bash
sh install.sh wayland
```
### Configuring Naga Service for Non-Razer Devices

The Naga service uses a udev rule based on USB vendor ID. By default it is set to Razer (`1532`), but it can be changed for other devices using :
```bash
naga vendor
```

---

## 🖱️ Supported Devices
Tested & confirmed 99% compatible with:

- **Naga Epic (pre-2014)** - (thanks unknown early contributor)
- **Naga (RZ01-0028)** – (thanks [khornem](https://github.com/khornem))  
- **Naga Chroma** – (thanks [felipeacsi](https://github.com/felipeacsi))  
- **Naga Molten** – (thanks noobxgockel)
- **Naga 2012 (RZ01-0058)** – (thanks [violet-fish](https://github.com/violet-fish), [brianfreytag](https://github.com/brianfreytag))  
- **Naga Chroma** – (thanks [ipsod](https://github.com/ipsod))  
- **Naga Trinity** – (thanks [haringsrob](https://github.com/haringsrob), [ws141](https://github.com/ws141))  
- **Naga Pro Wireless** – (thanks [Stibax](https://github.com/Stibax))  
- **Naga V2** – (thanks [ibarrick](https://github.com/ibarrick))  
- **Naga Left-Handed** – (thanks [Izaya-San](https://github.com/Izaya-San))  
- **Naga X** – (thanks [bgrabow](https://github.com/bgrabow))
- **Naga HyperSpeed** – (thanks [khornem](https://github.com/khornem)) (Bluetooth files still unknown, USB working)
- **Naga 2014** –
- **Naga Epic Chroma** –   

On theses systems : 
Ubuntu, Manjaro, Linux Mint, CentOS, ArchLinux, and more..

> **Disclaimer:** Make sure to have at least **2MB of free RAM** to run this daemon seamlessly.
---

## ⚙️ Commands
| Command | Description |
|---------|-------------|
| `naga start` | Starts the daemon |
| `naga stop` | Stops the daemon |
| `naga kill` | Kills the daemon |
| `naga enable` | Enables the daemon |
| `naga disable` | Disables the daemon |
| `naga edit ($EDITOR)` | Edits config (&restarts daemon). Examples: `naga edit` or `naga edit vim` |
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
### Wayland  
```bash
sudo apt install g++ nano pkexec procps wget gnome-shell-extension-prefs dbus-x11 curl libdbus-1-dev golang-go
```

### X11  
```bash
sudo apt install libx11-dev xdotool xinput g++ libxtst-dev libxmu-dev nano pkexec procps
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
Looking for higher-level building blocks? Check out [Functions, Loops & Contexts](#-functions-loops--contexts).
- `chmap` – Switch to another config
- `chmapRelease` – Switch config on key release
- `unlockChmap` – Unlocks auto window-based configs  
- `sleep` / `sleepRelease` – Put system to sleep
- `randomSleep` / `randomSleepRelease` – Sleep for a random duration (0 to N ms, where N is the argument)
- `string` / `stringRelease` – Type a literal string
- `key` – Press + release a key
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
- `runAndWriteRelease` – Run a command and write its output live to screen on release
- `runAndWriteRelease2` – Same as above, synchronous
- `keyClick` – Press key once on press
- `keyClickRelease` – Press key once on release
- `function` / `functionRelease` – Call a predefined function
- `loop` – Start/stop a predefined loop inline (synchronous)
- `loop2` – Start a predefined loop in a detached thread (asynchronous)
- Wayland-only `dotool` / `dotoolrelease` – Send the full command string directly to the nagaDotool pipe [see DoTool man page](https://git.sr.ht/~geb/dotool/tree/master/doc/dotool.1.scd)
- Wayland-only `click` – Press mouse click (left | center | right)
- Wayland-only `clickRelease` – Press mouse click (left | center | right) on release
- X11-only `xdotoolType` / `xdotoolTypeRelease` – Type a string via xdotool (fallback)
- X11-only `specialKey` – Press + release a special key
- X11-only `setWorkspace` – Switch workspace (via xdotool)
- X11-only `mousePosition` – Move mouse to `<x> <y>` position

**Special single-character press/release actions** (faster than xdotool, but no ctrl/media support yet):  
- X11-only `specialPressOnPress`  
- X11-only `specialPressOnRelease`  
- X11-only `specialReleaseOnPress`  
- X11-only `specialReleaseOnRelease`  

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
- Multiple actions per key are allowed; they run sequentially or async depending on choice.  
- Use `naga edit` to reload configs.  

---

## 🔄 Autorun
- Uses **systemd** for service management  
- Adds persistence lines to `~/.profile` and sudoers  

---

## 🧠 Functions, Loops & Contexts
These advanced building blocks let you share logic, create repeatable sequences, and keep large configs tidy.

### 🧩 Functions
- Define reusable combos once with `function=<name>` … `functionEnd`.
- Only press-phase actions are recorded; call them from a config using `function=<name>` or `functionRelease=<name>`.
- Functions can call other functions (nesting is allowed) for modular setups.

**Compatible press-phase actions** (usable in both functions and loops):
- `function` (invoke another function)
- `loop` / `loop2` (nested loops with the arguments `=start`, `=stop`, or negative numbers for fixed repeated loops `=-3`.)
- `chmap`
- `sleep`
- `run`
- `run2`
- `runandwrite`
- `runandwrite2`
- `unlockChmap`
- `keypressOnPress`
- `keyReleaseOnPress`
- `keyClick`
- `string`
- Wayland-only `click`
- X11-only `specialPressOnPress`
- X11-only `specialReleaseOnPress`

**Example:**
```txt
function=prepStandup
run=notify-send "Stand-up" "Time to share updates"
sleep=150
key=Super+N
sleep=120
string=Standup notes ready!
functionEnd

function=toggleMic
key=XF86AudioMicMute
functionEnd

# Press to prep workspace, release to mute/unmute the mic
4 - function=prepStandup
4 - functionRelease=toggleMic
```

### 🔁 Loops
- Wrap repeating sequences with `loop=<name>` … `loopEnd`.
- Bind them in configs via `loop=<name>` (runs inline) or `loop2=<name>` (runs in a detached thread).
- Optional arguments let you control behavior:
	- `loop=myLoop=start` (default) begins the loop.
	- `loop=myLoop=stop` stops it manually (auto-added on key release unless you pass a negative count).
	- `loop=myLoop=5` runs exactly 5 cycles; `loop=myLoop=-3` runs 3 cycles without waiting for stop.
	- `loop=myLoop=startOnRelease` begins loop on key release.
	- `loop=myLoop=stopOnRelease` stops loop on key release.
	- `loop=myLoop=toggle` toggles the loop on/off with each press.
	- `loop=myLoop=togglerelease` toggles the loop on/off with each release.
- **Only one instance per loop**: Each loop can have only one running instance at a time. Starting a loop while it's already running will stop the old instance and start a new one.
- **Loops can include other loops**: Inside a `loop=` or `loop2=` definition, you can call other loops using `loop=anotherLoop=start`, `loop=anotherLoop=stop`, or negative numbers like `loop=anotherLoop=-5` (for fixed-count loops without auto-stop).
- **All press-phase actions compatible with functions are also compatible with loops**, including nested `loop` / `loop2` calls.

**Example:**
```txt
loop=burstRotation
key=Ctrl+1
sleep=150
key=Ctrl+2
loopEnd

loop=autoFarm
loop=burstRotation=-2    # negative count: run 2 cycles without auto-stop
sleep=500
loopEnd

3 - loop=burstRotation        # hold to spam the rotation
4 - loop2=burstRotation=5     # run 5 cycles in background
5 - loop=autoFarm             # nested loop example
```

### 🗂️ Contexts
- Use `context=<name>` … `contextEnd` to store shared lines (they can reference other contexts).
- Inside a config, drop `context=<name>` and every line stored in that context is injected in place.
- Great for repeating key layouts across multiple profiles or layering optional behaviors.

**Example:**
```txt
context=mediaOverlay
1 - key=XF86AudioPlay
2 - key=XF86AudioNext
contextEnd

context=focusShortcuts
context=mediaOverlay
3 - key=Super+Left
4 - key=Super+Right
contextEnd

config=focusMode
context=focusShortcuts
5 - run=notify-send "Focus mode engaged"
configEnd
```

---

## ❌ Uninstall
```bash
naga uninstall
```
