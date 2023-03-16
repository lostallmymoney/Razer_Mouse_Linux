// This is lostallmymoney's remake of RaulPPelaez's original tool.
// RaulPPelaez, et. al wrote the original file.  As long as you retain this notice you
// can do whatever you want with this stuff.

#include "fakeKeys.hpp"
#include "getactivewindow.hpp"
#include <algorithm>
#include <iostream>
#include <vector>
#include <fstream>
#include <fcntl.h>
#include <unistd.h>
#include <linux/input.h>
#include <cstring>
#include <thread>
#include <mutex>
#include <map>
#include <sstream>
using namespace std;

typedef pair<const char *, const char *> CharAndChar;
typedef pair<char, FakeKey *> CharAndFakeKey;
static mutex fakeKeyFollowUpsMutex, configSwitcherMutex;
static vector<CharAndFakeKey *> *const fakeKeyFollowUps = new vector<CharAndFakeKey *>();
static int fakeKeyFollowCount = 0;

class configKey
{
private:
	const string *const prefix;
	const bool onKeyPressed;
	const void (*const internalFunction)(const string *const c);

public:
	const bool &IsOnKeyPressed() const { return onKeyPressed; }
	const void runInternal(const string *const content) const { internalFunction(content); }
	const string *const Prefix() const { return prefix; }

	configKey(const bool tonKeyPressed, const void (*const tinternalF)(const string *cc) = NULL, const string tcontent = "") : prefix(new string(tcontent)), onKeyPressed(tonKeyPressed), internalFunction(tinternalF)
	{
	}
};

typedef pair<string, configKey *> stringAndConfigKey;

class MacroEvent
{
private:
	const configKey *const keyType;
	const string *const content;

public:
	const configKey *KeyType() const { return keyType; }
	const string *Content() const { return content; }

	MacroEvent(const configKey *tkeyType, const string *tcontent) : keyType(tkeyType), content(new string(*tcontent))
	{
	}
};

typedef vector<MacroEvent *> MacroEventVector;

class configSwitchScheduler
{
private:
	bool scheduledReMap = false, aWindowConfigActive = false;
	string scheduledReMapString = "", temporaryWindowConfigName = "", backupConfigName = "";

public:
	vector<string *> configWindowsNamesVector;

	const string &RemapString() const
	{
		return scheduledReMapString;
	}
	const string &temporaryWindowName() const
	{
		return temporaryWindowConfigName;
	}
	const string &getBackupConfigName() const
	{
		return backupConfigName;
	}
	const bool &isAWindowConfigActive() const
	{
		return aWindowConfigActive;
	}
	const bool &isRemapScheduled() const
	{
		return scheduledReMap;
	}
	const void scheduleReMap(const string *reMapString)
	{
		scheduledReMapString = *reMapString;
		scheduledReMap = true;
		aWindowConfigActive = false;
		temporaryWindowConfigName = "";
		backupConfigName = "";
	}
	const void scheduleWindowReMap(const string *reMapString)
	{
		if (!aWindowConfigActive)
		{
			backupConfigName = scheduledReMapString;
		}
		scheduledReMapString = *reMapString;
		temporaryWindowConfigName = *reMapString;
		scheduledReMap = true;
		aWindowConfigActive = true;
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
	const string conf_file = string(getenv("HOME")) + "/.naga/keyMap.txt";

	map<string, configKey *const> configKeysMap;
	map<string, map<int, map<bool, MacroEventVector>>> macroEventsKeyMaps;

	string currentConfigName;
	struct input_event ev1[64];
	const int size = sizeof(struct input_event) * 64;
	vector<CharAndChar> devices;
	bool areSideBtnEnabled = true, areExtraBtnEnabled = true, areWindowConfigsInitialised = false;

	void loadConf(const string configName, bool silent = false)
	{
		if (!macroEventsKeyMaps.contains(configName))
		{
			ifstream in(conf_file.c_str(), ios::in);
			if (!in)
			{
				cerr << "Cannot open " << conf_file << ". Exiting." << endl;
				exit(1);
			}
			bool found1 = false, found2 = false;
			int pos, configLine, configEndLine;
			string commandContent;

			if (!areWindowConfigsInitialised)
			{
				for (int readingLine = 1; getline(in, commandContent) && !found2; readingLine++)
				{
					if (commandContent.find("configWindow=") != string::npos)
					{
						commandContent.erase(0, 13);
						(*configSwitcher).configWindowsNamesVector.emplace_back(new string(commandContent));
					}
				}
				areWindowConfigsInitialised = true;
			}

			in.clear();
			in.seekg(0, ios::beg); // reset file reading

			for (int readingLine = 1; getline(in, commandContent) && !found2; readingLine++)
			{
				if (!found1)
				{
					if (commandContent.find("config=" + configName) != string::npos || commandContent.find("configWindow=" + configName) != string::npos) // finding configname
					{
						configLine = readingLine;
						found1 = true;
					}
				}
				else
				{
					if (commandContent.find("configEnd") != string::npos) // finding configEnd
					{
						configEndLine = readingLine;
						found2 = true;
					}
				}
			}
			if (!found1 || !found2)
			{
				clog << "Error with config names and configEnd : " << configName << ". Exiting." << endl;
				exit(1);
			}
			in.clear();
			in.seekg(0, ios::beg); // reset file reading

			for (int readingLine = 1; getline(in, commandContent) && readingLine < configEndLine; readingLine++)
			{
				if (readingLine > configLine)
				{
					if (commandContent[0] == '#' || commandContent.find_first_not_of(' ') == string::npos)
						continue; // Ignore comments, empty lines
					pos = commandContent.find('=');
					string *commandType = new string(commandContent.substr(0, pos));							   // commandType = numbers + command type
					commandContent.erase(0, pos + 1);															   // commandContent = command content
					commandType->erase(remove(commandType->begin(), commandType->end(), ' '), commandType->end()); // Erase spaces inside 1st part of the line
					pos = commandType->find("-");
					const string *const buttonNumber = new string(commandType->substr(0, pos)); // Isolate button number
					commandType = new string(commandType->substr(pos + 1));						// Isolate command type
					for (char &c : *commandType)
						c = tolower(c);
					if (configKeysMap.contains(*commandType))
					{ // filter out bad types
						int buttonNumberI;
						try
						{
							buttonNumberI = stoi(*buttonNumber);
						}
						catch (...)
						{
							clog << "At config line " << readingLine << ": expected a number" << endl;
							exit(1);
						}

						if (*commandType == "key")
						{
							if (commandContent.size() == 1)
							{
								commandContent = hexChar(commandContent[0]);
							}
							const string *const commandContent2 = new string(*configKeysMap["keyreleaseonrelease"]->Prefix() + commandContent);
							commandContent = *configKeysMap["keypressonpress"]->Prefix() + commandContent;
							macroEventsKeyMaps[configName][buttonNumberI][true].emplace_back(new MacroEvent(configKeysMap["keypressonpress"], &commandContent));
							macroEventsKeyMaps[configName][buttonNumberI][false].emplace_back(new MacroEvent(configKeysMap["keyreleaseonrelease"], commandContent2));
						}
						else
						{
							if (*configKeysMap[*commandType]->Prefix() != "")
								commandContent = *configKeysMap[*commandType]->Prefix() + commandContent;
							macroEventsKeyMaps[configName][buttonNumberI][configKeysMap[*commandType]->IsOnKeyPressed()].emplace_back(new MacroEvent(configKeysMap[*commandType], &commandContent));
						} // Encode and store mapping v3
					}
				}
			}
			in.close();
		}
		currentConfigName = configName;
		if (!silent)
		{
			(void)!(system(("notify-send -t 200 'New config :' '" + configName + "'").c_str()));
		}
	}

	string hexChar(const char a)
	{
		stringstream hexedChar;
		hexedChar << "0x00" << hex << (int)(a);
		return hexedChar.str();
	}

	int side_btn_fd, extra_btn_fd;
	input_event *ev11;
	fd_set readset;

	void checkForWindowConfig()
	{
		char *c;
		try
		{
			c = getActiveWindow();
		}
		catch (...)
		{
			return;
		}
		clog << "Name of current window : " << c << endl;
		bool found = false;
		if (configSwitcher->temporaryWindowName() == "" || strstr(c, configSwitcher->temporaryWindowName().c_str()) == NULL)
		{
			for (string *configWindowName : (*configSwitcher).configWindowsNamesVector)
			{
				if (strstr(c, configWindowName->c_str()) != NULL)
				{
					lock_guard<mutex> guard(configSwitcherMutex);
					configSwitcher->scheduleWindowReMap(configWindowName);
					loadConf(configSwitcher->RemapString(), true); // change config for macroEvents[ii]->Content()
					configSwitcher->unScheduleReMap();
					found = true;
					break;
				}
			}
			if (!found && configSwitcher->isAWindowConfigActive())
			{
				lock_guard<mutex> guard(configSwitcherMutex);
				configSwitcher->scheduleReMap(&configSwitcher->getBackupConfigName());
				loadConf(configSwitcher->RemapString(), true); // change config for macroEvents[ii]->Content()
				configSwitcher->unScheduleReMap();
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
				configSwitcher->unScheduleReMap();
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
					case 2:
					case 3:
					case 4:
					case 5:
					case 6:
					case 7:
					case 8:
					case 9:
					case 10:
					case 11:
					case 12:
					case 13:
						checkForWindowConfig();
						thread(runActions, &macroEventsKeyMaps[currentConfigName][ev11->code - 1][ev11->value == 1]).detach(); // real key number = ev11->code - 1
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
					case 275:
					case 276:
					case 277:
					case 278:
						checkForWindowConfig();
						thread(runActions, &macroEventsKeyMaps[currentConfigName][ev11->code - 262][ev11->value == 1]).detach(); // real key number = ev11->code - OFFSET (#262)
						break;
					}
				}
			}
		}
	}

	// Functions that can be given to configKeys
	const static void writeStringNow(const string *macroContent)
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
		deleteFakeKey(aKeyFaker);
	}

	const static void specialPressNow(const string *macroContent)
	{
		lock_guard<mutex> guard(fakeKeyFollowUpsMutex);
		FakeKey *const aKeyFaker = fakekey_init(XOpenDisplay(NULL));
		fakekey_press(aKeyFaker, (unsigned char *)&(*macroContent)[0], 8, 0);
		XFlush(aKeyFaker->xdpy);
		fakeKeyFollowUps->emplace_back(new CharAndFakeKey((*macroContent)[0], aKeyFaker));
		fakeKeyFollowCount++;
	}

	const static void specialReleaseNow(const string *macroContent)
	{
		if (fakeKeyFollowCount > 0)
		{
			lock_guard<mutex> guard(fakeKeyFollowUpsMutex);
			for (int vectorId = fakeKeyFollowUps->size() - 1; vectorId >= 0; vectorId--)
			{
				CharAndFakeKey *const aKeyFollowUp = (*fakeKeyFollowUps)[vectorId];
				if (get<0>(*aKeyFollowUp) == (*macroContent)[0])
				{
					FakeKey *const aKeyFaker = get<1>(*aKeyFollowUp);
					fakekey_release(aKeyFaker);
					XFlush(aKeyFaker->xdpy);
					XCloseDisplay(aKeyFaker->xdpy);
					fakeKeyFollowUps->erase(fakeKeyFollowUps->begin() + vectorId);
					fakeKeyFollowCount--;
					deleteFakeKey(aKeyFaker);
				}
				delete aKeyFollowUp;
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
	// end of configKeys functions

	static void runActions(MacroEventVector *const relativeMacroEventsPointer)
	{
		for (MacroEvent *const macroEventPointer : *relativeMacroEventsPointer)
		{ // run all the events at Key
			macroEventPointer->KeyType()->runInternal(macroEventPointer->Content());
		}
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
		configKeysMap.insert(stringAndConfigKey("key", NULL)); // special one

		configKeysMap.insert(stringAndConfigKey("chmap", new configKey(true, chmapNow))); // change keymap
		configKeysMap.insert(stringAndConfigKey("chmaprelease", new configKey(false, chmapNow)));

		configKeysMap.insert(stringAndConfigKey("sleep", new configKey(true, sleepNow)));
		configKeysMap.insert(stringAndConfigKey("sleeprelease", new configKey(false, sleepNow)));

		configKeysMap.insert(stringAndConfigKey("run", new configKey(true, executeNow, "setsid ")));
		configKeysMap.insert(stringAndConfigKey("run2", new configKey(true, executeNow)));

		configKeysMap.insert(stringAndConfigKey("runrelease", new configKey(false, executeNow, "setsid ")));
		configKeysMap.insert(stringAndConfigKey("runrelease2", new configKey(false, executeNow)));

		configKeysMap.insert(stringAndConfigKey("keypressonpress", new configKey(true, executeNow, "setsid xdotool keydown --window getactivewindow ")));
		configKeysMap.insert(stringAndConfigKey("keypressonrelease", new configKey(false, executeNow, "setsid xdotool keydown --window getactivewindow ")));

		configKeysMap.insert(stringAndConfigKey("keyreleaseonpress", new configKey(true, executeNow, "setsid xdotool keyup --window getactivewindow ")));
		configKeysMap.insert(stringAndConfigKey("keyreleaseonrelease", new configKey(false, executeNow, "setsid xdotool keyup --window getactivewindow ")));

		configKeysMap.insert(stringAndConfigKey("keyclick", new configKey(true, executeNow, "setsid xdotool key --window getactivewindow ")));
		configKeysMap.insert(stringAndConfigKey("keyclickrelease", new configKey(false, executeNow, "setsid xdotool key --window getactivewindow ")));

		configKeysMap.insert(stringAndConfigKey("string", new configKey(true, writeStringNow)));
		configKeysMap.insert(stringAndConfigKey("stringrelease", new configKey(false, writeStringNow)));

		configKeysMap.insert(stringAndConfigKey("specialpressonpress", new configKey(true, specialPressNow)));
		configKeysMap.insert(stringAndConfigKey("specialpressonrelease", new configKey(false, specialPressNow)));

		configKeysMap.insert(stringAndConfigKey("specialreleaseonpress", new configKey(true, specialReleaseNow)));
		configKeysMap.insert(stringAndConfigKey("specialreleaseonrelease", new configKey(false, specialReleaseNow)));

		configSwitcher->scheduleReMap(&mapConfig);
		loadConf(mapConfig); // Initialize config
		configSwitcher->unScheduleReMap();
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
		if (strstr(argv[1], "start") != NULL)
		{
			stopD();
			clog << "Starting naga daemon in hidden mode, keep the window for the logs..." << endl;
			usleep(40000);
			(void)!(system("/usr/local/bin/Naga_Linux/nagaXinputStart.sh"));

			if (daemon(0, 1))
				perror("Couldn't daemonise from unistd");

			if (argc > 2)
				NagaDaemon(string(argv[2]).c_str());
			else
				NagaDaemon();
		}
		else if (strstr(argv[1], "kill") != NULL || strstr(argv[1], "stop") != NULL)
		{
			stopD();
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
				(void)!(system("bash /usr/local/bin/Naga_Linux/nagaUninstall.sh"));
			}
		}
	}
	else
	{
		clog << "Possible arguments : \n  -start          Starts the daemon in hidden mode. (stops it before)\n  -stop           Stops the daemon.\n  -uninstall           Uninstalls the daemon." << endl;
	}
	return 0;
}
