// This is lostallmymoney's remake of RaulPPelaez's original tool.
// RaulPPelaez, et. al wrote the original file.  As long as you retain this notice you
// can do whatever you want with this stuff.

#define NAGA_CORE_HPP

#include "extraButtonCapture.hpp"
#include "nagaSettings.hpp"
#include "notifySendHelper.hpp"
#include "windowConfigExpr.hpp"

#include <iostream>
#include <array>
#include <condition_variable>
#include <deque>
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
#include <unordered_set>
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
	std::vector<shared_ptr<IMacroEvent>> eventList;
	std::vector<shared_ptr<IMacroEvent>> exitEventList;

public:
	void addEvent(const shared_ptr<IMacroEvent> &newEvent)
	{
		eventList.emplace_back(newEvent);
		++eventCount; // not atomic; safe since no concurrent adds
	}

	void addExitEvent(const shared_ptr<IMacroEvent> &newEvent)
	{
		exitEventList.emplace_back(newEvent);
	}

	void stop() const noexcept
	{
		for (const shared_ptr<IMacroEvent> &exitEvent : exitEventList)
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
		else if (taNagaLoopArgument == "toggle" || taNagaLoopArgument == "toggleonrelease")
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
// Actions sit in fixed slots: 2..13 side buttons (config 1..12), 14..15 mouse thumb buttons
// (config 13..14). Fixed slots keep dispatch a pure read, and two threads share one profile.
inline constexpr int FirstButtonSlot = 2;
inline constexpr int ButtonSlotCount = 16;
using ButtonActionSlots = std::array<vector<shared_ptr<IMacroEvent>>, 2>; // [0] on release, [1] on press
using IMacroEventKeyMap = std::array<ButtonActionSlots, ButtonSlotCount>;

/**
 * ParsedCommand - Represents a parsed command from configuration
 */
struct ParsedCommand
{
	bool isOnKeyPressed;
	shared_ptr<IMacroEvent> macroEvent;
	bool allowedOnReleaseAtLoopExit;

	ParsedCommand(const bool tisOnKeyPressed, shared_ptr<IMacroEvent> tmacroEvent, const bool tallowedOnReleaseInLoop = false)
		: isOnKeyPressed(tisOnKeyPressed), macroEvent(std::move(tmacroEvent)), allowedOnReleaseAtLoopExit(tallowedOnReleaseInLoop)
	{
	}
};

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
	vector<shared_ptr<IMacroEvent>> eventList;
	void addEvent(const shared_ptr<IMacroEvent> &newEvent)
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
	const string *profileName;
};

using WindowConfigMap = unordered_map<string, WindowConfigLock>;

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
	string lastWindowClassChecked;
	WindowConfigMap configWindowAndLockMap;
	unordered_map<string, WindowConfigMap::iterator> windowClassCache;
	// Keys owned by configWindowAndLockMap: element refs survive a rehash, iterators do not.
	vector<const string *> configWindowExprList;
	IMacroEventKeyMap *currentConfigPtr = nullptr;
	WindowConfigMap::iterator matchedWindowConfigPtr, scheduledUnlockWindowCfgPtr;
	unordered_map<string, shared_ptr<notifySendHelper::NotificationCommand>> notifySendMap, unlockNotifySendMap;

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
		{
			const auto notification = notifySendMap.find(*scheduledReMapName);
			if (notification == notifySendMap.end() || !notification->second || notification->second->empty())
			{
				std::cerr << "Warning : No prepared notification for profile : " << *scheduledReMapName << std::endl;
				return;
			}
			notifySendHelper::sendNotification(notification->second);
		}
	}

	static WindowConfigMap::iterator resolveWindowMatch(const string &windowClass)
	{
		std::pair<unordered_map<string, WindowConfigMap::iterator>::iterator, bool> cachedDecision =
			windowClassCache.try_emplace(windowClass);

		if (!cachedDecision.second)
			return cachedDecision.first->second;

		WindowConfigMap::iterator configWindow = configWindowAndLockMap.find(windowClass);
		if (configWindow == configWindowAndLockMap.end())
		{
			for (const string *exprMatch : configWindowExprList)
			{
				if (windowConfigExpr::matches(*exprMatch, windowClass))
				{
					configWindow = configWindowAndLockMap.find(*exprMatch);
					break;
				}
			}
		}

		return cachedDecision.first->second = configWindow;
	}

	static void applyWindowConfig(WindowConfigMap::iterator configWindow)
	{
		if (configWindow != configWindowAndLockMap.end())
		{
			WindowConfigLock &windowConfigLock = configWindow->second;
			matchedWindowConfigPtr = configWindow;
			if (!winConfigActive)
				bckConfName = currentConfigName;
			if (windowConfigLock.isLocked)
				scheduledReMapName = windowConfigLock.lockedConfigName;
			else
				scheduledReMapName = windowConfigLock.profileName;
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

	static void checkForWindowConfig()
	{
		const string currAppClass(getActiveWindowTitle());
		lock_guard<mutex> guard(configSwitcherMutex);
		if (currAppClass != lastWindowClassChecked || forceRecheck)
		{
			if (currAppClass != lastWindowClassChecked)
			{
				clog << "\033[35mInfo : WindowName : " << currAppClass << "\033[0m" << '\n';
				lastWindowClassChecked = currAppClass;
			}
			forceRecheck = false;

			applyWindowConfig(resolveWindowMatch(currAppClass));
		}
	}

	static void remapRoutine()
	{
		lock_guard<mutex> guard(configSwitcherMutex);
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
			WindowConfigLock &windowConfigLock = matchedWindowConfigPtr->second;
			windowConfigLock.isLocked = forceRecheck = notifyOnNextLoad = true;
			windowConfigLock.lockedConfigName = &reMapStr;
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
		shared_ptr<notifySendHelper::NotificationCommand> unlockNotification;
		{
			lock_guard<mutex> guard(configSwitcherMutex);
			scheduledUnlockWindowCfgPtr = configWindowAndLockMap.find(unlockStr);
			if (scheduledUnlockWindowCfgPtr != configWindowAndLockMap.end())
			{
				WindowConfigLock &windowConfigLock = scheduledUnlockWindowCfgPtr->second;
				if (windowConfigLock.isLocked)
				{
					const bool isActiveTree = winConfigActive && matchedWindowConfigPtr == scheduledUnlockWindowCfgPtr;
					windowConfigLock.isLocked = false;
					if (isActiveTree)
						shouldRecheck = forceRecheck = notifyOnNextLoad = true;
					else
						unlockNotification = unlockNotifySendMap[unlockStr];
				}
			}
		}
		if (unlockNotification && !unlockNotification->empty())
			notifySendHelper::sendNotification(unlockNotification);
		if (shouldRecheck)
			checkForWindowConfig();
	}
}

namespace NagaDaemon
{
	using ParsedCommandList = vector<ParsedCommand>;
	ParsedCommandList platformComboKeyParser(const std::string &commandType, const std::string &commandContent);

	static constexpr bool OnKeyPressed = true;
	static constexpr bool OnKeyReleased = false;
	static constexpr size_t BufferSize = 1024;
	static unordered_map<string, nagaCommandClass *const> nagaCommandsMap;
	static std::map<std::string, bool> multilineEnabledList;

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
	static const char *currentSideDevicePath, *currentExtraDevicePath;

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

	static void emplaceConfigKey(const string &nagaCommand, bool onKeyPressed, void (*functionPtr)(const string &), const string &prefix = "", const string &suffix = "")
	{
		nagaCommandsMap.emplace(nagaCommand, new nagaCommandClass(onKeyPressed, functionPtr, prefix, suffix));
	}

	static void emplaceMultilineConfigKey(const string &nagaCommand, bool onKeyPressed, void (*functionPtr)(const string &), const string &prefix = "", const string &suffix = "")
	{
		NagaDaemon::multilineEnabledList.emplace(nagaCommand, true);
		emplaceConfigKey(nagaCommand, onKeyPressed, functionPtr, prefix, suffix);
	}

	static void registerCoreCommands()
	{
		emplaceConfigKey("chmap", OnKeyPressed, chmapNow);
		emplaceConfigKey("chmaponrelease", OnKeyReleased, chmapNow);

		emplaceConfigKey("sleep", OnKeyPressed, sleepNow);
		emplaceConfigKey("sleeponrelease", OnKeyReleased, sleepNow);

		emplaceConfigKey("randomsleep", OnKeyPressed, randomSleepNow);
		emplaceConfigKey("randomsleeponrelease", OnKeyReleased, randomSleepNow);

		emplaceConfigKey("unlockchmap", OnKeyPressed, unlockChmap);
		emplaceConfigKey("unlockchmaponrelease", OnKeyReleased, unlockChmap);

		emplaceConfigKey("launch", OnKeyReleased, executeThreadNow, "gtk-launch ");
		emplaceConfigKey("launch2", OnKeyReleased, executeNow, "gtk-launch ");

		emplaceMultilineConfigKey("run", OnKeyPressed, executeThreadNow);
		emplaceMultilineConfigKey("run2", OnKeyPressed, executeNow);
		emplaceMultilineConfigKey("userrun", OnKeyPressed, executeThreadNow, "sudo -Siu $USER -- ");
		emplaceMultilineConfigKey("userrunonrelease", OnKeyReleased, executeThreadNow, "sudo -Siu $USER -- ");
		emplaceMultilineConfigKey("userrun2", OnKeyPressed, executeNow, "sudo -Siu $USER -- ");
		emplaceMultilineConfigKey("userrun2onrelease", OnKeyReleased, executeNow, "sudo -Siu $USER -- ");

		emplaceMultilineConfigKey("runonrelease", OnKeyReleased, executeThreadNow);
		emplaceMultilineConfigKey("runonrelease2", OnKeyReleased, executeNow);

		emplaceMultilineConfigKey("runandwrite", OnKeyPressed, runAndWriteThread);
		emplaceMultilineConfigKey("runandwrite2", OnKeyPressed, platformRunAndWrite);
		emplaceMultilineConfigKey("runandwriteonrelease", OnKeyReleased, runAndWriteThread);
		emplaceMultilineConfigKey("runandwriteonrelease2", OnKeyReleased, platformRunAndWrite);
	}

	template <typename SectionMap>
	static bool sectionNameAvailable(SectionMap &sectionMap, const std::string &name,
									 const std::string &sectionType)
	{
		if (name.empty())
		{
			clog << "\033[38;5;208mSkipping " << sectionType << " with no name\033[0m\n";
			return false;
		}

		if (!sectionMap.contains(name))
			return true;

		clog << "\033[38;5;208mSkipping duplicate " << sectionType
			 << " named : " << name << "\033[0m\n";

		return false;
	}

	// Registers a window match (exact class or wildcard expression) as a
	// condition pointing back to the currently open profile: all matches of
	// a section are part of the same entry in the config map.
	static void registerWindowMatch(const std::string &matchContent, bool matchIsExpression,
									const string *profileNamePtr)
	{
		if (!sectionNameAvailable(configSwitcher::configWindowAndLockMap,
								  matchContent, "window match"))
			return;

		std::pair<WindowConfigMap::iterator, bool> inserted =
			configSwitcher::configWindowAndLockMap.emplace(
				matchContent, WindowConfigLock{false, nullptr, profileNamePtr});

		if (matchIsExpression)
			configSwitcher::configWindowExprList.push_back(&inserted.first->first);
	}

	struct ConfigParseState
	{
		const std::vector<std::string_view> &configLines;
		int currentlyReadLine;
		int currentIndentLevel;
	};

	static bool parsePlatformCommands(ParsedCommandList &result, const std::string &commandType,
									  const std::string &commandContent)
	{
		ParsedCommandList specialCommands =
			NagaDaemon::platformComboKeyParser(commandType, commandContent);

		if (specialCommands.empty())
			return false;

		for (ParsedCommand &command : specialCommands)
			result.emplace_back(std::move(command));

		return true;
	}

	static ParsedCommandPointerList getOnReleaseCommands(const ParsedCommandList &commands)
	{
		ParsedCommandPointerList result;

		for (const ParsedCommand &command : commands)
		{
			if (!command.isOnKeyPressed)
				result.emplace_back(&command);
		}

		return result;
	}

	static ParsedCommandList parseCommand(ConfigParseState &state, std::string commandContent)
	{
		ParsedCommandList result;

		std::string commandType = nagaText::textBefore(commandContent, '=');

		if (commandType.empty())
			return result;

		commandContent = nagaText::textAfter(commandContent, '=');
		commandType = nagaText::normalizeCommandType(commandType);

		if (NagaDaemon::multilineEnabledList.contains(commandType) && commandContent.empty())
		{
			std::string multilineCommand;
			int lastValidLine = state.currentlyReadLine;
			int minimumIndentLevel = state.currentIndentLevel + 4;

			for (int lineIndex = state.currentlyReadLine + 1;
				 lineIndex < static_cast<int>(state.configLines.size());
				 ++lineIndex)
			{
				std::string currentLine(state.configLines[lineIndex]);

				if (nagaText::isBlankOrComment(currentLine))
					continue;

				if (nagaText::getIndentLevel(currentLine) < minimumIndentLevel)
					break;

				currentLine.erase(0, currentLine.find_first_not_of(" \t"));

				multilineCommand += currentLine;
				multilineCommand += "\n";

				lastValidLine = lineIndex;
			}

			state.currentlyReadLine = lastValidLine;

			std::string wrappedCommand =
				"sh -s <<'nagaDelimiter1'\n" +
				multilineCommand +
				"nagaDelimiter1\n";

			result.emplace_back(
				nagaCommandsMap[commandType]->IsOnKeyPressed(),
				make_shared<MacroEvent>(
					*nagaCommandsMap[commandType],
					nagaCommandsMap[commandType]->generateCommand(wrappedCommand)));
		}
		else if (nagaCommandsMap.contains(commandType))
		{
			result.emplace_back(
				nagaCommandsMap[commandType]->IsOnKeyPressed(), make_shared<MacroEvent>(
																	*nagaCommandsMap[commandType],
																	nagaCommandsMap[commandType]->generateCommand(commandContent)));
		}
		else if (parsePlatformCommands(result, commandType, commandContent))
		{
		}
		else if (commandType == "key")
		{
			result.emplace_back(true, make_shared<MacroEvent>(
										  *nagaCommandsMap["keypressonpress"],
										  nagaCommandsMap["keypressonpress"]->generateCommand(commandContent)));

			result.emplace_back(false, make_shared<MacroEvent>(
										   *nagaCommandsMap["keyreleaseonrelease"],
										   nagaCommandsMap["keyreleaseonrelease"]->generateCommand(commandContent)));
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
				loopName = nagaText::stripAllWhitespaces(loopName);

				std::string pressArgument = commandContent.substr(loopArgPos + 1);
				pressArgument = nagaText::normalizeCommandType(pressArgument);

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
				else if (pressArgument == "toggleonrelease")
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
				loopName = nagaText::stripAllWhitespaces(loopName);
				shouldAddStop = true;
			}

			unordered_map<std::string, loop *>::iterator loopIt = loopsMap.find(loopName);

			if (loopIt == loopsMap.end())
			{
				clog << "\033[38;5;208mDiscarding loop binding, undefined loop: "
					 << loopName << "\033[0m\n";

				return result;
			}

			const loop &loopRef = *loopIt->second;

			std::function<shared_ptr<IMacroEvent>(const std::string &)> makeLoopEvent;

			if (commandType == "loop2")
			{
				makeLoopEvent = [&](const std::string &arg)
				{
					return make_shared<ThreadedLoopMacroEvent>(loopRef, arg);
				};
			}
			else
			{
				makeLoopEvent = [&](const std::string &arg)
				{
					return make_shared<loopMacroEvent>(loopRef, arg);
				};
			}

			result.emplace_back(isOnPress, makeLoopEvent(actualArgument));

			if (shouldAddStop)
				result.emplace_back(false, makeLoopEvent("stop"), true);
		}
		else if (commandType == "function" || commandType == "functiononrelease")
		{
			commandContent = nagaText::stripAllWhitespaces(commandContent);

			unordered_map<std::string, nagaFunction *>::iterator functionIt =
				functionsMap.find(commandContent);

			if (functionIt == functionsMap.end())
			{
				clog << "\033[38;5;208mDiscarding function binding, undefined function: "
					 << commandContent << "\033[0m\n";

				return result;
			}

			bool isOnKeyPressed = commandType == "function";

			for (const shared_ptr<IMacroEvent> &funcEvent : functionIt->second->eventList)
				result.emplace_back(isOnKeyPressed, funcEvent);
		}
		else
		{
			clog << "\033[38;5;208mDiscarding : "
				 << commandType << "=" << commandContent << "\033[0m\n";
		}

		return result;
	}
	static void initConf()
	{
		string commandContent, commandContent2;
		IMacroEventKeyMap *iteratedConfig;
		const string *iteratedConfigNamePtr;

		nagaFunction *currentFunction = nullptr;
		loop *currentLoop = nullptr;
		std::string currentContextName;

		bool isIteratingConfig = false,
			 isIteratingLoop = false,
			 isIteratingFunction = false,
			 isIteratingContext = false,
			 sectionHasContent = false;

		nagaSettings::TextFile configFile;
		nagaSettings::readTextFile(conf_file, true, configFile);
		const std::vector<std::string_view> &configLines = configFile.lines;
		ConfigParseState parseState{configLines, 0, 0};

		constexpr string_view configPrefix = "config=";
		constexpr string_view contextPrefix = "context=";
		constexpr string_view configWindowPrefix = "configWindow=";
		constexpr string_view configWindowExprPrefix = "configWindowExpr=";

		for (; static_cast<std::size_t>(parseState.currentlyReadLine) < configLines.size(); ++parseState.currentlyReadLine)
		{
			const std::string_view &line = configLines[parseState.currentlyReadLine];

			commandContent = line;
			if (nagaText::isBlankOrComment(commandContent))
				continue;

			parseState.currentIndentLevel = nagaText::getIndentLevel(commandContent);
			commandContent.erase(0, commandContent.find_first_not_of(" \t"));

			if (isIteratingConfig)
			{
				if (parseState.currentIndentLevel == 0)
				{
					// Window match lines placed at the profile's own level, before
					// any key bindings, belong to the current profile (same entry).
					if (!sectionHasContent)
					{
						const bool matchIsExpression = commandContent.starts_with(configWindowExprPrefix);

						if (matchIsExpression || commandContent.starts_with(configWindowPrefix))
						{
							std::string matchContent = matchIsExpression
														   ? commandContent.substr(configWindowExprPrefix.size())
														   : commandContent.substr(configWindowPrefix.size());
							matchContent = nagaText::trimWhiteSpaces(matchContent);

							registerWindowMatch(matchContent, matchIsExpression, iteratedConfigNamePtr);
							continue;
						}
					}

					isIteratingConfig = false;
				}
				else
				{
					const std::function<void(const std::string &)> processConfigLine = [&](const std::string &configLine)
					{
						int buttonNumberInt = nagaText::getButtonNumber(configLine);

						if (buttonNumberInt == -1)
							return;

						if (buttonNumberInt < FirstButtonSlot || buttonNumberInt >= ButtonSlotCount)
						{
							clog << "\033[38;5;208mSkipping out of range button : " << buttonNumberInt - 1 << "\033[0m\n";
							return;
						}

						std::string modifiedConfigLine = configLine;
						modifiedConfigLine = nagaText::textAfter(modifiedConfigLine, '-');
						modifiedConfigLine = nagaText::trimWhiteSpaces(modifiedConfigLine);

						ParsedCommandList commands = parseCommand(parseState, modifiedConfigLine);

						for (const ParsedCommand &command : commands)
							(*iteratedConfig)[buttonNumberInt][command.isOnKeyPressed]
								.emplace_back(command.macroEvent);
					};

					if (commandContent.starts_with(contextPrefix))
					{
						commandContent = nagaText::textAfter(commandContent, '=');
						commandContent = nagaText::stripAllWhitespaces(commandContent);

						for (const string &contextItem : contextMap[commandContent])
							processConfigLine(contextItem);
					}
					else
					{
						processConfigLine(commandContent);
					}

					sectionHasContent = true;
					continue;
				}
			}

			if (isIteratingFunction)
			{
				if (parseState.currentIndentLevel == 0)
				{
					isIteratingFunction = false;
				}
				else
				{
					ParsedCommandList commands = parseCommand(parseState, commandContent);

					if (!getOnReleaseCommands(commands).empty())
					{
						clog << "\033[38;5;208mDiscarding in function (contains onKeyReleased): "
							 << commandContent << "\033[0m\n";

						continue;
					}

					for (const ParsedCommand &command : commands)
						currentFunction->addEvent(command.macroEvent);

					continue;
				}
			}

			if (isIteratingLoop)
			{
				if (parseState.currentIndentLevel == 0)
				{
					isIteratingLoop = false;
				}
				else
				{
					ParsedCommandList commands = parseCommand(parseState, commandContent);
					ParsedCommandPointerList onReleaseCommands = getOnReleaseCommands(commands);

					bool shouldDiscardLine = false;

					for (const ParsedCommand *const rcommand : onReleaseCommands)
					{
						if (!rcommand->allowedOnReleaseAtLoopExit)
						{
							clog << "\033[38;5;208mDiscarding in loop (contains onKeyReleased): "
								 << commandContent << "\033[0m\n";

							shouldDiscardLine = true;
							break;
						}
					}

					if (!shouldDiscardLine)
					{
						for (const ParsedCommand &command : commands)
						{
							if (command.isOnKeyPressed)
								currentLoop->addEvent(command.macroEvent);
							else if (command.allowedOnReleaseAtLoopExit)
								currentLoop->addExitEvent(command.macroEvent);
						}
					}

					continue;
				}
			}

			if (isIteratingContext)
			{
				if (parseState.currentIndentLevel == 0)
				{
					isIteratingContext = false;
				}
				else if (commandContent.substr(0, 8) == "context=")
				{
					std::string nestedContextName = commandContent;

					nestedContextName = nagaText::textAfter(nestedContextName, '=');
					nestedContextName = nagaText::stripAllWhitespaces(nestedContextName);

					for (const std::string &contextItem : contextMap[nestedContextName])
						contextMap[currentContextName].emplace_back(contextItem);

					continue;
				}
				else
				{
					contextMap[currentContextName].emplace_back(commandContent);
					continue;
				}
			}

			if (commandContent.substr(0, 9) == "function=")
			{
				commandContent = nagaText::textAfter(commandContent, '=');
				commandContent = nagaText::stripAllWhitespaces(commandContent);

				if (!sectionNameAvailable(functionsMap, commandContent, "function"))
					continue;

				currentFunction = new nagaFunction();
				functionsMap.emplace(string(commandContent), currentFunction);

				isIteratingFunction = true;
			}
			else if (commandContent.substr(0, 5) == "loop=")
			{
				commandContent = nagaText::textAfter(commandContent, '=');
				commandContent = nagaText::stripAllWhitespaces(commandContent);

				if (!sectionNameAvailable(loopsMap, commandContent, "loop"))
					continue;

				currentLoop = new loop();
				loopsMap.emplace(string(commandContent), currentLoop);

				isIteratingLoop = true;
			}
			else if (commandContent.starts_with(contextPrefix))
			{
				commandContent = nagaText::textAfter(commandContent, '=');
				commandContent = nagaText::stripAllWhitespaces(commandContent);

				if (!sectionNameAvailable(contextMap, commandContent, "context"))
					continue;

				vector<string> newContext = vector<string>();
				contextMap.emplace(string(commandContent), newContext);

				currentContextName = commandContent;
				isIteratingContext = true;
			}
			else
			{
				const bool isWindowConfig = commandContent.starts_with(configWindowPrefix);
				const bool isWindowExprConfig = commandContent.starts_with(configWindowExprPrefix);

				if (!isWindowConfig && !isWindowExprConfig && !commandContent.starts_with(configPrefix))
					continue;

				commandContent = nagaText::textAfter(commandContent, '=');
				commandContent = nagaText::trimWhiteSpaces(commandContent);

				if (!sectionNameAvailable(IMacroEventKeyMaps, commandContent, "profile"))
					continue;

				isIteratingConfig = true;
				unordered_map<string, IMacroEventKeyMap>::iterator profile =
					IMacroEventKeyMaps.emplace(commandContent, IMacroEventKeyMap{}).first;
				iteratedConfig = &profile->second;
				iteratedConfigNamePtr = &profile->first;
				sectionHasContent = false;

				if (isWindowConfig || isWindowExprConfig)
					registerWindowMatch(commandContent, isWindowExprConfig, iteratedConfigNamePtr);

				configSwitcher::notifySendMap.emplace(
					commandContent,
					notifySendHelper::prepare(nagaSettings::buildNotifyCommand(commandContent)));
				configSwitcher::unlockNotifySendMap.emplace(
					commandContent,
					notifySendHelper::prepare(nagaSettings::buildUnlockedNotifyCommand(commandContent)));
			}
		}
	}

	static void runActions(const std::vector<shared_ptr<IMacroEvent>> &relativeMacroEvents)
	{
		for (const shared_ptr<IMacroEvent> &macroEvent : relativeMacroEvents)
			macroEvent->runInternal();
	}

	// One parked worker per slot: a press costs a lock and a notify, and actions keep arrival order.
	// While a worker sits in a long action (a toggled loop), new events get their own thread so the stop reaches it.
	class ButtonWorker
	{
	private:
		std::mutex queueMutex;
		std::condition_variable queuePending;
		std::deque<const std::vector<shared_ptr<IMacroEvent>> *> queue;
		bool workerStarted = false, runningAction = false;

		void consume()
		{
			std::unique_lock<std::mutex> lock(queueMutex);
			while (true)
			{
				queuePending.wait(lock, [this] { return !queue.empty(); });
				const std::vector<shared_ptr<IMacroEvent>> *const actions = queue.front();
				queue.pop_front();
				runningAction = true;
				lock.unlock();
				runActions(*actions);
				lock.lock();
				runningAction = false;
			}
		}

	public:
		void submit(const std::vector<shared_ptr<IMacroEvent>> &actions)
		{
			std::unique_lock<std::mutex> lock(queueMutex);
			if (runningAction)
			{
				lock.unlock();
				thread(runActions, std::cref(actions)).detach();
				return;
			}

			queue.push_back(&actions);
			queuePending.notify_one();

			if (!workerStarted)
			{
				workerStarted = true;
				thread(&ButtonWorker::consume, this).detach();
			}
		}
	};

	static std::array<ButtonWorker, ButtonSlotCount> buttonWorkers;

	static bool reopenDevice(const char *devicePath, int &fd, struct input_event *ev, size_t ev_size, bool checkGrab)
	{
		if (devicePath == nullptr)
			return false;

		if (fd != -1)
		{
			if (checkGrab)
				ioctl(fd, EVIOCGRAB, 0);
			close(fd);
		}

		fd = open(devicePath, O_RDONLY);
		if (fd == -1)
			return false;

		if (ioctl(fd, EVIOCGRAB, 1) == -1 && checkGrab)
		{
			close(fd);
			fd = -1;
			return false;
		}

		fcntl(fd, F_SETFL, O_NONBLOCK);
		while (read(fd, ev, ev_size) > 0)
		{
		}
		fcntl(fd, F_SETFL, 0);
		return true;
	}

	// buttonThreadFunc parameters, all compile time: each instantiation inlines its own codes and forwarding.
	struct SideButtonDevice
	{
		// KEY_1..KEY_EQUAL; only clean presses and releases count.
		static constexpr int FirstCode = 2, LastCode = 13, SlotOffset = 0;
		static constexpr bool RequireCleanValue = true, ForwardUnmatched = false, CheckGrabOnReopen = false;
		static constexpr const char *ReadError = "\033[31mError reading from side button device. Retrying in 5 seconds...\n\033[0m";
		static constexpr const char *ReopenError = "\033[31mFailed to reopen side button device. Retrying...\n\033[0m";
		static constexpr size_t BufferBytes = side_ev_size;
		static input_event *Buffer() { return side_ev; }
		static int &Fd() { return side_btn_fd; }
		static const char *Path() { return currentSideDevicePath; }
	};

	struct ExtraButtonDevice
	{
		// 275/276 are the mouse's own thumb buttons, in the slots above them.
		static constexpr int FirstCode = 275, LastCode = 276, SlotOffset = -261;
		static constexpr bool RequireCleanValue = false, ForwardUnmatched = true, CheckGrabOnReopen = true;
		static constexpr const char *ReadError = "\033[31mError reading from extra button device. Retrying in 5 seconds...\n\033[0m";
		static constexpr const char *ReopenError = "\033[31mFailed to reopen extra button device. Retrying...\n\033[0m";
		static constexpr size_t BufferBytes = extra_ev_size;
		static input_event *Buffer() { return extra_ev; }
		static int &Fd() { return extra_btn_fd; }
		static const char *Path() { return currentExtraDevicePath; }
	};

	template <typename Device>
	static void buttonThreadFunc()
	{
		bool checkedForWindowConfig = false;
		while (true)
		{
			configSwitcher::remapRoutine();
			const ssize_t bytesRead = read(Device::Fd(), Device::Buffer(), Device::BufferBytes);
			if (bytesRead == -1)
			{
				std::cerr << Device::ReadError;
				sleep(5);
				if (!reopenDevice(Device::Path(), Device::Fd(), Device::Buffer(), Device::BufferBytes, Device::CheckGrabOnReopen))
				{
					std::cerr << Device::ReopenError;
				}
				checkedForWindowConfig = false;
				continue;
			}

			const size_t eventCount = bytesRead / input_event_size;
			for (size_t i = 0; i < eventCount; ++i)
			{
				const input_event &event = Device::Buffer()[i];

				if (event.type != EV_KEY || event.code < Device::FirstCode || event.code > Device::LastCode ||
					(Device::RequireCleanValue && event.value != 0 && event.value != 1))
				{
					if constexpr (Device::ForwardUnmatched)
					{
						if (extraDeviceGrabbed && extraForwarder)
							extraForwarder->forward(event);
					}
					continue;
				}

				if (!checkedForWindowConfig)
				{
					configSwitcher::checkForWindowConfig();
					checkedForWindowConfig = true;
				}

				const int slot = event.code + Device::SlotOffset;
				const std::vector<shared_ptr<IMacroEvent>> &actions =
					(*configSwitcher::currentConfigPtr)[slot][event.value == 1];
				if (!actions.empty())
					buttonWorkers[slot].submit(actions);
			}
			checkedForWindowConfig = false;
		}
	}

	static void run()
	{
		if (areSideBtnEnabled)
			std::thread(buttonThreadFunc<SideButtonDevice>).detach();
		if (areExtraBtnEnabled)
			std::thread(buttonThreadFunc<ExtraButtonDevice>).detach();

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
				currentSideDevicePath = device.first;
				currentExtraDevicePath = device.second;
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
	static const std::function<void()> displayHelp = []()
	{
		clog << "Possible arguments : \n"
			 << "  start          Starts the daemon in hidden mode. (stops it before)\n"
			 << "  stop           Stops the daemon.\n"
			 << "  enable         Enables the daemon.\n"
			 << "  disable        Disables the daemon.\n"
			 << "  edit           Lets you edit the config.\n"
			 << "  settings       Lets you edit naga settings.\n"
			 << "  debug          Shows logs.\n"
			 << "  kill           Kills daemon processes.\n"
			 << "  fix            Fixes dead keypad / USB input state.\n"
			 << "  vendor         Configure vendor ID / udev rules.\n"
			 << "  uninstall      Uninstalls the daemon.\n"
			 << "  serviceHelper  Internal service bootstrap.\n";
	};

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
			return nagaSettings::editFile(argc, argv, conf_file);
		}
		else if (strstr(argv[1], "settings"))
		{
			return nagaSettings::editFile(argc, argv, nagaSettings::settingsPath());
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
		else
			displayHelp();
	}
	else
		displayHelp();
	return 0;
}
