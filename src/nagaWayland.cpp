// This is lostallmymoney's remake of RaulPPelaez's original tool.
// RaulPPelaez, et. al wrote the original file.  As long as you retain this notice you
// can do whatever you want with this stuff.

#include <algorithm>
#include <iostream>
#include <dbus/dbus.h>
#include <vector>
#include <cstring>
#include <string_view>
#include <fstream>
#include <fcntl.h>
#include <unistd.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <sys/ioctl.h>
#include <cerrno>
#include "extraButtonCapture.hpp"
#include <thread>
#include <iomanip>
#include <mutex>
#include <map>
#include <memory>
#include <sstream>
#include <utility>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
using namespace std;

static mutex configSwitcherMutex;
static const string conf_file = string(getenv("HOME")) + "/.naga/keyMapWayland.txt";

static mutex dotoolPipeMutex;
static FILE *dotoolPipe = nullptr;

static void writeDotoolCommand(string_view command)
{
	lock_guard<mutex> lock(dotoolPipeMutex);
	if (fwrite(command.data(), 1, command.size(), dotoolPipe) != command.size() ||
		fputc('\n', dotoolPipe) == EOF)
	{
		throw runtime_error("runAndWrite Failed to write command to dotoolc");
	}

	if (fflush(dotoolPipe) == EOF)
	{
		throw runtime_error("runAndWrite Failed to flush dotoolc");
	}
}

static void closeDotoolPipe()
{
	lock_guard<mutex> lock(dotoolPipeMutex);
	if (dotoolPipe)
	{
		pclose(dotoolPipe);
		dotoolPipe = nullptr;
	}
}

static void initDotoolPipe()
{
	lock_guard<mutex> lock(dotoolPipeMutex);
	if (dotoolPipe)
	{
		return;
	}

	dotoolPipe = popen("dotoolc", "w");
	if (!dotoolPipe)
	{
		throw runtime_error("runAndWrite Failed to start dotoolc");
	}
	setvbuf(dotoolPipe, nullptr, _IOLBF, 0);
	atexit(closeDotoolPipe);
}

constexpr const char *DB_INTERFACE = "org.gnome.Shell.Extensions.WindowsExt";
constexpr const char *DB_DESTINATION = "org.gnome.Shell";
constexpr const char *DB_PATH = "/org/gnome/Shell/Extensions/WindowsExt";
constexpr const char *DB_METHOD = "FocusClass";

static string getTitle()
{
	DBusError error;
	dbus_error_init(&error);

	DBusConnection *connection = dbus_bus_get(DBUS_BUS_SESSION, &error);
	if (dbus_error_is_set(&error))
	{
		cerr << "Error connecting to bus: " << error.message << endl;
		dbus_error_free(&error);
		return "";
	}

	DBusMessage *message = dbus_message_new_method_call(DB_DESTINATION, DB_PATH, DB_INTERFACE, DB_METHOD);
	if (message == nullptr)
	{
		cerr << "Error creating message" << endl;
		return "";
	}

	DBusMessage *reply = dbus_connection_send_with_reply_and_block(connection, message, -1, &error);
	if (dbus_error_is_set(&error))
	{
		cerr << "Error calling method: " << error.message << endl;
		dbus_error_free(&error);
		return "";
	}

	char *result;
	if (!dbus_message_get_args(reply, &error, DBUS_TYPE_STRING, &result, DBUS_TYPE_INVALID))
	{
		cerr << "Error reading reply: " << error.message << endl;
		dbus_error_free(&error);
		return "";
	}

	dbus_message_unref(message);
	dbus_message_unref(reply);
	clog << "WindowNameLog : " << result << endl;
	return result;
}

class configKey
{
private:
	const string *const prefix, *const suffix;
	const bool onKeyPressed;
	void (*const internalFunction)(const string *const c);

public:
	bool IsOnKeyPressed() const { return onKeyPressed; }
	void runInternal(const string *const content) const { internalFunction(content); }
	const string *Prefix() const { return prefix; }
	const string *Suffix() const { return suffix; }
	void (*InternalFunction() const)(const string *const) { return internalFunction; }

	configKey(const bool tonKeyPressed, void (*const tinternalF)(const string *const cc), const string tcontent = "", const string tsuffix = "") : prefix(new string(tcontent)), suffix(new string(tsuffix)), onKeyPressed(tonKeyPressed), internalFunction(tinternalF)
	{
	}
};

typedef const pair<const configKey *, const string *> MacroEvent;

static map<string, map<int, map<bool, vector<MacroEvent *>>>> macroEventsKeyMaps;

class configSwitchScheduler
{
private:
	bool scheduledReMap = false, winConfigActive = false, scheduledUnlock = false, forceRecheck = false;
	const string *currentConfigName = nullptr, *scheduledReMapName = nullptr, *bckConfName = nullptr;

public:
	map<const string, pair<bool, const string *> *> *configWindowAndLockMap = new map<const string, pair<bool, const string *> *>();
	map<int, map<bool, vector<MacroEvent *>>> *currentConfigPtr;
	map<const string, pair<bool, const string *> *>::iterator currentWindowConfigPtr, scheduledUnlockWindowCfgPtr;
	map<string, const char *> notifySendMap;
	void loadConf(bool silent = false)
	{
		if (!macroEventsKeyMaps.contains(*scheduledReMapName))
		{
			clog << "Undefined profile : " << *scheduledReMapName << endl;
			return;
		}
		scheduledReMap = false;
		currentConfigName = scheduledReMapName;
		currentConfigPtr = &macroEventsKeyMaps[*scheduledReMapName];
		if (!silent)
			(void)system(notifySendMap[*scheduledReMapName]);
	}
	void checkForWindowConfig()
	{
		const string currAppClass(getTitle());
		clog << "WindowNameLog : " << currAppClass << endl;
		lock_guard<mutex> guard(configSwitcherMutex);
		if (!winConfigActive || currAppClass != currentWindowConfigPtr->first || forceRecheck)
		{
			forceRecheck = false;
			map<const string, pair<bool, const string *> *>::iterator configWindow = configWindowAndLockMap->find(currAppClass);
			if (configWindow != configWindowAndLockMap->end())
			{
				currentWindowConfigPtr = configWindow;
				if (!winConfigActive)
					bckConfName = currentConfigName;
				if (configWindow->second->first)
					scheduledReMapName = configWindow->second->second;
				else
					scheduledReMapName = &configWindow->first;
				scheduledReMap = winConfigActive = true;
				loadConf(true);
			}
			else if (winConfigActive)
			{
				scheduledReMap = true, winConfigActive = false;
				scheduledReMapName = bckConfName;
				loadConf(true);
			}
		}
	}
	void remapRoutine()
	{
		lock_guard<mutex> guard(configSwitcherMutex);
		if (scheduledUnlock)
		{
			scheduledUnlock = false;
			scheduledUnlockWindowCfgPtr->second->first = false;
			forceRecheck = true;
		}

		if (scheduledReMap)
		{
			loadConf();
		}
	}
	void scheduleReMap(const string *const reMapStr)
	{
		lock_guard<mutex> guard(configSwitcherMutex);
		if (winConfigActive)
		{
			currentWindowConfigPtr->second->first = true;
			currentWindowConfigPtr->second->second = reMapStr;
			forceRecheck = true;
		}
		else
		{
			scheduledReMapName = reMapStr;
			scheduledReMap = true;
		}
	}
	void scheduleUnlockChmap(const string *const unlockStr)
	{
		lock_guard<mutex> guard(configSwitcherMutex);
		scheduledUnlockWindowCfgPtr = configWindowAndLockMap->find(*unlockStr);
		if (scheduledUnlockWindowCfgPtr != configWindowAndLockMap->end() && scheduledUnlockWindowCfgPtr->second->first)
		{
			scheduledUnlock = true;
		}
	}
};

static configSwitchScheduler *const configSwitcher = new configSwitchScheduler();

class NagaDaemon
{
private:
	static constexpr size_t BufferSize = 1024;
	static constexpr size_t DotoolCommandLimit = 65536;
	map<string, configKey *const> configKeysMap;

	string currentConfigName;
	struct input_event ev1[64];
	const int size = sizeof(ev1);
	vector<pair<const char *const, const char *const>> devices;
	bool areSideBtnEnabled = true, areExtraBtnEnabled = true;

	unique_ptr<UInputForwarder> extraForwarder;
	bool extraDeviceGrabbed = false;

	void initConf()
	{
		string commandContent;
		map<int, map<bool, vector<MacroEvent *>>> *iteratedConfig;
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
						buttonNumberInt = stoi(*buttonNumber) + 1; //+1 here so no need to do it at runtime
					}
					catch (...)
					{
						clog << "CONFIG ERROR : " << commandContent << endl;
						exit(1);
					}

					map<bool, vector<MacroEvent *>> *iteratedButtonConfig = &(*iteratedConfig)[buttonNumberInt];

					if (configKeysMap.contains(commandType))
					{ // filter out bad types
						if (!configKeysMap[commandType]->Prefix()->empty())
							commandContent = *configKeysMap[commandType]->Prefix() + commandContent;

						if (!configKeysMap[commandType]->Suffix()->empty())
							commandContent = commandContent + *configKeysMap[commandType]->Suffix();

						(*iteratedButtonConfig)[configKeysMap[commandType]->IsOnKeyPressed()].emplace_back(new MacroEvent(configKeysMap[commandType], new string(commandContent)));
						// Encode and store mapping v3
					}
					else if (commandType == "key")
					{
						string *commandContent2 = new string(*configKeysMap["keyreleaseonrelease"]->Prefix() + commandContent + *configKeysMap["keyreleaseonrelease"]->Suffix());
						commandContent = *configKeysMap["keypressonpress"]->Prefix() + commandContent + *configKeysMap["keypressonpress"]->Suffix();
						(*iteratedButtonConfig)[true].emplace_back(new MacroEvent(configKeysMap["keypressonpress"], new string(commandContent)));
						(*iteratedButtonConfig)[false].emplace_back(new MacroEvent(configKeysMap["keyreleaseonrelease"], new string(*commandContent2)));
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
				(*configSwitcher->configWindowAndLockMap)[commandContent] = new pair<bool, const string *>(false, new string(""));
				configSwitcher->notifySendMap.emplace(commandContent, (new string("notify-send \"Profile : " + commandContent + "\""))->c_str());
			}
			else if (commandContent.substr(0, 7) == "config=")
			{
				isIteratingConfig = true;
				commandContent.erase(0, 7);
				iteratedConfig = &macroEventsKeyMaps[commandContent];
				configSwitcher->notifySendMap.emplace(commandContent, (new string("notify-send \"Profile : " + commandContent + "\""))->c_str());
			}
		}
		in.close();
	}

	int side_btn_fd, extra_btn_fd;
	fd_set readset;

	void run()
	{
		if (areSideBtnEnabled)
			ioctl(side_btn_fd, EVIOCGRAB, 1); // Give application exclusive control over side buttons.
		while (1)
		{
			configSwitcher->remapRoutine();

			FD_ZERO(&readset);
			if (areSideBtnEnabled)
				FD_SET(side_btn_fd, &readset);
			if (areExtraBtnEnabled)
				FD_SET(extra_btn_fd, &readset);
			if (select(FD_SETSIZE, &readset, nullptr, nullptr, nullptr) == -1)
				exit(2);

			if (areSideBtnEnabled && FD_ISSET(side_btn_fd, &readset)) // Side buttons
			{
				ssize_t bytesRead = read(side_btn_fd, ev1, size);
				if (bytesRead == -1)
					exit(2);
				if (bytesRead % sizeof(input_event) != 0)
				{
					continue;
				}
				size_t eventCount = static_cast<size_t>(bytesRead) / sizeof(input_event);
				for (size_t i = 0; i < eventCount; ++i)
				{
					const input_event &event = ev1[i];
					if (event.type != EV_KEY)
					{
						continue;
					}
					if (event.code < 2 || event.code > 13)
					{
						continue;
					}
					if (event.value != 0 && event.value != 1)
					{
						continue;
					}

					configSwitcher->checkForWindowConfig();
					thread(runActions, &(*configSwitcher->currentConfigPtr)[event.code][event.value == 1]).detach();
				}
			}
			if (areExtraBtnEnabled && FD_ISSET(extra_btn_fd, &readset)) // Extra buttons
			{
				ssize_t bytesRead = read(extra_btn_fd, ev1, size);
				if (bytesRead == -1)
					exit(2);
				size_t eventCount = static_cast<size_t>(bytesRead) / sizeof(input_event);
				for (size_t i = 0; i < eventCount; ++i)
				{
					const input_event &event = ev1[i];
					if (event.type == EV_KEY)
					{
						if (event.code == 275 || event.code == 276)
						{
							configSwitcher->checkForWindowConfig();
							thread(runActions, &(*configSwitcher->currentConfigPtr)[event.code - 261][event.value == 1]).detach();
							continue;
						}
						else if (extraDeviceGrabbed && extraForwarder)
						{
							extraForwarder->forward(event);
							continue;
						}
					}
					else if (extraDeviceGrabbed && extraForwarder)
					{
						extraForwarder->forward(event);
					}
				}
			}
		}
	}

	// Functions that can be given to configKeys

	static void chmapNow(const string *const macroContent)
	{
		configSwitcher->scheduleReMap(macroContent); // schedule config switch/change
	}

	static void sleepNow(const string *const macroContent)
	{
		usleep(stoul(*macroContent) * 1000); // microseconds make me dizzy in keymap.txt
	}
	static void dotoolKeydown(const string *const macroContent)
	{
		writeDotoolCommand("keydown " + *macroContent);
	}

	static void dotoolKeyup(const string *const macroContent)
	{
		writeDotoolCommand("keyup " + *macroContent);
	}

	static void dotoolKey(const string *const macroContent)
	{
		writeDotoolCommand("key " + *macroContent);
	}

	static void dotoolType(const string *const macroContent)
	{
		writeDotoolCommand("type " + *macroContent);
	}
	static void runAndWrite(const string *const macroContent)
	{
		unique_ptr<FILE, int (*)(FILE *)> pipe(popen(macroContent->c_str(), "r"), &pclose);
		if (!pipe)
		{
			throw runtime_error("runAndWrite execution Failed !");
		}

		string currentLine;

		int ch = 0;
		while ((ch = fgetc(pipe.get())) != EOF)
		{
			if (ch == '\n')
			{
				writeDotoolCommand("type " + currentLine);
				writeDotoolCommand("key enter");
				writeDotoolCommand("key home");
				currentLine.clear();
			}
			else
			{
				if (currentLine.size() == DotoolCommandLimit)
				{
					writeDotoolCommand("type " + currentLine);
					currentLine.clear();
				}
				currentLine.push_back(static_cast<char>(ch));
			}
		}

		if (!currentLine.empty())
		{
			writeDotoolCommand("type " + currentLine);
		}
	}
	static void runAndWriteThread(const string *const macroContent)
	{
		thread(runAndWrite, macroContent).detach();
	}

	static void executeNow(const string *const macroContent)
	{
		(void)system(macroContent->c_str());
	}
	static void executeThreadNow(const string *const macroContent)
	{
		thread(executeNow, macroContent).detach();
	}
	// end of configKeys functions

	static void runActions(vector<MacroEvent *> *const relativeMacroEventsPointer)
	{
		for (MacroEvent *const &macroEvent : *relativeMacroEventsPointer)
		{ // run all the events at Key
			macroEvent->first->runInternal(macroEvent->second);
		}
	}

	void emplaceConfigKey(const string &key, bool onKeyPressed, void (*functionPtr)(const string *const), const string &prefix = "", const string &suffix = "")
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
		devices.emplace_back("/dev/input/by-id/usb-Razer_Razer_Naga_Left_Handed_Edition-if02-event-kbd", "/dev/input/by-id/usb-Razer_Razer_Naga_Left_Handed_Edition-event-mouse");	 // NAGA Left Handed THANKS TO https://github.com/Izaya-San
		devices.emplace_back("/dev/input/by-id/usb-Razer_Razer_Naga_Pro_000000000000-if02-event-kbd", "/dev/input/by-id/usb-Razer_Razer_Naga_Pro_000000000000-event-mouse");		 // NAGA PRO WIRELESS
		devices.emplace_back("/dev/input/by-id/usb-1532_Razer_Naga_Pro_000000000000-if02-event-kbd", "/dev/input/by-id/usb-1532_Razer_Naga_Pro_000000000000-event-mouse");			 // NAGA PRO
		devices.emplace_back("/dev/input/by-id/usb-Razer_Razer_Naga_V2_Pro-if02-event-kbd", "/dev/input/by-id/usb-Razer_Razer_Naga_V2_Pro-event-mouse");							 // NAGA V2 THANKS TO https://github.com/ibarrick
		devices.emplace_back("/dev/input/by-id/usb-Razer_Razer_Naga_X-if02-event-kbd", "/dev/input/by-id/usb-Razer_Razer_Naga_X-event-mouse");										 // NAGA X THANKS TO https://github.com/bgrabow

		// devices.emplace_back("/dev/input/by-id/YOUR_DEVICE_FILE", "/dev/input/by-id/YOUR_DEVICE_FILE#2");		 // DUMMY EXAMPLE, ONE CAN BE EMPTY LIKE SUCH : ""  (for devices with no extra buttons)

		bool isThereADevice = false;
		for (pair<const char *const, const char *const> &device : devices)
		{ // Setup check
			side_btn_fd = open(device.first, O_RDONLY), extra_btn_fd = open(device.second, O_RDONLY);

			if (side_btn_fd != -1 || extra_btn_fd != -1)
			{
				if (side_btn_fd == -1)
				{
					clog << "Reading from: " << device.second << endl;
					areSideBtnEnabled = false;
				}
				else if (extra_btn_fd == -1)
				{
					clog << "Reading from: " << device.first << endl;
					areExtraBtnEnabled = false;
				}
				else
				{
					clog << "Reading from: " << device.first << endl
						 << " and " << device.second << endl;
				}
				isThereADevice = true;
				break;
			}
		}

		if (!isThereADevice)
		{
			cerr << "No naga devices found or you don't have permission to access them." << endl;
			exit(1);
		}

		if (areExtraBtnEnabled)
		{
			extraForwarder.reset(new UInputForwarder());
			if (!extraForwarder->init(extra_btn_fd))
			{
				extraForwarder.reset();
				clog << "[naga] uinput forwarding disabled; continuing without exclusive extra buttons." << endl;
			}
			else if (ioctl(extra_btn_fd, EVIOCGRAB, 1) == -1)
			{
				clog << "[naga] failed to grab extra button device: " << strerror(errno) << endl;
				extraForwarder.reset();
			}
			else
			{
				extraDeviceGrabbed = true;
				clog << "[naga] extra buttons grabbed; pointer events forwarded via uinput." << endl;
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

		emplaceConfigKey("runandwrite", ONKEYPRESSED, runAndWriteThread);
		emplaceConfigKey("runandwrite2", ONKEYPRESSED, runAndWrite);

		emplaceConfigKey("runrelease", ONKEYRELEASED, executeThreadNow);
		emplaceConfigKey("runrelease2", ONKEYRELEASED, executeNow);

		emplaceConfigKey("launch", ONKEYRELEASED, executeThreadNow, "gtk-launch ");
		emplaceConfigKey("launch2", ONKEYRELEASED, executeNow, "gtk-launch ");

		emplaceConfigKey("keypressonpress", ONKEYPRESSED, dotoolKeydown);
		emplaceConfigKey("keypressonrelease", ONKEYRELEASED, dotoolKeydown);

		emplaceConfigKey("keyreleaseonpress", ONKEYPRESSED, dotoolKeyup);
		emplaceConfigKey("keyreleaseonrelease", ONKEYRELEASED, dotoolKeyup);

		emplaceConfigKey("keyclick", ONKEYPRESSED, dotoolKey);
		emplaceConfigKey("keyclickrelease", ONKEYRELEASED, dotoolKey);

		emplaceConfigKey("string", ONKEYPRESSED, dotoolType);
		emplaceConfigKey("stringrelease", ONKEYRELEASED, dotoolType);

		initConf();

		configSwitcher->scheduleReMap(&mapConfig);
		configSwitcher->loadConf(); // Initialize config

		run();
	}
};

void stopD()
{
	clog << "Stopping possible naga daemon" << endl;
	(void)system(("/usr/local/bin/Naga_Linux/nagaKillroot.sh " + to_string((int)getpid())).c_str());
};

// arguments manage
int main(const int argc, const char *const argv[])
{
	if (argc > 1)
	{
		if (strstr(argv[1], "serviceHelper"))
		{
			stopD();
			(void)system("/usr/local/bin/Naga_Linux/nagaXinputStart.sh");
			initDotoolPipe();
			if (argc > 2 && argv[2][0] != '\0')
				NagaDaemon(argv[2]); // lets you configure a default profile in /etc/systemd/system/naga.service
			else
				NagaDaemon();
		}
		else if (strstr(argv[1], "start"))
		{
			clog << "Starting naga daemon as service, naga debug to see logs..." << endl;
			usleep(100000);
			(void)system("sudo systemctl restart naga");
		}
		else if (strstr(argv[1], "debug"))
		{
			clog << "Starting naga debug, logs :" << endl;
			if (argc > 2)
			{
				(void)system(("journalctl " + string(argv[2]) + " naga").c_str());
			}
			else
			{
				(void)system("journalctl -fu naga");
			}
		}
		else if (strstr(argv[1], "kill") || strstr(argv[1], "stop"))
		{
			clog << "Stopping possible naga daemon" << endl;
			(void)system(("sudo sh /usr/local/bin/Naga_Linux/nagaKillroot.sh " + to_string((int)getpid())).c_str());
		}
		else if (strstr(argv[1], "repair") || strstr(argv[1], "tame") || strstr(argv[1], "fix"))
		{
			clog << "Fixing dead keypad syndrome... STUTTER!!" << endl;
			(void)system("sudo bash -c \"sh /usr/local/bin/Naga_Linux/nagaKillroot.sh && modprobe -r usbhid && modprobe -r psmouse && modprobe usbhid && modprobe psmouse && sleep 1 && sudo systemctl start naga\"");
		}
		else if (strstr(argv[1], "edit"))
		{
			if (argc > 2)
			{
				(void)system(("sudo bash -c 'orig_sum=\"$(sudo md5sum " + conf_file + ")\"; " + string(argv[2]) + " " + conf_file + "; [[ \"$(sudo md5sum " + conf_file + ")\" != \"$orig_sum\" ]] && sudo systemctl restart naga'").c_str());
			}
			else
			{
				(void)system(("sudo bash -c 'orig_sum=\"$(sudo md5sum " + conf_file + ")\"; sudo nano -m " + conf_file + "; [[ \"$(sudo md5sum " + conf_file + ")\" != \"$orig_sum\" ]] && sudo systemctl restart naga'").c_str());
			}
		}
		else if (strstr(argv[1], "uninstall"))
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
				(void)system("/usr/local/bin/Naga_Linux/nagaUninstall.sh");
			}
		}
	}
	else
	{
		clog << "Possible arguments : \n  start          Starts the daemon in hidden mode. (stops it before)\n  stop           Stops the daemon.\n  edit           Lets you edit the config.\n  debug		 Shows log.\n  fix            For dead keypad.\n  uninstall      Uninstalls the daemon." << endl;
	}
	return 0;
}
