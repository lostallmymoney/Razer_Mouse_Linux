# Razer Mouse Linux (Easy macro utility)
..also can accept other devices by adding the files. Contact me to add devices.     
Currently supporting X11 AND WAYLAND.(Ubuntu & flavors & anything you can install the dependencies on)     
Now with app auto profiles !   
## INSTALL :
In one command :     
`sudo apt install unzip wget -y && wget https://codeload.github.com/lostallmymoney/Razer_Mouse_Linux/zip/refs/heads/master -O Razer_Mouse_Linux.zip && unzip -o Razer_Mouse_Linux.zip && cd Razer_Mouse_Linux-master && sh install.sh && cd .. && rm -rf Razer_Mouse_Linux-master Razer_Mouse_Linux.zip`     
Or run `sh install.sh` from the directory to install.     
## SUPPORTS :     
99% compatible with :
- Razer Naga Epic Chroma in CentOS 7
- Razer Naga Epic (pre-2014 version) in Ubuntu 14.04, 15.04, 15.10
- Razer Naga (RZ01-0028) (Thanks to khornem) in Ubuntu 14.04
- Razer Naga Chroma (Thanks to [felipeacsi](https://github.com/felipeacsi)) in Manjaro
- Razer Naga Molten (Thanks to noobxgockel) in Linux Mint 17.02 (Same as 2012)
- Razer Naga 2012 (RZ01-0058) (Thanks to [mrlinuxfish](https://github.com/violet-fish), [brianfreytag](https://github.com/brianfreytag)) in Arch Linux, Ubuntu 16.04
- Razer Naga Chroma (Thanks to [ipsod](https://github.com/ipsod)) in Linux Mint KDE 18.1
- Razer Naga Trinity (Thanks to [haringsrob](https://github.com/haringsrob) and [ws141](https://github.com/ws141))
- Razer Naga Pro Wireless (Thanks to [Stibax](https://github.com/Stibax))
- Razer Naga V2 (Thanks to [ibarrick](https://github.com/ibarrick))
- Razer Naga Left Handed (Thanks to [Izaya-San](https://github.com/Izaya-San))
- Razer Naga X (Thanks to [grabow](https://github.com/bgrabow))
- Razer Naga 2014 (Ubuntu)
## COMMANDS :

	`naga start` 				//Starts a daemon.	
	`naga edit ($EDITOR)` 			//Edits naga config, then restart service if edited.     
							(you can also specify editor : naga edit vim)
	`naga debug` 				//Shows logs in realt time. (Add arg to override journalctl's args, ex -au.)
	`naga stop`				//Stops the daemon.
	`naga fix`				//Restarts usb services.
	`naga uninstall` 			//Uninstalls the daemon tool.
	`naga` 					//Gives help.
    More :
	`naga serviceHelper ($CONFIG)`		//For the services or manual change of configs.      
							(need to disable service & add udev rule for $USER)        
Map razer naga devices keys easily with the command `naga edit`.     
## UBUNTU FLAVORS :    
Try installing first and if it doesn't work     
please add the corresponding repos for xdotool and his dependency :     
	https://packages.ubuntu.com/search?keywords=xdotool     
	https://packages.ubuntu.com/search?keywords=libxdo3&searchon=names     
	(Click on your version and add the line to /etc/apt/sources.list, then you can run the script.)     
	You can also install libnotify-bin to get notifications if they are not there : `sudo apt install libnotify-bin`     	
## DEPENDENCIES :	
X11 : `libx11-dev xdotool xinput g++ libxtst-dev libxmu-dev nano pkexec procps`    
Wayland : `g++ nano pkexec procps wget gnome-shell-extension-prefs dbus-x11 curl libdbus-1-dev golang-go`    
If you are running something else than ubuntu and it's not compiling theses are the packages to find.     
## INSTALLATION :     
Run `sh install.sh`.       
This will compile the source and copy the necessary files (see `install.sh` for more info).   
It will prompt you for your password, as it uses sudo to copy some files.  

You can also edit `src/naga.cpp` to adapt the installation to another device, using different inputs and/or different key codes than   
the Naga Epic, 2014, Molten or Chroma. For Example, Epic Chroma is compatible with Epic (they have the same buttons),   
so you would only have to add an additional line to the devices vector.
 
## AUTORUN :     
Now works with systemctl services !    
Also adds 2 lines to your .profile and a line to the sudoer's file to make sure you are always able to start the daemon on relogin.     

## CONFIGURATION :
The configuration file `keyMap.txt` has the following syntax :
    `config=<configName>` sets the name of the config. The initial loaded config is `defaultConfig` unless specified as argument.
    `<keynumber> - <option>=<command>`
    `<keynumber>` is a number between 1-14 representing the 12 keys of the naga's keypad + two on the top of the naga.
    `<option>` determines what will be applied to `<command>`. The possible choices are:

- `chmap`: Changes the config for another config inside `keymap.txt` in `~/.naga`.
- `chmapRelease`: Changes the config on key release.
- `unlockChmap`: Unlocks the config for a windowConfig that has been changed to another config (can also simply chmap back to the windowConfig). Simply said : Re-allows auto config changes per window.
- `sleep` and `sleepRelease`: Sleeps.
- `string` and `stringRelease`: Writes a string. This doesn't use xdotool so xdotool keys won't work.
- `xdotoolType` and `xdotoolTypeRelease`: Types a string using xdotool (fallback option).
- `key`: keyPress at press and keyRelease at release.
- `specialKey`: Special keyPress at press and special keyRelease at release.
- `keyPressOnPress`: The xdotool key is pressed when the key is pressed.
- `keyReleaseOnRelease`: The xdotool key is released when the key is released.	
- `keyPressOnRelease`: The xdotool key is pressed when the key is released.
- `keyReleaseOnPress`: The xdotool key is released when the key is pressed. There seems to be a list of keys on https://cgit.freedesktop.org/xorg/proto/x11proto/plain/keysymdef.h but you need to remove XK_ and they're not all there so google them if you want to be sure.
- `run`: Runs the command `<command>` (in bash /bin/bash) on key press.
- `run2`: Runs the command synchronously
- `runRelease`: Runs the command.
- `runRelease2`: Runs the command synchronously.
- `runAndWrite`: Runs the command `<command>` (in shell /bin/sh) and starts writing all of it's output, including newlines to the screen!
- `runAndWrite2`: Same as runAndWrite, synchronously.
- `setWorkspace`: Runs `<command>` in xdotool set_desktop <command> .
- `mousePosition`: Runs `<command>` in xdotool mousemove <command> .
- `keyClick` : Presses a key once when button pressed
- `keyClickRelease` : Presses a Key once when button released

#Use theses to press/unpress any char (faster than xdotool, no support for keys like ctrl or media yet)
- `specialPressOnPress` : Supports 1 char.
- `specialPressOnRelease` : 1 char
- `specialReleaseOnPress` : 1 char
- `specialReleaseOnRelease` : 1 char


`configEnd` Marks the end of a config.   
For a mouseclick use something like `run=xdotool click xPos yPos` (Check xdotool wiki for more).

You may have as many configs as you want in the keyMap.txt file, just make sure to give them different names and include defaultConfig.

[Link for Keys](https://cgit.freedesktop.org/xorg/proto/x11proto/plain/keysymdef.h)

### NOTES :     
To reload the config run the command :     
	naga start   
which will restart the daemon    
!!!!!!!!!!!!    
If the `$HOME/.naga/keyMap.txt` file is missing the daemon won't start      
(the program will NOT autocreate this file, the install.sh script will copy an example file though).      
For a key multiple actions may be defined. They will then be executed sequentially at the key press.     
An example `keyMap.txt` configuration file is the following:     
    #Comments should be accepted
    config=defaultConfig
    1 - key=XF86AudioPlay
    3 - chmap=420configEnemyBlazerWoW
    4 - run=notify-send 'Button # 4' 'Pressed'
    #etc
    configEnd

    config=420configEnemyBlazerWoW
    1 - run=sh ~/hacks.sh
    2 - chmap=defaultConfig
    #etc
    configEnd     
If you are trying to disable a button's original input (for the top buttons, the numpad is ok), you might want to test with xinput or evtest.     
Any non existing functionality can be created through the "run" option.       
#### In depth :
1) In order to get rid of the original bindings it disables the keypad using xinput as follows:      
    $ xinput set-int-prop [id] "Device Enabled" 8 0     
where [id] is the id number of the keypad returned by $ xinput.     
2) You may have to also run     
    $ xinput set-button-map [id2] 1 2 3 4 5 6 7 11 10 8 9 13 14 15      
where [id2] is the id number of the pointer device returned by `xinput`   
In the case of naga 2014 you also have to check which of those two has more than 7 numbers by typing `xinput get-button-map [id2]`.     
Although this seems to be unnecessary in some systems (i.e CentOS 7)     
This tool adds the files `$HOME/.naga/`, `/etc/udev/rules.d/80-naga.rules`, `/usr/local/bin/(naga && nagaXinputStart.sh)`, and `/etc/systemd/system/naga.service`.     
It also adds two lines to your ~/.profile for persistance, along with one line to the sudoer's file, which lets you run `sudo systemctl start naga` from within your .profile.     
## UNINSTALLATION :     
To uninstall run `naga uninstall` .
