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
static int fakeKeyFollowCount = 0;
map<string, string> notifySendMap;
string userString = "", userID = "";

const string *const applyUserString(string c)
{
	return new string("sudo -Hiu " + userString + " DBUS_SESSION_BUS_ADDRESS=unix:path=/run/user/" + userID + "/bus bash -c '" + c + "'");
}

const void fetchUserString()
{
	ifstream file("/home/razerInput/.naga/user.txt");
	string line;
	if (getline(file, line))
	{
		line.erase(remove_if(line.begin(), line.end(), [](unsigned char c)
							 { return isspace(c); }),
				   line.end()); // nuke all whitespaces
		userString = line;
		if (getline(file, line))
		{
			line.erase(remove_if(line.begin(), line.end(), [](unsigned char c)
								 { return isspace(c); }),
					   line.end()); // nuke all whitespaces
			userID = line;
		}
		else
			exit(1);
	}
	else
		exit(1);
	file.close();
}

class configKey
{
private:
	const string *const prefix;
	const bool onKeyPressed;
	const void (*const internalFunction)(const string *const c);

public:
	const bool IsOnKeyPressed() const { return onKeyPressed; }
	const void runInternal(const string *const content) const { internalFunction(content); }
	const string *const Prefix() const { return prefix; }
	const void (*InternalFunction() const)(const string *const) { return internalFunction; }

	configKey(const bool tonKeyPressed, const void (*const tinternalF)(const string *const cc), const string tcontent = "") : prefix(new string(tcontent)), onKeyPressed(tonKeyPressed), internalFunction(tinternalF)
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

	MacroEvent(const configKey *const tkeyType, const string *const tcontent) : keyType(tkeyType), content(new string(*tcontent))
	{
	}
};

typedef vector<MacroEvent> MacroEventVector;

class configSwitchScheduler
{
private:
	bool scheduledReMap = false, aWindowConfigActive = false;
	string scheduledReMapString = "", temporaryWindowConfigName = "", backupConfigName = "";

public:
	vector<string *> configWindowsNamesVector;

	const string *RemapString() const
	{
		return &scheduledReMapString;
	}
	const string *temporaryWindowName() const
	{
		return &temporaryWindowConfigName;
	}
	const string *getBackupConfigName() const
	{
		return &backupConfigName;
	}
	const bool isAWindowConfigActive() const
	{
		return aWindowConfigActive;
	}
	const bool isRemapScheduled() const
	{
		return scheduledReMap;
	}
	const void scheduleReMap(const string *reMapString)
	{
		scheduledReMapString = *reMapString;
		scheduledReMap = true, aWindowConfigActive = false;
		temporaryWindowConfigName = backupConfigName = "";
	}
	const void scheduleWindowReMap(const string *reMapString)
	{
		if (!aWindowConfigActive)
		{
			backupConfigName = scheduledReMapString;
		}
		scheduledReMapString = temporaryWindowConfigName = *reMapString;
		scheduledReMap = aWindowConfigActive = true;
	}
	const void unScheduleReMap()
	{
		scheduledReMap = false;
	}
};

static configSwitchScheduler *const configSwitcher = new configSwitchScheduler();

class NagaDaemon
{
private:
	const string conf_file = "/home/razerInput/.naga/keyMap.txt";

	map<string, configKey *const> configKeysMap;
	map<string, map<int, map<bool, MacroEventVector>>> macroEventsKeyMaps;

	string currentConfigName;
	struct input_event ev1[64];
	const int size = sizeof(struct input_event) * 64;
	vector<CharAndChar> devices;
	bool areSideBtnEnabled = true, areExtraBtnEnabled = true;
	map<int, map<bool, MacroEventVector>> *currentConfigPtr;

	void initConf()
	{
		string commandContent;
		map<int, map<bool, MacroEventVector>> *iteratedConfig;
		bool isIteratingConfig = false;

		ifstream in(conf_file.c_str(), ios::in);
		if (!in)
		{
			cerr << "Cannot open " << conf_file << ". Exiting." << endl;
			exit(1);
		}

		for (int readingLine = 1; getline(in, commandContent); readingLine++)
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
					string commandType = commandContent.substr(0, pos);										   // commandType = numbers + command type
					commandContent.erase(0, pos + 1);														   // commandContent = command content
					commandType.erase(remove(commandType.begin(), commandType.end(), ' '), commandType.end()); // Erase spaces inside 1st part of the line
					pos = commandType.find("-");
					const string buttonNumber = commandType.substr(0, pos); // Isolate button number
					commandType = commandType.substr(pos + 1);				// Isolate command type
					for (char &c : commandType)
						c = tolower(c);

					int buttonNumberInt;
					try
					{
						buttonNumberInt = stoi(buttonNumber);
					}
					catch (...)
					{
						clog << "At config line " << readingLine << ": expected a number" << endl;
						exit(1);
					}

					map<bool, MacroEventVector> *iteratedButtonConfig = &(*iteratedConfig)[buttonNumberInt];

					if (configKeysMap.contains(commandType))
					{ // filter out bad types
						if (!configKeysMap[commandType]->Prefix()->empty())
							commandContent = *configKeysMap[commandType]->Prefix() + commandContent;

						if (configKeysMap[commandType]->InternalFunction() == executeNow)
							(*iteratedButtonConfig)[configKeysMap[commandType]->IsOnKeyPressed()].emplace_back(MacroEvent(&configKeysMap[commandType], applyUserString(commandContent)));
						else
							(*iteratedButtonConfig)[configKeysMap[commandType]->IsOnKeyPressed()].emplace_back(MacroEvent(&configKeysMap[commandType], &commandContent));
						// Encode and store mapping v3
					}
					else if (commandType == "key")
					{
						if (commandContent.size() == 1)
						{
							commandContent = hexChar(commandContent.front());
						}
						string commandContent2 = *configKeysMap["keyreleaseonrelease"]->Prefix() + commandContent;
						commandContent = *configKeysMap["keypressonpress"]->Prefix() + commandContent;
						(*iteratedButtonConfig)[true].emplace_back(MacroEvent(configKeysMap["keypressonpress"], applyUserString(commandContent)));
						(*iteratedButtonConfig)[false].emplace_back(MacroEvent(configKeysMap["keyreleaseonrelease"], applyUserString(commandContent2)));
					}
					else if (commandType == "specialkey")
					{
						(*iteratedButtonConfig)[true].emplace_back(MacroEvent(configKeysMap["specialpressonpress"], &commandContent));
						(*iteratedButtonConfig)[false].emplace_back(MacroEvent(configKeysMap["specialreleaseonrelease"], &commandContent));
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
				notifySendMap.emplace(commandContent, new string (*applyUserString("notify-send \"Profile : " + commandContent + "\"")));
			}
			else if (commandContent.substr(0, 7) == "config=")
			{
				isIteratingConfig = true;
				commandContent.erase(0, 7);
				iteratedConfig = &macroEventsKeyMaps[commandContent];
				notifySendMap.emplace(commandContent, new string (*applyUserString("notify-send \"Profile : " + commandContent + "\"")));
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
			int err = (system(notifySendMap[currentConfigName].c_str()));
		}
	}

	string hexChar(const char a)
	{
		ostringstream hexedChar;
		hexedChar << "0x" << std::setfill('0') << std::setw(2) << std::hex << static_cast<int>(a);
		return hexedChar.str();
	}

	int side_btn_fd, extra_btn_fd;
	input_event *ev11;
	fd_set readset;

	void checkForWindowConfig()
	{
		char * currentFolder = getActiveWindow();
		clog << "WindowNameLog : " << currentFolder << endl;
		auto tempWindowName = configSwitcher->temporaryWindowName();
		if (tempWindowName->empty()  || strcmp(currentFolder, tempWindowName->c_str()) == 0)
		{
			bool found = false;
			for (string *configWindowName : configSwitcher->configWindowsNamesVector)
			{
				if (string(currentFolder) == *configWindowName)
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
				if (ev1[0].value != ' ' && ev11->type == EV_KEY)
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
				if (ev11->type == 1)
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
		const int strSize = macroContent->size();
		for (int z = 0; z < strSize; z++)
		{
			fakekey_press(aKeyFaker, (unsigned char *)&(*macroContent)[z], 8, 0);
			fakekey_release(aKeyFaker);
		}
		XFlush(aKeyFaker->xdpy);
		XCloseDisplay(aKeyFaker->xdpy);
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
		int err = (system(macroContent->c_str()));
	}
	// end of configKeys functions

	static void runActions(MacroEventVector *const relativeMacroEventsPointer)
	{
		for (auto macroEventPointer : *relativeMacroEventsPointer)
		{ // run all the events at Key
			macroEventPointer.KeyType()->runInternal(macroEventPointer.Content());
		}
	}

	void emplaceConfigKey(const std::string &key, bool onKeyPressed, auto functionPtr, const std::string &arg = "")
	{
		configKeysMap.emplace(key, new configKey(onKeyPressed, functionPtr, arg));
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
		// devices.emplace_back("/dev/input/by-id/YOUR_DEVICE_FILE", "/dev/input/by-id/YOUR_DEVICE_FILE#2");			 // DUMMY EXAMPLE ~ ONE CAN BE EMPTY LIKE SUCH : ""  (for devices with no extra buttons)

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
		fetchUserString();
#define ONKEYPRESSED true
#define ONKEYRELEASED false

		emplaceConfigKey("chmap", ONKEYPRESSED, chmapNow);
		emplaceConfigKey("chmaprelease", ONKEYRELEASED, chmapNow);
		emplaceConfigKey("sleep", ONKEYPRESSED, sleepNow);
		emplaceConfigKey("sleeprelease", ONKEYRELEASED, sleepNow);

		emplaceConfigKey("run", ONKEYPRESSED, executeNow, "setsid ");
		emplaceConfigKey("run2", ONKEYPRESSED, executeNow);
		emplaceConfigKey("runrelease", ONKEYRELEASED, executeNow, "setsid ");
		emplaceConfigKey("runrelease2", ONKEYRELEASED, executeNow);

		emplaceConfigKey("keypressonpress", ONKEYPRESSED, executeNow, "setsid xdotool keydown --window getactivewindow ");
		emplaceConfigKey("keypressonrelease", ONKEYRELEASED, executeNow, "setsid xdotool keydown --window getactivewindow ");

		emplaceConfigKey("keyreleaseonpress", ONKEYPRESSED, executeNow, "setsid xdotool keyup --window getactivewindow ");
		emplaceConfigKey("keyreleaseonrelease", ONKEYRELEASED, executeNow, "setsid xdotool keyup --window getactivewindow ");

		emplaceConfigKey("keyclick", ONKEYPRESSED, executeNow, "setsid xdotool key --window getactivewindow ");
		emplaceConfigKey("keyclickrelease", ONKEYRELEASED, executeNow, "setsid xdotool key --window getactivewindow ");

		emplaceConfigKey("string", ONKEYPRESSED, writeStringNow);
		emplaceConfigKey("stringrelease", ONKEYRELEASED, writeStringNow);

		emplaceConfigKey("specialpressonpress", ONKEYPRESSED, specialPressNow);
		emplaceConfigKey("specialpressonrelease", ONKEYRELEASED, specialPressNow);

		emplaceConfigKey("specialreleaseonpress", ONKEYPRESSED, specialReleaseNow);
		emplaceConfigKey("specialreleaseonrelease", ONKEYRELEASED, specialReleaseNow);

		initConf();

		configSwitcher->scheduleReMap(&mapConfig);
		loadConf(&mapConfig); // Initialize config

		int err = (system(("export PATH=$(" + *applyUserString("printf '%s' $PATH") + ")").c_str()));
		int err2 = (system(("export DISPLAY=$(" + *applyUserString("printf '%s' $DISPLAY") + ")").c_str()));
		run();
	}
};

void stopD()
{
	clog << "Stopping possible naga daemon" << endl;
	int err = (system(("/usr/local/bin/Naga_Linux/nagaKillroot.sh " + to_string((int)getpid())).c_str()));
};

// arguments manage
int main(const int argc, const char *const argv[])
{
	if (argc > 1)
	{
		if (strstr(argv[1], "serviceHelper") != NULL)
		{
			int err = (system("/usr/local/bin/Naga_Linux/nagaXinputStart.sh"));
			if (argc > 2)
				NagaDaemon(string(argv[2]).c_str());
			else
				NagaDaemon();
		}
		else if (strstr(argv[1], "service") != NULL)
		{
			if (argc > 2)
			{
				if (strstr(argv[2], "start") != NULL)
				{
					int err = (system("sudo systemctl start naga"));
				}
				else if (strstr(argv[2], "stop") != NULL)
				{
					int err = (system("sudo systemctl stop naga"));
				}
				else if (strstr(argv[2], "disable") != NULL)
				{
					int err = (system("sudo systemctl disable naga"));
				}
				else if (strstr(argv[2], "enable") != NULL)
				{
					int err = (system("sudo systemctl enable naga"));
				}
			}
		}
		else if (strstr(argv[1], "start") != NULL)
		{
			stopD();
			clog << "Starting naga daemon as service, naga debug to see logs..." << endl;
			if (argc > 2 && strstr(argv[2], "debug") != NULL)
			{
				clog << "Starting naga debug, logs :" << endl;
				usleep(100000);
				int err = (system("/usr/local/bin/Naga_Linux/nagaXinputStart.sh"));
				NagaDaemon();
			}
			else
			{
				usleep(100000);
				int err = (system("sudo systemctl start naga"));
			}
		}
		else if (strstr(argv[1], "kill") != NULL || strstr(argv[1], "stop") != NULL)
		{
			stopD();
		}
		else if (strstr(argv[1], "repair") != NULL || strstr(argv[1], "tame") != NULL || strstr(argv[1], "fix") != NULL)
		{
			clog << "Fixing dead keypad syndrome... STUTTER!!" << endl;
			int err = (system("sudo bash -c \"naga stop && modprobe -r usbhid && modprobe -r psmouse && modprobe usbhid && modprobe psmouse && sleep 1 && systemctl start naga\""));
		}
		else if (strstr(argv[1], "edit") != NULL)
		{
			int err = (system("sudo bash -c \"nano /home/razerInput/.naga/keyMap.txt && systemctl restart naga\""));
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
				int err = (system("/usr/local/bin/Naga_Linux/nagaUninstall.sh"));
			}
		}
	}
	else
	{
		clog << "Possible arguments : \n  start          Starts the daemon in hidden mode. (stops it before)\n  stop           Stops the daemon.\n  edit           Lets you edit the config.\n  start debug     Shows log.\n  repair         For dead keypad.\n  uninstall      Uninstalls the daemon.\n  There's also naga service (start | stop | disable | enable)" << endl;
	}
	return 0;
}
