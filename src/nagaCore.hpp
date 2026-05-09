// This is lostallmymoney's remake of RaulPPelaez's original tool.
// RaulPPelaez, et. al wrote the original file.  As long as you retain this notice you
// can do whatever you want with this stuff.

#define NAGA_CORE_HPP

#include "extraButtonCapture.hpp"

#include <iostream>
#include <condition_variable>
#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <map>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <functional>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <array>
#include <atomic>
#include <fcntl.h>
#include <unistd.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <sys/ioctl.h>

using namespace std;

/**
 * IMacroEvent - Abstract base class for all macro events
 */
class IMacroEvent
{
public:
	virtual ~IMacroEvent() = default;
	virtual void runInternal() const = 0;
};

/**
 * nagaCommandClass - Represents a command that can be executed
 * Holds prefix/suffix strings and a function pointer to the implementation
 */
class nagaCommandClass
{
private:
	const string prefix, suffix;
	const bool onKeyPressed;
	void (*const internalFunction)(const string &c);

public:
	bool IsOnKeyPressed() const { return onKeyPressed; }
	void run(const string &content) const { internalFunction(content); }

	std::string generateCommand(const std::string &commandContent) const
	{
		return prefix + commandContent + suffix;
	}

	nagaCommandClass(const bool tonKeyPressed, void (*const tinternalF)(const string &cc), const string &tprefix = "", const string &tsuffix = "") : prefix(tprefix), suffix(tsuffix), onKeyPressed(tonKeyPressed), internalFunction(tinternalF)
	{
	}
};

/**
 * MacroEvent - Concrete implementation of IMacroEvent
 * Executes a nagaCommand with specific arguments
 */
class MacroEvent : public IMacroEvent
{
private:
	const nagaCommandClass &command;
	const string commandArgument;

public:
	MacroEvent(const nagaCommandClass &commandRef, const string &commandArgumentRef) : command(commandRef), commandArgument(commandArgumentRef)
	{
	}
	void runInternal() const override
	{
		command.run(commandArgument);
	}
};

/**
 * loop - Manages a sequence of macro events that can be executed
 * Supports atomic operations for thread-safe control
 */
class loop
{
private:
	mutable std::atomic<uint32_t> generation{0};
	mutable std::atomic<bool> globalStop{false};
	mutable std::atomic<bool> toggled{false};
	size_t eventCount{0};
	std::vector<IMacroEvent *> eventList;
	std::vector<IMacroEvent *> exitEventList;

public:
	void addEvent(IMacroEvent *const newEvent)
	{
		eventList.emplace_back(newEvent);
		++eventCount; // not atomic; safe since no concurrent adds
	}

	void addExitEvent(IMacroEvent *const newEvent)
	{
		exitEventList.emplace_back(newEvent);
	}

	void stop() const noexcept
	{
		for (IMacroEvent *const exitEvent : exitEventList)
		{
			exitEvent->runInternal();
		}
		globalStop.store(true, std::memory_order_release);
	}

	void run() const
	{
		uint32_t myGen = generation.fetch_add(1, std::memory_order_acq_rel) + 1;
		globalStop.store(false, std::memory_order_release);

		size_t index = 0;

		while (true)
		{
			if (!(index < eventCount))
				index = 0;

			eventList[index]->runInternal();
			++index;

			if (globalStop.load(std::memory_order_acquire) || generation.load(std::memory_order_acquire) != myGen)
				break;
		}
	}

	/**
	 * Toggle the loop's running state.
	 * If the loop is currently running (toggled), this will stop it.
	 * If the loop is not running, this will start it and set toggled to true for the duration.
	 * No threading is involved; toggle is synchronous.
	 */
	void toggle() const
	{
		if (toggled.load(std::memory_order_acquire))
		{
			this->stop();
		}
		else
		{
			toggled.store(true, std::memory_order_release);
			this->run();
			toggled.store(false, std::memory_order_release);
		}
	}

	void runThisManyTimes(size_t times) const
	{
		uint32_t myGen = generation.fetch_add(1, std::memory_order_acq_rel) + 1;
		globalStop.store(false, std::memory_order_release);

		size_t index = 0;
		size_t runCount = 0;

		while (true)
		{
			if (!(index < eventCount))
			{
				index = 0;
				++runCount;
				if (runCount == times)
					break;
			}

			eventList[index]->runInternal();
			++index;

			if (globalStop.load(std::memory_order_acquire) || generation.load(std::memory_order_acquire) != myGen)
				break;
		}
	}

	void runThisManyTimesWithoutStop(size_t times) const
	{
		size_t index = 0;
		size_t runCount = 0;

		while (true)
		{
			if (!(index < eventCount))
			{
				index = 0;
				++runCount;
				if (runCount == times)
					break;
			}

			eventList[index]->runInternal();
			++index;
		}
	}
};

/**
 * loopMacroEvent - Event that controls loop execution
 * Supports start, stop, toggle operations and repeat counts
 */
class loopMacroEvent : public IMacroEvent
{
protected:
	const loop &aNagaLoop;
	function<void()> loopAction;

public:
	loopMacroEvent(const loop &taNagaLoop, const string &taNagaLoopArgument) : aNagaLoop(taNagaLoop)
	{
		if (taNagaLoopArgument == "start")
		{
			loopAction = [&loopRef = aNagaLoop]()
			{ loopRef.run(); };
		}
		else if (taNagaLoopArgument == "stop")
		{
			loopAction = [&loopRef = aNagaLoop]()
			{ loopRef.stop(); };
		}
		else if (taNagaLoopArgument == "toggle" || taNagaLoopArgument == "togglerelease")
		{
			loopAction = [&loopRef = aNagaLoop]()
			{ loopRef.toggle(); };
		}
		else
		{
			try
			{
				const long long parsedTimes = stoll(taNagaLoopArgument);
				if (parsedTimes > 0)
				{
					const size_t convertedTimes = static_cast<size_t>(parsedTimes);
					loopAction = [&loopRef = aNagaLoop, convertedTimes]()
					{ loopRef.runThisManyTimes(convertedTimes); };
				}
				else if (parsedTimes < 0)
				{
					const size_t convertedTimes = static_cast<size_t>(-parsedTimes);
					loopAction = [&loopRef = aNagaLoop, convertedTimes]()
					{ loopRef.runThisManyTimesWithoutStop(convertedTimes); };
				}
				else
				{
					clog << "\033[93mWarning : Invalid loop argument (zero): " << taNagaLoopArgument << "\033[0m" << '\n';
				}
			}
			catch (...)
			{
				clog << "\033[93mWarning : Invalid loop argument: " << taNagaLoopArgument << "\033[0m" << '\n';
			}
		}
	}

	void runInternal() const override
	{
		loopAction();
	}
};

/**
 * ThreadedLoopMacroEvent - Loop event that executes in a separate thread
 */
class ThreadedLoopMacroEvent : public loopMacroEvent
{
public:
	using loopMacroEvent::loopMacroEvent;

	void runInternal() const override
	{
		thread loopThread([this]()
						  { loopAction(); });
		loopThread.detach();
	}
};

// Type aliases for key mapping structures
using IMacroEventKeyMap = map<int, map<bool, vector<IMacroEvent *>>>;

/**
 * ParsedCommand - Represents a parsed command from configuration
 */
struct ParsedCommand
{
	bool isOnKeyPressed;
	IMacroEvent &macroEvent;
	bool allowedOnReleaseAtLoopExit;

	ParsedCommand(const bool tisOnKeyPressed, IMacroEvent &tmacroEvent, const bool tallowedOnReleaseInLoop = false)
		: isOnKeyPressed(tisOnKeyPressed), macroEvent(tmacroEvent), allowedOnReleaseAtLoopExit(tallowedOnReleaseInLoop)
	{
	}
};

using ParsedCommandList = vector<ParsedCommand>;
using ParsedCommandPointerList = vector<const ParsedCommand *>;

// Global data structures for key mappings, loops, and functions
inline unordered_map<string, IMacroEventKeyMap> IMacroEventKeyMaps;
inline unordered_map<string, loop *> loopsMap;
inline unordered_map<string, vector<string>> contextMap;

/**
 * nagaFunction - Container for a sequence of macro events
 */
class nagaFunction
{
public:
	vector<IMacroEvent *> eventList;
	void addEvent(IMacroEvent *const newEvent)
	{
		eventList.emplace_back(newEvent);
	}
};

inline unordered_map<string, nagaFunction *> functionsMap;

inline mutex configSwitcherMutex;

/**
 * WindowConfigLock - Represents a locked window configuration
 */
struct WindowConfigLock
{
	bool isLocked;
	const string *lockedConfigName;
};

using WindowConfigMap = unordered_map<string, WindowConfigLock *>;

/**
 * Platform-specific abstraction for getting the active window title
 * Must be implemented by each platform (X11 or Wayland)
 */
extern string getActiveWindowTitle();
extern string conf_file;
extern void platformRunAndWrite(const string &macroContent);
extern void initAndRegisterPlatformCommands();

namespace configSwitcher
{
	bool scheduledReMap = false, winConfigActive = false, scheduledUnlock = false, forceRecheck = false, notifyOnNextLoad = false;
	const string *currentConfigName = nullptr, *scheduledReMapName = nullptr, *bckConfName = nullptr;
	string lastLoggedWindow;
	WindowConfigMap *configWindowAndLockMap = new unordered_map<string, WindowConfigLock *>();
	IMacroEventKeyMap *currentConfigPtr = nullptr;
	WindowConfigMap::iterator currentWindowConfigPtr, scheduledUnlockWindowCfgPtr;
	unordered_map<string, const char *> notifySendMap;

	static void loadConf(bool silent = false)
	{
		if (notifyOnNextLoad)
			silent = false;

		scheduledReMap = notifyOnNextLoad = false;
		unordered_map<std::string, IMacroEventKeyMap>::iterator scheduledConfig = IMacroEventKeyMaps.find(*scheduledReMapName);
		if (scheduledConfig == IMacroEventKeyMaps.end())
		{
			clog << "\033[93mWarning : Undefined profile : " << *scheduledReMapName << "\033[0m" << '\n';
			return;
		}
		currentConfigName = scheduledReMapName;
		currentConfigPtr = &scheduledConfig->second;
		if (!silent)
			std::ignore = system(notifySendMap[*scheduledReMapName]);
	}

	static void checkForWindowConfig()
	{
		const string currAppClass(getActiveWindowTitle());
		if (currAppClass != lastLoggedWindow)
		{
			clog << "\033[35mInfo : WindowName : " << currAppClass << "\033[0m" << '\n';
			lastLoggedWindow = currAppClass;
		}
		lock_guard<mutex> guard(configSwitcherMutex);
		if (!winConfigActive || currAppClass != currentWindowConfigPtr->first || forceRecheck)
		{
			forceRecheck = false;
			WindowConfigMap::iterator configWindow = configWindowAndLockMap->find(currAppClass);
			if (configWindow != configWindowAndLockMap->end())
			{
				const string &windowName = configWindow->first;
				WindowConfigLock *const &windowConfigLock = configWindow->second;
				currentWindowConfigPtr = configWindow;
				if (!winConfigActive)
					bckConfName = currentConfigName;
				if (windowConfigLock->isLocked)
					scheduledReMapName = windowConfigLock->lockedConfigName;
				else
					scheduledReMapName = &windowName;
				winConfigActive = true;
				loadConf(true);
			}
			else if (winConfigActive)
			{
				winConfigActive = false;
				scheduledReMapName = bckConfName;
				loadConf(true);
			}
		}
	}

	static void remapRoutine()
	{
		lock_guard<mutex> guard(configSwitcherMutex);
		if (scheduledUnlock)
		{
			WindowConfigLock *const &windowConfigLock = scheduledUnlockWindowCfgPtr->second;
			windowConfigLock->isLocked = scheduledUnlock = false;
			forceRecheck = notifyOnNextLoad = true;
		}

		if (scheduledReMap)
		{
			loadConf();
		}
	}

	static void scheduleReMap(const string &reMapStr)
	{
		lock_guard<mutex> guard(configSwitcherMutex);
		if (winConfigActive)
		{
			WindowConfigLock *const &windowConfigLock = currentWindowConfigPtr->second;
			windowConfigLock->isLocked = forceRecheck = notifyOnNextLoad = true;
			windowConfigLock->lockedConfigName = &reMapStr;
		}
		else
		{
			scheduledReMapName = &reMapStr;
			scheduledReMap = true;
		}
	}

	static void scheduleUnlockChmap(const string &unlockStr)
	{
		bool shouldRecheck = false;
		{
			lock_guard<mutex> guard(configSwitcherMutex);
			scheduledUnlockWindowCfgPtr = configWindowAndLockMap->find(unlockStr);
			if (scheduledUnlockWindowCfgPtr != configWindowAndLockMap->end())
			{
				WindowConfigLock *const &windowConfigLock = scheduledUnlockWindowCfgPtr->second;
				if (windowConfigLock->isLocked)
				{
					scheduledUnlock = shouldRecheck = true;
				}
			}
		}
		if (shouldRecheck)
			checkForWindowConfig();
	}
}

namespace NagaDaemon
{
	static constexpr bool OnKeyPressed = true;
	static constexpr bool OnKeyReleased = false;
	static constexpr size_t BufferSize = 1024;
	static unordered_map<string, nagaCommandClass *const> nagaCommandsMap;

	static constexpr size_t input_event_size = sizeof(input_event);
	static struct input_event side_ev[64];
	static struct input_event extra_ev[64];
	static const size_t side_ev_size = static_cast<size_t>(sizeof(side_ev));
	static const size_t extra_ev_size = static_cast<size_t>(sizeof(extra_ev));
	static constexpr std::array<pair<const char *, const char *>, 16> devices = {{
		{"/dev/input/by-id/usb-Razer_Razer_Naga_Epic-if01-event-kbd", "/dev/input/by-id/usb-Razer_Razer_Naga_Epic-event-mouse"},
		{"/dev/input/by-id/usb-Razer_Razer_Naga_Epic_Dock-if01-event-kbd", "/dev/input/by-id/usb-Razer_Razer_Naga_Epic_Dock-event-mouse"},
		{"/dev/input/by-id/usb-Razer_Razer_Naga_2014-if02-event-kbd", "/dev/input/by-id/usb-Razer_Razer_Naga_2014-event-mouse"},
		{"/dev/input/by-id/usb-Razer_Razer_Naga-if01-event-kbd", "/dev/input/by-id/usb-Razer_Razer_Naga-event-mouse"},
		{"/dev/input/by-id/usb-Razer_Razer_Naga_Epic_Chroma-if01-event-kbd", "/dev/input/by-id/usb-Razer_Razer_Naga_Epic_Chroma-event-mouse"},
		{"/dev/input/by-id/usb-Razer_Razer_Naga_Epic_Chroma_Dock-if01-event-kbd", "/dev/input/by-id/usb-Razer_Razer_Naga_Epic_Chroma_Dock-event-mouse"},
		{"/dev/input/by-id/usb-Razer_Razer_Naga_Chroma-if02-event-kbd", "/dev/input/by-id/usb-Razer_Razer_Naga_Chroma-event-mouse"},
		{"/dev/input/by-id/usb-Razer_Razer_Naga_Hex-if01-event-kbd", "/dev/input/by-id/usb-Razer_Razer_Naga_Hex-event-mouse"},
		{"/dev/input/by-id/usb-Razer_Razer_Naga_Hex_V2-if02-event-kbd", "/dev/input/by-id/usb-Razer_Razer_Naga_Hex_V2-event-mouse"},
		{"/dev/input/by-id/usb-Razer_Razer_Naga_Trinity_00000000001A-if02-event-kbd", "/dev/input/by-id/usb-Razer_Razer_Naga_Trinity_00000000001A-event-mouse"},
		{"/dev/input/by-id/usb-Razer_Razer_Naga_Left_Handed_Edition-if02-event-kbd", "/dev/input/by-id/usb-Razer_Razer_Naga_Left_Handed_Edition-event-mouse"},
		{"/dev/input/by-id/usb-Razer_Razer_Naga_Pro_000000000000-if02-event-kbd", "/dev/input/by-id/usb-Razer_Razer_Naga_Pro_000000000000-event-mouse"},
		{"/dev/input/by-id/usb-1532_Razer_Naga_Pro_000000000000-if02-event-kbd", "/dev/input/by-id/usb-1532_Razer_Naga_Pro_000000000000-event-mouse"},
		{"/dev/input/by-id/usb-Razer_Razer_Naga_V2_Pro-if02-event-kbd", "/dev/input/by-id/usb-Razer_Razer_Naga_V2_Pro-event-mouse"},
		{"/dev/input/by-id/usb-Razer_Razer_Naga_X-if02-event-kbd", "/dev/input/by-id/usb-Razer_Razer_Naga_X-event-mouse"},
		{"/dev/input/by-id/usb-Razer_Razer_Naga_V2_HyperSpeed_000000000000-if02-event-kbd", "/dev/input/by-id/usb-Razer_Razer_Naga_V2_HyperSpeed_000000000000-event-mouse"},
	}};
	static bool areSideBtnEnabled = true, areExtraBtnEnabled = true;

	static unique_ptr<UInputForwarder> extraForwarder;
	static bool extraDeviceGrabbed = false;
	static int side_btn_fd, extra_btn_fd;

	static void emplaceConfigKey(const string &nagaCommand, bool onKeyPressed, void (*functionPtr)(const string &), const string &prefix = "", const string &suffix = "")
	{
		nagaCommandsMap.emplace(nagaCommand, new nagaCommandClass(onKeyPressed, functionPtr, prefix, suffix));
	}

	static void chmapNow(const string &macroContent)
	{
		configSwitcher::scheduleReMap(macroContent);
	}

	static void unlockChmap(const string &macroContent)
	{
		configSwitcher::scheduleUnlockChmap(macroContent);
	}

	static void randomSleepNow(const string &macroContent)
	{
		usleep(static_cast<useconds_t>((rand() % (stoul(macroContent) + 1)) * 1000));
	}

	static void sleepNow(const string &macroContent)
	{
		usleep(static_cast<useconds_t>(stoul(macroContent) * 1000));
	}

	static void executeNow(const string &macroContent)
	{
		std::ignore = system(macroContent.c_str());
	}

	static void executeThreadNow(const string &macroContent)
	{
		thread(executeNow, std::ref(macroContent)).detach();
	}

	static void runAndWriteThread(const string &macroContent)
	{
		thread(platformRunAndWrite, std::ref(macroContent)).detach();
	}

	static void registerCoreCommands()
	{
		emplaceConfigKey("chmap", OnKeyPressed, chmapNow);
		emplaceConfigKey("chmaprelease", OnKeyReleased, chmapNow);

		emplaceConfigKey("sleep", OnKeyPressed, sleepNow);
		emplaceConfigKey("sleeprelease", OnKeyReleased, sleepNow);

		emplaceConfigKey("randomsleep", OnKeyPressed, randomSleepNow);
		emplaceConfigKey("randomsleeprelease", OnKeyReleased, randomSleepNow);

		emplaceConfigKey("run", OnKeyPressed, executeThreadNow);
		emplaceConfigKey("run2", OnKeyPressed, executeNow);

		emplaceConfigKey("runrelease", OnKeyReleased, executeThreadNow);
		emplaceConfigKey("runrelease2", OnKeyReleased, executeNow);

		emplaceConfigKey("runandwrite", OnKeyPressed, runAndWriteThread);
		emplaceConfigKey("runandwrite2", OnKeyPressed, platformRunAndWrite);
		emplaceConfigKey("runandwriterelease", OnKeyReleased, runAndWriteThread);
		emplaceConfigKey("runandwriterelease2", OnKeyReleased, platformRunAndWrite);

		emplaceConfigKey("unlockchmap", OnKeyPressed, unlockChmap);
		emplaceConfigKey("unlockchmaprelease", OnKeyReleased, unlockChmap);

		emplaceConfigKey("launch", OnKeyReleased, executeThreadNow, "gtk-launch ");
		emplaceConfigKey("launch2", OnKeyReleased, executeNow, "gtk-launch ");
	}

	static void initConf()
	{
		string commandContent, commandContent2;
		IMacroEventKeyMap *iteratedConfig;
		bool isIteratingConfig = false, isIteratingLoop = false, isIteratingFunction = false, isIteratingContext = false, isWindowConfig = false;

		ifstream in(conf_file.c_str(), ios::in);
		if (!in)
		{
			std::cerr << "\033[91mError : Cannot open " << conf_file << ". Exiting.\033[0m\n";
			std::exit(1);
		}

		bool (*const shouldIgnoreLine)(const string &) = [](const string &line) -> bool
		{
			const string::size_type first = line.find_first_not_of(' ');
			return first == string::npos || line[first] == '#';
		};

		const std::function<void(std::string &)> nukeWhitespaces = [](std::string &value)
		{
			value.erase(std::remove_if(value.begin(), value.end(),
									   [](unsigned char c)
									   { return std::isspace(c); }),
						value.end());
		};
		const std::function<void(std::string &)> trimSpaces = [](std::string &value)
		{
			const std::string::size_type start = value.find_first_not_of(" \t\r\n");
			if (start == std::string::npos)
			{
				value.clear();
				return;
			}
			const std::string::size_type end = value.find_last_not_of(" \t\r\n");
			value = value.substr(start, end - start + 1);
		};

		const std::function<void(std::string &)> normalizeCommandType = [&nukeWhitespaces](std::string &value)
		{
			nukeWhitespaces(value);
			std::transform(value.begin(), value.end(), value.begin(),
						   [](unsigned char c)
						   { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); });
		};

		const std::function<int(const std::string &)> getButtonNumber = [](const std::string &configLine) -> int
		{
			std::string::size_type dashPos = configLine.find('-');
			if (dashPos == std::string::npos)
				return -1;
			try
			{
				return std::stoi(configLine.substr(0, dashPos)) + 1;
			}
			catch (...)
			{
				return -1;
			}
		};

		const std::function<std::string(const std::string &)> getCommandType = [](const std::string &configLine) -> std::string
		{
			std::string::size_type equalPos = configLine.find('=');
			if (equalPos == std::string::npos)
				return configLine;
			return configLine.substr(0, equalPos);
		};

		const std::function<void(std::string &)> cleaveButtonNumber = [](std::string &configLine)
		{
			std::string::size_type dashPos = configLine.find('-');
			if (dashPos == std::string::npos)
				return;
			configLine = configLine.substr(dashPos + 1);
			configLine.erase(0, configLine.find_first_not_of(' '));
		};

		const std::function<void(std::string &)> cleaveCommandType = [](std::string &configLine)
		{
			std::string::size_type equalPos = configLine.find('=');
			if (equalPos == std::string::npos)
				return;
			configLine = configLine.substr(equalPos + 1);
		};

		const std::function<ParsedCommandPointerList(const ParsedCommandList &)> getOnReleaseCommands = [](const ParsedCommandList &commands) -> ParsedCommandPointerList
		{
			ParsedCommandPointerList result;
			for (const ParsedCommand &command : commands)
			{
				if (!command.isOnKeyPressed)
					result.emplace_back(&command);
			}
			return result;
		};

		const std::function<void(const std::string &)> skipAfter = [&](const std::string &endMarker)
		{
			while (getline(in, commandContent2))
			{
				trimSpaces(commandContent2);
				if (commandContent2 == endMarker)
					break;
			}
		};

		std::function<ParsedCommandList(std::string)> parseCommand = [&](std::string commandContent) -> ParsedCommandList
		{
			ParsedCommandList result;
			std::string commandType = getCommandType(commandContent);
			if (commandType.empty())
				return result;

			cleaveCommandType(commandContent);
			normalizeCommandType(commandType);

			if (nagaCommandsMap.contains(commandType))
			{
				result.emplace_back(nagaCommandsMap[commandType]->IsOnKeyPressed(), *(new MacroEvent(*nagaCommandsMap[commandType], nagaCommandsMap[commandType]->generateCommand(commandContent))));
			}
			else if (commandType == "key")
			{
				result.emplace_back(true, *(new MacroEvent(*nagaCommandsMap["keypressonpress"], nagaCommandsMap["keypressonpress"]->generateCommand(commandContent))));
				result.emplace_back(false, *(new MacroEvent(*nagaCommandsMap["keyreleaseonrelease"], nagaCommandsMap["keyreleaseonrelease"]->generateCommand(commandContent))));
			}
			else if (commandType == "specialkey")
			{
				unordered_map<std::string, nagaCommandClass *const>::iterator specialPress = nagaCommandsMap.find("specialpressonpress");
				unordered_map<std::string, nagaCommandClass *const>::iterator specialRelease = nagaCommandsMap.find("specialreleaseonrelease");
				if (specialPress != nagaCommandsMap.end() && specialRelease != nagaCommandsMap.end())
				{
					result.emplace_back(true, *(new MacroEvent(*specialPress->second, *(new std::string(commandContent)))));
					result.emplace_back(false, *(new MacroEvent(*specialRelease->second, *(new std::string(commandContent)))));
				}
			}
			else if (commandType == "loop" || commandType == "loop2")
			{
				std::string loopName = commandContent;
				std::string actualArgument = "start";
				const std::string::size_type loopArgPos = commandContent.find('=');
				bool shouldAddStop = false;
				bool isOnPress = true;

				if (loopArgPos != std::string::npos)
				{
					loopName = commandContent.substr(0, loopArgPos);
					nukeWhitespaces(loopName);
					std::string pressArgument = commandContent.substr(loopArgPos + 1);
					normalizeCommandType(pressArgument);

					if (pressArgument == "startonrelease")
					{
						isOnPress = false;
						actualArgument = "start";
					}
					else if (pressArgument == "stoponrelease")
					{
						isOnPress = false;
						actualArgument = "stop";
					}
					else if (pressArgument == "start" || pressArgument == "stop")
					{
						actualArgument = pressArgument;
					}
					else if (pressArgument == "toggle")
					{
						isOnPress = true;
						actualArgument = pressArgument;
					}
					else if (pressArgument == "togglerelease")
					{
						isOnPress = false;
						actualArgument = pressArgument;
					}
					else
					{
						actualArgument = pressArgument;
						try
						{
							if (std::stoll(pressArgument) > 0)
								shouldAddStop = true;
						}
						catch (...)
						{
							shouldAddStop = true;
						}
					}
				}
				else
				{
					nukeWhitespaces(loopName);
					shouldAddStop = true;
				}

				unordered_map<std::string, loop *>::iterator loopIt = loopsMap.find(loopName);
				if (loopIt == loopsMap.end())
				{
					clog << "\033[38;5;208mDiscarding loop binding, undefined loop: " << loopName << "\033[0m\n";
					return result;
				}

				const loop &loopRef = *loopIt->second;
				std::function<IMacroEvent *(const std::string &)> makeLoopEvent;
				if (commandType == "loop2")
					makeLoopEvent = [&](const std::string &arg)
					{ return new ThreadedLoopMacroEvent(loopRef, *(new std::string(arg))); };
				else
					makeLoopEvent = [&](const std::string &arg)
					{ return new loopMacroEvent(loopRef, *(new std::string(arg))); };
				result.emplace_back(isOnPress, *makeLoopEvent(actualArgument));
				if (shouldAddStop)
					result.emplace_back(false, *makeLoopEvent("stop"), true);
			}
			else if (commandType == "function" || commandType == "functionrelease")
			{
				nukeWhitespaces(commandContent);
				unordered_map<std::string, nagaFunction *>::iterator functionIt = functionsMap.find(commandContent);
				if (functionIt == functionsMap.end())
				{
					clog << "\033[38;5;208mDiscarding function binding, undefined function: " << commandContent << "\033[0m\n";
					return result;
				}
				bool isOnKeyPressed = commandType == "function";
				for (IMacroEvent *const funcEvent : functionIt->second->eventList)
					result.emplace_back(isOnKeyPressed, *funcEvent);
			}
			else
			{
				clog << "\033[38;5;208mDiscarding : " << commandType << "=" << commandContent << "\033[0m\n";
			}

			return result;
		};

		while (getline(in, commandContent))
		{
			if (shouldIgnoreLine(commandContent))
				continue;

			if (isIteratingConfig)
			{
				if (commandContent.substr(0, 9) == "configEnd")
				{
					isIteratingConfig = false;
				}
				else
				{
					const std::function<void(const std::string &)> processConfigLine = [&](const std::string &configLine)
					{
						int buttonNumberInt = getButtonNumber(configLine);
						if (buttonNumberInt == -1)
							return;

						std::string modifiedConfigLine = configLine;
						cleaveButtonNumber(modifiedConfigLine);

						ParsedCommandList commands = parseCommand(modifiedConfigLine);
						for (const ParsedCommand &command : commands)
							(*iteratedConfig)[buttonNumberInt][command.isOnKeyPressed].emplace_back(&command.macroEvent);
					};
					if (commandContent.substr(0, 8) == "context=")
					{
						cleaveCommandType(commandContent);
						nukeWhitespaces(commandContent);
						for (const string &contextItem : contextMap[commandContent])
							processConfigLine(contextItem);
					}
					else
					{
						processConfigLine(commandContent);
					}
				}
			}
			else if (commandContent.substr(0, 9) == "function=")
			{
				cleaveCommandType(commandContent);
				nukeWhitespaces(commandContent);
				if (functionsMap.contains(commandContent))
				{
					clog << "\033[38;5;208mSkipping duplicate function named : " << commandContent << "\033[0m\n";
					skipAfter("functionEnd");
					continue;
				}
				nagaFunction *newFunction = new nagaFunction();
				functionsMap.emplace(string(commandContent), newFunction);
				isIteratingFunction = true;
				while (isIteratingFunction && getline(in, commandContent2))
				{
					if (shouldIgnoreLine(commandContent2))
						continue;
					if (commandContent2.substr(0, 11) == "functionEnd")
					{
						isIteratingFunction = false;
					}
					else
					{
						ParsedCommandList commands = parseCommand(commandContent2);
						if (!getOnReleaseCommands(commands).empty())
						{
							clog << "\033[38;5;208mDiscarding in function (contains onKeyReleased): " << commandContent2 << "\033[0m\n";
							continue;
						}
						for (const ParsedCommand &command : commands)
							newFunction->addEvent(&command.macroEvent);
					}
				}
			}
			else if (commandContent.substr(0, 5) == "loop=")
			{
				cleaveCommandType(commandContent);
				nukeWhitespaces(commandContent);
				if (loopsMap.contains(commandContent))
				{
					clog << "\033[38;5;208mSkipping duplicate loop named : " << commandContent << "\033[0m\n";
					skipAfter("loopEnd");
					continue;
				}
				loop *newLoop = new loop();
				loopsMap.emplace(string(commandContent), newLoop);
				isIteratingLoop = true;
				while (isIteratingLoop && getline(in, commandContent2))
				{
					if (shouldIgnoreLine(commandContent2))
						continue;
					if (commandContent2.substr(0, 7) == "loopEnd")
					{
						isIteratingLoop = false;
					}
					else
					{
						ParsedCommandList commands = parseCommand(commandContent2);
						ParsedCommandPointerList onReleaseCommands = getOnReleaseCommands(commands);
						bool shouldDiscardLine = false;
						for (const ParsedCommand *const rcommand : onReleaseCommands)
							if (!rcommand->allowedOnReleaseAtLoopExit)
							{
								clog << "\033[38;5;208mDiscarding in loop (contains onKeyReleased): " << commandContent2 << "\033[0m\n";
								shouldDiscardLine = true;
								break;
							}

						if (!shouldDiscardLine)
							for (const ParsedCommand &command : commands)
							{
								if (command.isOnKeyPressed)
									newLoop->addEvent(&command.macroEvent);
								else if (command.allowedOnReleaseAtLoopExit)
									newLoop->addExitEvent(&command.macroEvent);
							}
					}
				}
			}
			else if (commandContent.substr(0, 8) == "context=")
			{
				cleaveCommandType(commandContent);
				nukeWhitespaces(commandContent);
				if (contextMap.contains(commandContent))
				{
					clog << "\033[38;5;208mSkipping duplicate context named : " << commandContent << "\033[0m\n";
					skipAfter("contextEnd");
					continue;
				}
				vector<string> newContext = vector<string>();
				contextMap.emplace(string(commandContent), newContext);
				isIteratingContext = true;
				while (isIteratingContext && getline(in, commandContent2))
				{
					if (shouldIgnoreLine(commandContent2))
						continue;
					if (commandContent2.substr(0, 10) == "contextEnd")
					{
						isIteratingContext = false;
					}
					else if (commandContent2.substr(0, 8) == "context=")
					{
						std::string &nestedContextName = commandContent2;
						cleaveCommandType(nestedContextName);
						nukeWhitespaces(nestedContextName);
						for (const std::string &contextItem : contextMap[nestedContextName])
							contextMap[commandContent].emplace_back(contextItem);
					}
					else
					{
						contextMap[commandContent].emplace_back(commandContent2);
					}
				}
			}
			else if ((isWindowConfig = (commandContent.substr(0, 13) == "configWindow=")) || commandContent.substr(0, 7) == "config=")
			{
				cleaveCommandType(commandContent);
				trimSpaces(commandContent);
				if (IMacroEventKeyMaps.contains(commandContent))
				{
					clog << "\033[38;5;208mSkipping duplicate profile named : " << commandContent << "\033[0m\n";
					skipAfter("configEnd");
					continue;
				}
				isIteratingConfig = true;
				iteratedConfig = &IMacroEventKeyMaps[commandContent];
				if (isWindowConfig)
					(*configSwitcher::configWindowAndLockMap)[commandContent] = new WindowConfigLock{false, new string("")};
				configSwitcher::notifySendMap.emplace(commandContent, (new string("notify-send -a Naga -t 300 \"Profile : " + commandContent + "\""))->c_str());
			}
		}
		in.close();
	}

	static void runActions(const std::vector<IMacroEvent *> &relativeMacroEvents)
	{
		for (IMacroEvent *const macroEvent : relativeMacroEvents)
			macroEvent->runInternal();
	}

	static void sideBtnThreadFunc()
	{
		ssize_t bytesRead;
		size_t eventCount;
		bool checkedForWindowConfig = false;
		while (true)
		{
			configSwitcher::remapRoutine();
			bytesRead = read(side_btn_fd, side_ev, side_ev_size);
			if (bytesRead == -1)
			{
				std::cerr << "\033[31mError reading from side button device.\n\033[0m";
				std::exit(2);
			}
			eventCount = bytesRead / input_event_size;
			for (size_t i = 0; i < eventCount; ++i)
			{
				const input_event &event = side_ev[i];
				if (event.type != EV_KEY || event.code < 2 || event.code > 13 || (event.value != 0 && event.value != 1))
					continue;

				if (!checkedForWindowConfig)
				{
					configSwitcher::checkForWindowConfig();
					checkedForWindowConfig = true;
				}
				thread(runActions, std::ref((*configSwitcher::currentConfigPtr)[event.code][event.value == 1])).detach();
			}
			checkedForWindowConfig = false;
		}
	}

	static void extraBtnThreadFunc()
	{
		ssize_t bytesRead;
		size_t eventCount;
		bool checkedForWindowConfig = false;
		while (true)
		{
			configSwitcher::remapRoutine();
			bytesRead = read(extra_btn_fd, extra_ev, extra_ev_size);
			if (bytesRead == -1)
			{
				std::cerr << "\033[31mError reading from extra button device.\n\033[0m";
				std::exit(2);
			}
			eventCount = bytesRead / input_event_size;
			for (size_t i = 0; i < eventCount; ++i)
			{
				const input_event &event = extra_ev[i];
				if (event.type == EV_KEY && (event.code == 275 || event.code == 276))
				{
					if (!checkedForWindowConfig)
					{
						configSwitcher::checkForWindowConfig();
						checkedForWindowConfig = true;
					}
					thread(runActions, std::ref((*configSwitcher::currentConfigPtr)[event.code - 261][event.value == 1])).detach();
					continue;
				}
				if (extraDeviceGrabbed && extraForwarder)
					extraForwarder->forward(event);
			}
			checkedForWindowConfig = false;
		}
	}

	static void run()
	{
		if (areSideBtnEnabled)
			std::thread(sideBtnThreadFunc).detach();
		if (areExtraBtnEnabled)
			std::thread(extraBtnThreadFunc).detach();

		static std::mutex mtx;
		static std::condition_variable cv;
		std::unique_lock<std::mutex> lock(mtx);
		cv.wait(lock, []
				{ return false; });
	}

	static void initDevices()
	{
		bool isThereADevice = false;
		for (const pair<const char *, const char *> &device : devices)
		{
			side_btn_fd = open(device.first, O_RDONLY);
			extra_btn_fd = open(device.second, O_RDONLY);

			if (side_btn_fd != -1 || extra_btn_fd != -1)
			{
				if (side_btn_fd == -1)
				{
					clog << "Reading from: \033[32m" << device.second << "\033[0m\n";
					areSideBtnEnabled = false;
				}
				else if (extra_btn_fd == -1)
				{
					clog << "Reading from: \033[32m" << device.first << "\033[0m\n";
					areExtraBtnEnabled = false;
				}
				else
				{
					clog << "Reading from: \033[32m" << device.first << "\033[0m" << "\n and \033[32m" << device.second << "\033[0m\n";
				}
				isThereADevice = true;
				break;
			}
		}

		if (!isThereADevice)
		{
			cerr << "No naga devices found or you don't have permission to access them.\n";
			exit(1);
		}

		if (areSideBtnEnabled)
		{
			ioctl(side_btn_fd, EVIOCGRAB, 1);
			fcntl(side_btn_fd, F_SETFL, O_NONBLOCK);
			while (read(side_btn_fd, side_ev, side_ev_size) > 0)
			{
			}
			fcntl(side_btn_fd, F_SETFL, 0);
		}

		if (areExtraBtnEnabled)
		{
			extraForwarder.reset(new UInputForwarder());
			if (!extraForwarder->init(extra_btn_fd))
			{
				extraForwarder.reset();
				clog << "[naga] uinput forwarding disabled; continuing without exclusive extra buttons.\n";
			}
			else if (ioctl(extra_btn_fd, EVIOCGRAB, 1) == -1)
			{
				clog << "\033[31m[naga] failed to grab extra button device: " << strerror(errno) << "\033[0m\n";
				extraForwarder.reset();
			}
			else
			{
				extraDeviceGrabbed = true;
				clog << "[naga] extra buttons grabbed; pointer events forwarded via uinput.\n";
				fcntl(extra_btn_fd, F_SETFL, O_NONBLOCK);
				while (read(extra_btn_fd, extra_ev, extra_ev_size) > 0)
				{
				}
				fcntl(extra_btn_fd, F_SETFL, 0);
			}
		}
	}

	static void init(const string &mapConfig = "defaultConfig")
	{
		srand((unsigned int)time(NULL) ^ (unsigned int)getpid());
		initDevices();
		registerCoreCommands();
		initAndRegisterPlatformCommands();
		initConf();
		configSwitcher::scheduleReMap(mapConfig);
		configSwitcher::loadConf();
		run();
	}
}

static void stopD()
{
	clog << "Stopping possible naga daemon\n";
	std::ignore = system(("/usr/local/bin/Naga_Linux/nagaKillroot.sh " + to_string((int)getpid())).c_str());
};

static int nagaMain(const int argc, const char *const argv[])
{
	if (argc > 1)
	{
		if (strstr(argv[1], "serviceHelper"))
		{
			stopD();
			NagaDaemon::init(argc > 2 && argv[2][0] != '\0' ? argv[2] : "defaultConfig");
		}
		else if (strstr(argv[1], "start"))
		{
			clog << "Starting naga daemon as service, naga debug to see logs...\n";
			usleep(100000);
			std::ignore = system("sudo systemctl restart naga");
		}
		else if (strstr(argv[1], "debug"))
		{
			clog << "Starting naga debug, logs :\n";
			std::ignore = system((argc > 2 ? ("journalctl -o cat " + std::string(argv[2]) + " naga") : "journalctl -o cat -fu naga").c_str());
		}
		else if (strstr(argv[1], "kill"))
		{
			clog << "Killing naga daemon processes:\n";
			std::ignore = system(("sudo sh /usr/local/bin/Naga_Linux/nagaKillroot.sh " + to_string((int)getpid())).c_str());
		}
		else if (strstr(argv[1], "stop"))
		{
			clog << "Stopping possible naga daemon\n";
			std::ignore = system("sudo systemctl stop naga");
		}
		else if (strstr(argv[1], "disable") || strstr(argv[1], "stop"))
		{
			clog << "Disabling naga daemon\n";
			std::ignore = system("sudo systemctl disable naga");
		}
		else if (strstr(argv[1], "enable") || strstr(argv[1], "stop"))
		{
			clog << "Enabling naga daemon\n";
			std::ignore = system("sudo systemctl enable naga");
		}
		else if (strstr(argv[1], "repair") || strstr(argv[1], "tame") || strstr(argv[1], "fix"))
		{
			clog << "Fixing dead keypad syndrome... STUTTER!!\n";
			std::ignore = system("sudo bash -c \"sh /usr/local/bin/Naga_Linux/nagaKillroot.sh && modprobe -r usbhid && modprobe -r psmouse && modprobe usbhid && modprobe psmouse && sleep 1 && sudo systemctl start naga\"");
		}
		else if (strstr(argv[1], "edit"))
		{
			std::ignore = system(("sudo bash -c 'orig_sum=\"$(sudo md5sum " + conf_file + ")\"; " + (argc > 2 ? string(argv[2]) : "sudo nano -m") + " " + conf_file + "; [[ \"$(sudo md5sum " + conf_file + ")\" != \"$orig_sum\" ]] && sudo systemctl restart naga'").c_str());
		}
		else if (strstr(argv[1], "vendor"))
		{
			std::ignore = system("lsusb | sed -E 's/ID ([0-9a-fA-F]{4}):([0-9a-fA-F]{4})/ID \\x1b[38;5;208m\\1\\x1b[0m:\\2/g'");

			std::string vendorId;
			while (true)
			{
				std::cout << "Enter vendor ID (or empty for razer (1532)): ";
				std::getline(std::cin, vendorId);
				if (vendorId.empty())
				{
					vendorId = "1532";
					break;
				}
				std::transform(vendorId.begin(), vendorId.end(), vendorId.begin(),
							   [](unsigned char c)
							   { return std::tolower(c); });
				if (vendorId.rfind("0x", 0) == 0)
					vendorId = vendorId.substr(2);
				if (vendorId.size() == 4 &&
					std::all_of(vendorId.begin(), vendorId.end(),
								[](unsigned char c)
								{ return std::isxdigit(c); }))
					break;
				std::cout << "Invalid vendor ID (4 hex chars, e.g. 046d).\n";
			}

			std::ignore = system(("printf 'KERNEL==\"event[0-9]*\",SUBSYSTEM==\"input\", ATTRS{idVendor}==\"" +
								  vendorId +
								  "\", GROUP=\"razerInputGroup\", MODE=\"0660\"' | sudo tee /etc/udev/rules.d/80-naga.rules >/dev/null")
									 .c_str());

			clog << "Restarting naga daemon service... PLEASE run "
				 << "\033[38;5;208msudo udevadm control --reload-rules && sudo udevadm trigger\033[0m"
				 << " or restart.\n";

			usleep(100000);
			std::ignore = system("sudo systemctl restart naga");
		}
		else if (strstr(argv[1], "uninstall"))
		{
			string answer;
			clog << "Are you sure you want to uninstall ? y/n\n";
			cin >> answer;
			if (answer.size() != 1 || (answer[0] != 'y' && answer[0] != 'Y'))
			{
				clog << "Aborting\n";
			}
			else
			{
				std::ignore = system("/usr/local/bin/Naga_Linux/nagaUninstall.sh");
			}
		}
	}
	else
	{
		clog << "Possible arguments : \n  start          Starts the daemon in hidden mode. (stops it before)\n  stop           Stops the daemon.\n  edit           Lets you edit the config.\n  debug\t\t Shows log.\n";
		clog << "  fix            For dead keypad.\n";
		clog << "  uninstall      Uninstalls the daemon.\n";
	}
	return 0;
}
