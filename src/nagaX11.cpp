// This is lostallmymoney's remake of RaulPPelaez's original tool.
// RaulPPelaez, et. al wrote the original file.  As long as you retain this notice you
// can do whatever you want with this stuff.

#include "fakeKeysX11.hpp"
#include "getactivewindowX11.hpp"
#include <algorithm>
#include <iostream>
#include <vector>
#include <fstream>
#include <fcntl.h>
#include <unistd.h>
#include <linux/input.h>
#include <thread>
#include <iomanip>
#include <mutex>
#include <map>
#include <sstream>
using namespace std;

typedef pair<const char *const, const char *const> CharAndChar;
static mutex fakeKeyFollowUpsMutex, configSwitcherMutex;
static map<const char *const, FakeKey *const> *const fakeKeyFollowUps = new map<const char *const, FakeKey *const>();
const string conf_file = string(getenv("HOME")) + "/.naga/keyMap.txt";
static int fakeKeyFollowCount = 0;
map<string, string *> notifySendMap;

class configKey
{
private:
	const string *const prefix, *const suffix;
	const bool onKeyPressed;
	const void (*const internalFunction)(const string *const c);

public:
	const bool IsOnKeyPressed() const { return onKeyPressed; }
	const void runInternal(const string *const content) const { internalFunction(content); }
	const string *const Prefix() const { return prefix; }
	const string *const Suffix() const { return suffix; }
	const void (*InternalFunction() const)(const string *const) { return internalFunction; }

	configKey(const bool tonKeyPressed, const void (*const tinternalF)(const string *const cc), const string tcontent = "", const string tsuffix = "") : suffix(new string(tsuffix)), prefix(new string(tcontent)), onKeyPressed(tonKeyPressed), internalFunction(tinternalF)
	{
	}
};

class MacroEvent
{
private:
	const configKey *const keyType;
	const string *const content;

public:
	const configKey *const KeyType() const { return keyType; }
	const string *const Content() const { return content; }

	MacroEvent(const configKey *tkeyType, const string *tcontent) : keyType(tkeyType), content(new string(*tcontent))
	{
	}
};

class configSwitchScheduler
{
private:
	bool scheduledReMap = false, aWindowConfigActive = false;
	const string * temporaryWindowConfigName = NULL;
	const string * backupConfigName = NULL;
	const string * scheduledReMapString = NULL;

public:
	vector<string *> configWindowsNamesVector;

	const string *RemapString()
	{
		return scheduledReMapString;
	}
	const string *temporaryWindowName()
	{
		return temporaryWindowConfigName;
	}
	const string *getBackupConfigName()
	{
		return backupConfigName;
	}
	bool isAWindowConfigActive()
	{
		return aWindowConfigActive;
	}
	bool isRemapScheduled()
	{
		return scheduledReMap;
	}
	void scheduleReMap(const string *const reMapString)
	{
		scheduledReMapString = reMapString;
		scheduledReMap = true, aWindowConfigActive = false;
		temporaryWindowConfigName = backupConfigName = NULL;
	}
	void scheduleWindowReMap(const string *reMapString)
	{
		if (!aWindowConfigActive)
		{
			backupConfigName = scheduledReMapString;
		}
		scheduledReMapString = temporaryWindowConfigName = reMapString;
		scheduledReMap = aWindowConfigActive = true;
	}
	void unScheduleReMap()
	{
		scheduledReMap = false;
	}
};

static configSwitchScheduler *const configSwitcher = new configSwitchScheduler();

class NagaDaemon
{
private:
	map<string, configKey *const> configKeysMap;
	map<string, map<int, map<bool, vector<MacroEvent *>>>> macroEventsKeyMaps;

	string currentConfigName;
	struct input_event ev1[64];
	const int size = sizeof(ev1);
	vector<CharAndChar> devices;
	bool areSideBtnEnabled = true, areExtraBtnEnabled = true;
	map<int, std::map<bool, vector<MacroEvent *>>> *currentConfigPtr;

	const string *applyBashCommand(string c)
	{
		return new string("bash -c '" + c + "'");
	}

	void initConf()
	{
		string commandContent;
		map<int, std::map<bool, vector<MacroEvent *>>> *iteratedConfig;
		bool isIteratingConfig = false;

		ifstream in(conf_file.c_str(), ios::in);
		if (!in)
		{
			cerr << "Cannot open " << conf_file << ". Exiting." << endl;
			exit(1);
		}

		while (getline(in, commandContent))
		{
			if (commandContent[0] == '#' || commandContent.find_first_not_of(' ') == string::npos)
				continue; // Ignore comments, empty lines

			if (isIteratingConfig)
			{
				if (commandContent.substr(0, 10) == "configEnd") // finding configEnd
				{
					isIteratingConfig = false;
				}
				else
				{
					int pos = commandContent.find('=');
					string commandType = commandContent.substr(0, pos); // commandType = numbers + command type
					commandContent.erase(0, pos + 1);					// commandContent = command content
					commandType.erase(remove_if(commandType.begin(), commandType.end(), [](unsigned char c)
												{ return isspace(c); }),
									  commandType.end()); // nuke all whitespaces
					pos = commandType.find("-");
					const string *const buttonNumber = new string(commandType.substr(0, pos)); // Isolate button number
					commandType = commandType.substr(pos + 1);								   // Isolate command type
					for (char &c : commandType)
						c = tolower(c);

					int buttonNumberInt;
					try
					{
						buttonNumberInt = stoi(*buttonNumber);
					}
					catch (...)
					{
						clog << "CONFIG ERROR : " << commandContent << endl;
						exit(1);
					}

					map<bool, vector<MacroEvent *>> *iteratedButtonConfig = &(*iteratedConfig)[buttonNumberInt];

					if (configKeysMap.contains(commandType))
					{ // filter out bad types

						if (commandType == "run" || commandType == "run2") // This makes you able to run complete bash inside run
							commandContent = *applyBashCommand(commandContent);
						if (!configKeysMap[commandType]->Prefix()->empty())
							commandContent = *configKeysMap[commandType]->Prefix() + commandContent;
						if (!configKeysMap[commandType]->Suffix()->empty())
							commandContent = commandContent + *configKeysMap[commandType]->Suffix();

						(*iteratedButtonConfig)[configKeysMap[commandType]->IsOnKeyPressed()].emplace_back(new MacroEvent(configKeysMap[commandType], &commandContent));
						// Encode and store mapping v3
					}
					else if (commandType == "key")
					{
						if (commandContent.size() == 1)
						{
							commandContent = hexChar(commandContent[0]);
						}
						string *commandContent2 = new string(*configKeysMap["keyreleaseonrelease"]->Prefix() + commandContent + *configKeysMap["keyreleaseonrelease"]->Suffix());
						commandContent = *configKeysMap["keypressonpress"]->Prefix() + commandContent + *configKeysMap["keypressonpress"]->Suffix();
						(*iteratedButtonConfig)[true].emplace_back(new MacroEvent(configKeysMap["keypressonpress"], &commandContent));
						(*iteratedButtonConfig)[false].emplace_back(new MacroEvent(configKeysMap["keyreleaseonrelease"], commandContent2));
					}
					else if (commandType == "specialkey")
					{
						(*iteratedButtonConfig)[true].emplace_back(new MacroEvent(configKeysMap["specialpressonpress"], &commandContent));
						(*iteratedButtonConfig)[false].emplace_back(new MacroEvent(configKeysMap["specialreleaseonrelease"], &commandContent));
					}
					else
					{
						clog << "Discarding : " << commandType << "=" << commandContent << endl;
					}
				}
			}
			else if (commandContent.substr(0, 13) == "configWindow=")
			{
				isIteratingConfig = true;
				commandContent.erase(0, 13);
				iteratedConfig = &macroEventsKeyMaps[commandContent];
				configSwitcher->configWindowsNamesVector.emplace_back(new string(commandContent));
				notifySendMap.emplace(commandContent, new string("notify-send \"Profile : " + commandContent + "\""));
			}
			else if (commandContent.substr(0, 7) == "config=")
			{
				isIteratingConfig = true;
				commandContent.erase(0, 7);
				iteratedConfig = &macroEventsKeyMaps[commandContent];
				notifySendMap.emplace(commandContent, new string("notify-send \"Profile : " + commandContent + "\""));
			}
		}
		in.close();
	}

	void loadConf(const string *const configName, const bool silent = false)
	{
		if (!macroEventsKeyMaps.contains(*configName))
		{
			clog << "Undefined profile : " << configName << endl;
			return;
		}
		configSwitcher->unScheduleReMap();

		currentConfigName = *configName;
		currentConfigPtr = &macroEventsKeyMaps[currentConfigName];
		if (!silent)
		{
			(void)!(system(notifySendMap[currentConfigName]->c_str()));
		}
	}

	string hexChar(const char a)
	{
		ostringstream hexedChar;
		hexedChar << "0x" << setfill('0') << setw(2) << hex << static_cast<int>(a);
		return hexedChar.str();
	}

	int side_btn_fd, extra_btn_fd;
	input_event *ev11;
	fd_set readset;

	void checkForWindowConfig()
	{
		char *currentAppTitle = getActiveWindow();
		clog << "WindowNameLog : " << currentAppTitle << endl;
		if (!configSwitcher->isAWindowConfigActive() || strcmp(currentAppTitle, configSwitcher->temporaryWindowName()->c_str()) != 0)
		{
			bool found = false;
			for (string *configWindowName : configSwitcher->configWindowsNamesVector)
			{
				if (strcmp(currentAppTitle, configWindowName->c_str()) == 0)
				{
					lock_guard<mutex> guard(configSwitcherMutex);
					configSwitcher->scheduleWindowReMap(configWindowName);
					loadConf(configSwitcher->RemapString(), true); // change config for macroEvents[ii]->Content()
					found = true;
				}
			}
			if (!found && configSwitcher->isAWindowConfigActive())
			{
				lock_guard<mutex> guard(configSwitcherMutex);
				configSwitcher->scheduleReMap(configSwitcher->getBackupConfigName());
				loadConf(configSwitcher->RemapString(), true); // change config for macroEvents[ii]->Content()
			}
		}
	}

	void run()
	{
		if (areSideBtnEnabled)
			ioctl(side_btn_fd, EVIOCGRAB, 1); // Give application exclusive control over side buttons.
		ev11 = &ev1[1];
		while (1)
		{
			if (configSwitcher->isRemapScheduled())
			{
				lock_guard<mutex> guard(configSwitcherMutex); // remap
				loadConf(configSwitcher->RemapString());	  // change config for macroEvents[ii]->Content()
			}

			FD_ZERO(&readset);
			if (areSideBtnEnabled)
				FD_SET(side_btn_fd, &readset);
			if (areExtraBtnEnabled)
				FD_SET(extra_btn_fd, &readset);
			if (select(FD_SETSIZE, &readset, NULL, NULL, NULL) == -1)
				exit(2);

			if (areSideBtnEnabled && FD_ISSET(side_btn_fd, &readset)) // Side buttons
			{
				if (read(side_btn_fd, ev1, size) == -1)
					exit(2);
				if (ev11->type == EV_KEY)
				{ // Key event (press or release)
					switch (ev11->code)
					{
					case 2 ... 13:
						checkForWindowConfig();
						thread(runActions, &(*currentConfigPtr)[ev11->code - 1][ev11->value == 1]).detach(); // real key number = ev11->code - 1
						break;
					}
				}
			}
			if (areExtraBtnEnabled && FD_ISSET(extra_btn_fd, &readset)) // Extra buttons
			{
				if (read(extra_btn_fd, ev1, size) == -1)
					exit(2);
				if (ev1[0].value != ' ' && ev11->type == EV_KEY)
				{ // Only extra buttons
					switch (ev11->code)
					{
					case 275 ... 276:
						checkForWindowConfig();
						thread(runActions, &(*currentConfigPtr)[ev11->code - 262][ev11->value == 1]).detach(); // real key number = ev11->code - OFFSET (#262)
						break;
					}
				}
			}
		}
	}

	// Functions that can be given to configKeys
	const static void writeStringNow(const string *const macroContent)
	{
		lock_guard<mutex> guard(fakeKeyFollowUpsMutex);
		FakeKey *const aKeyFaker = fakekey_init(XOpenDisplay(NULL));
		for (const char &c : *macroContent)
		{
			fakekey_press(aKeyFaker, reinterpret_cast<const unsigned char *>(&c), 8, 0);
			fakekey_release(aKeyFaker);
		}
		XFlush(aKeyFaker->xdpy);
		XCloseDisplay(aKeyFaker->xdpy);
		delete aKeyFaker;
	}

	const static void specialPressNow(const string *const macroContent)
	{
		lock_guard<mutex> guard(fakeKeyFollowUpsMutex);
		FakeKey *const aKeyFaker = fakekey_init(XOpenDisplay(NULL));
		const char *const keyCodeChar = &(*macroContent)[0];
		fakekey_press(aKeyFaker, reinterpret_cast<const unsigned char *>(keyCodeChar), 8, 0);
		XFlush(aKeyFaker->xdpy);
		fakeKeyFollowUps->emplace(keyCodeChar, aKeyFaker);
		fakeKeyFollowCount++;
	}

	const static void specialReleaseNow(const string *const macroContent)
	{
		if (fakeKeyFollowCount > 0)
		{
			for (map<const char *const, FakeKey *const>::iterator aKeyFollowUpPair = fakeKeyFollowUps->begin(); aKeyFollowUpPair != fakeKeyFollowUps->end(); ++aKeyFollowUpPair)
			{
				if (*aKeyFollowUpPair->first == (*macroContent)[0])
				{
					lock_guard<mutex> guard(fakeKeyFollowUpsMutex);
					FakeKey *const aKeyFaker = aKeyFollowUpPair->second;
					fakekey_release(aKeyFaker);
					XFlush(aKeyFaker->xdpy);
					XCloseDisplay(aKeyFaker->xdpy);
					fakeKeyFollowCount--;
					fakeKeyFollowUps->erase(aKeyFollowUpPair);
					delete aKeyFaker;
					break;
				}
			}
		}
		else
			clog << "No candidate for key release" << endl;
	}

	const static void chmapNow(const string *const macroContent)
	{
		lock_guard<mutex> guard(configSwitcherMutex);
		configSwitcher->scheduleReMap(macroContent); // schedule config switch/change
	}

	const static void sleepNow(const string *const macroContent)
	{
		usleep(stoul(*macroContent) * 1000); // microseconds make me dizzy in keymap.txt
	}

	const static void executeNow(const string *const macroContent)
	{
		(void)!(system(macroContent->c_str()));
	}
	const static void executeThreadNow(const string *const macroContent)
	{
		thread(executeNow, macroContent).detach();
	}
	// end of configKeys functions

	static void runActions(vector<MacroEvent *> *const relativeMacroEventsPointer)
	{
		for (MacroEvent *const macroEventPointer : *relativeMacroEventsPointer)
		{ // run all the events at Key
			macroEventPointer->KeyType()->runInternal(macroEventPointer->Content());
		}
	}

	void emplaceConfigKey(const std::string &key, bool onKeyPressed, auto functionPtr, const std::string &prefix = "", const std::string &suffix = "")
	{
		configKeysMap.emplace(key, new configKey(onKeyPressed, functionPtr, prefix, suffix));
	}

public:
	NagaDaemon(const string mapConfig = "defaultConfig")
	{
		// modulable device files list
		devices.emplace_back("/dev/input/by-id/usb-Razer_Razer_Naga_Epic-if01-event-kbd", "/dev/input/by-id/usb-Razer_Razer_Naga_Epic-event-mouse");								 // NAGA EPIC
		devices.emplace_back("/dev/input/by-id/usb-Razer_Razer_Naga_Epic_Dock-if01-event-kbd", "/dev/input/by-id/usb-Razer_Razer_Naga_Epic_Dock-event-mouse");						 // NAGA EPIC DOCK
		devices.emplace_back("/dev/input/by-id/usb-Razer_Razer_Naga_2014-if02-event-kbd", "/dev/input/by-id/usb-Razer_Razer_Naga_2014-event-mouse");								 // NAGA 2014
		devices.emplace_back("/dev/input/by-id/usb-Razer_Razer_Naga-if01-event-kbd", "/dev/input/by-id/usb-Razer_Razer_Naga-event-mouse");											 // NAGA MOLTEN
		devices.emplace_back("/dev/input/by-id/usb-Razer_Razer_Naga_Epic_Chroma-if01-event-kbd", "/dev/input/by-id/usb-Razer_Razer_Naga_Epic_Chroma-event-mouse");					 // NAGA EPIC CHROMA
		devices.emplace_back("/dev/input/by-id/usb-Razer_Razer_Naga_Epic_Chroma_Dock-if01-event-kbd", "/dev/input/by-id/usb-Razer_Razer_Naga_Epic_Chroma_Dock-event-mouse");		 // NAGA EPIC CHROMA DOCK
		devices.emplace_back("/dev/input/by-id/usb-Razer_Razer_Naga_Chroma-if02-event-kbd", "/dev/input/by-id/usb-Razer_Razer_Naga_Chroma-event-mouse");							 // NAGA CHROMA
		devices.emplace_back("/dev/input/by-id/usb-Razer_Razer_Naga_Hex-if01-event-kbd", "/dev/input/by-id/usb-Razer_Razer_Naga_Hex-event-mouse");									 // NAGA HEX
		devices.emplace_back("/dev/input/by-id/usb-Razer_Razer_Naga_Hex_V2-if02-event-kbd", "/dev/input/by-id/usb-Razer_Razer_Naga_Hex_V2-event-mouse");							 // NAGA HEX v2
		devices.emplace_back("/dev/input/by-id/usb-Razer_Razer_Naga_Trinity_00000000001A-if02-event-kbd", "/dev/input/by-id/usb-Razer_Razer_Naga_Trinity_00000000001A-event-mouse"); // NAGA Trinity
		devices.emplace_back("/dev/input/by-id/usb-Razer_Razer_Naga_Left_Handed_Edition-if02-event-kbd", "/dev/input/by-id/usb-Razer_Razer_Naga_Left_Handed_Edition-event-mouse");	 // NAGA Left Handed
		devices.emplace_back("/dev/input/by-id/usb-Razer_Razer_Naga_Pro_000000000000-if02-event-kbd", "/dev/input/by-id/usb-Razer_Razer_Naga_Pro_000000000000-event-mouse");		 // NAGA PRO WIRELESS
		devices.emplace_back("/dev/input/by-id/usb-1532_Razer_Naga_Pro_000000000000-if02-event-kbd", "/dev/input/by-id/usb-1532_Razer_Naga_Pro_000000000000-event-mouse");			 // NAGA PRO
		// devices.emplace_back("/dev/input/by-id/YOUR_DEVICE_FILE", "/dev/input/by-id/YOUR_DEVICE_FILE#2");			 // DUMMY EXAMPLE, ONE CAN BE EMPTY LIKE SUCH : ""  (for devices with no extra buttons)

		for (CharAndChar &device : devices)
		{ // Setup check
			side_btn_fd = open(device.first, O_RDONLY), extra_btn_fd = open(device.second, O_RDONLY);
			if (side_btn_fd != -1 || extra_btn_fd != -1)
			{
				if (side_btn_fd == -1)
				{
					if (extra_btn_fd == -1)
					{
						cerr << "No naga devices found or you don't have permission to access them." << endl;
						exit(1);
					}
					clog << "Reading from: " << device.second << endl;
					areSideBtnEnabled = false;
				}
				else if (extra_btn_fd == -1)
				{
					clog << "Reading from: " << device.first << endl;
					areExtraBtnEnabled = false;
				}
				else
					clog << "Reading from: " << device.first << endl
						 << " and " << device.second << endl;
				break;
			}
		}

		// modulable options list to manage internals inside runActions method arg1:COMMAND, arg2:onKeyPressed?, arg3:function to send prefix+config content.
#define ONKEYPRESSED true
#define ONKEYRELEASED false

		emplaceConfigKey("chmap", ONKEYPRESSED, chmapNow);
		emplaceConfigKey("chmaprelease", ONKEYRELEASED, chmapNow);

		emplaceConfigKey("sleep", ONKEYPRESSED, sleepNow);
		emplaceConfigKey("sleeprelease", ONKEYRELEASED, sleepNow);

		emplaceConfigKey("run", ONKEYPRESSED, executeThreadNow);
		emplaceConfigKey("run2", ONKEYPRESSED, executeNow);

		emplaceConfigKey("runrelease", ONKEYRELEASED, executeThreadNow);
		emplaceConfigKey("runrelease2", ONKEYRELEASED, executeNow);

		emplaceConfigKey("launch", ONKEYRELEASED, executeThreadNow, "gtk-launch ");
		emplaceConfigKey("launch2", ONKEYRELEASED, executeNow, "gtk-launch ");

		emplaceConfigKey("keypressonpress", ONKEYPRESSED, executeThreadNow, "xdotool keydown --window getactivewindow ");
		emplaceConfigKey("keypressonrelease", ONKEYRELEASED, executeThreadNow, "xdotool keydown --window getactivewindow ");

		emplaceConfigKey("keyreleaseonpress", ONKEYPRESSED, executeThreadNow, "xdotool keyup --window getactivewindow ");
		emplaceConfigKey("keyreleaseonrelease", ONKEYRELEASED, executeThreadNow, "xdotool keyup --window getactivewindow ");

		emplaceConfigKey("keyclick", ONKEYPRESSED, executeThreadNow, "xdotool key --window getactivewindow ");
		emplaceConfigKey("keyclickrelease", ONKEYRELEASED, executeThreadNow, "xdotool key --window getactivewindow ");

		emplaceConfigKey("string", ONKEYPRESSED, writeStringNow);
		emplaceConfigKey("stringrelease", ONKEYRELEASED, writeStringNow);

		emplaceConfigKey("specialpressonpress", ONKEYPRESSED, specialPressNow);
		emplaceConfigKey("specialpressonrelease", ONKEYRELEASED, specialPressNow);

		emplaceConfigKey("specialreleaseonpress", ONKEYPRESSED, specialReleaseNow);
		emplaceConfigKey("specialreleaseonrelease", ONKEYRELEASED, specialReleaseNow);

		initConf();

		configSwitcher->scheduleReMap(&mapConfig);
		loadConf(&mapConfig); // Initialize config

		run();
	}
};

void stopD()
{
	clog << "Stopping possible naga daemon" << endl;
	(void)!(system(("/usr/local/bin/Naga_Linux/nagaKillroot.sh " + to_string((int)getpid())).c_str()));
};

// arguments manage
int main(const int argc, const char *const argv[])
{
	if (argc > 1)
	{
		if (strstr(argv[1], "serviceHelper") != NULL)
		{
			stopD();
			(void)!(system("/usr/local/bin/Naga_Linux/nagaXinputStart.sh"));
			if (argc > 2 && !string(argv[2]).empty())
				NagaDaemon(string(argv[2]).c_str()); // lets you configure a default profile in /etc/systemd/system/naga.service
			else
				NagaDaemon();
		}
		else if (strstr(argv[1], "start") != NULL)
		{
			clog << "Starting naga daemon as service, naga debug to see logs..." << endl;
			usleep(100000);
			(void)!(system("sudo systemctl start naga"));
		}
		else if (strstr(argv[1], "debug") != NULL)
		{
			clog << "Starting naga debug, logs :" << endl;
			(void)!(system("journalctl -fu naga"));
		}
		else if (strstr(argv[1], "kill") != NULL || strstr(argv[1], "stop") != NULL)
		{
			clog << "Stopping possible naga daemon" << endl;
			(void)!(system(("sudo sh /usr/local/bin/Naga_Linux/nagaKillroot.sh " + to_string((int)getpid())).c_str()));
		}
		else if (strstr(argv[1], "repair") != NULL || strstr(argv[1], "tame") != NULL || strstr(argv[1], "fix") != NULL)
		{
			clog << "Fixing dead keypad syndrome... STUTTER!!" << endl;
			(void)!(system("sudo bash -c \"naga stop && modprobe -r usbhid && modprobe -r psmouse && modprobe usbhid && modprobe psmouse && sleep 1 && sudo systemctl start naga\""));
		}
		else if (strstr(argv[1], "edit") != NULL)
		{
			if (argc > 2)
			{
				(void)!(system(("sudo bash -c 'orig_sum=\"$(sudo md5sum " + conf_file + ")\"; " + string(argv[2]) + " " + conf_file + "; [[ \"$(sudo md5sum " + conf_file + ")\" != \"$orig_sum\" ]] && sudo systemctl restart naga'").c_str()));
			}
			else
			{
				(void)!(system(("sudo bash -c 'orig_sum=\"$(sudo md5sum " + conf_file + ")\"; sudo nano " + conf_file + "; [[ \"$(sudo md5sum " + conf_file + ")\" != \"$orig_sum\" ]] && sudo systemctl restart naga'").c_str()));
			}
		}
		else if (strstr(argv[1], "uninstall") != NULL)
		{
			string answer;
			clog << "Are you sure you want to uninstall ? y/n" << endl;
			cin >> answer;
			if (answer.length() != 1 || (answer[0] != 'y' && answer[0] != 'Y'))
			{
				clog << "Aborting" << endl;
			}
			else
			{
				(void)!(system("/usr/local/bin/Naga_Linux/nagaUninstall.sh"));
			}
		}
	}
	else
	{
		clog << "Possible arguments : \n  start          Starts the daemon in hidden mode. (stops it before)\n  stop           Stops the daemon.\n  edit           Lets you edit the config.\n  debug		 Shows log.\n  repair         For dead keypad.\n  uninstall      Uninstalls the daemon." << endl;
	}
	return 0;
}
