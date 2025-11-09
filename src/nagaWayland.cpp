// This is lostallmymoney's remake of RaulPPelaez's original tool.
// RaulPPelaez, et. al wrote the original file.  As long as you retain this notice you
// can do whatever you want with this stuff.

#include <iostream>
#include <vector>
#include <fstream>
#include <fcntl.h>
#include <unistd.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <sys/ioctl.h>
#include <cerrno>
#include <thread>
#include <algorithm>
#include <mutex>
#include <condition_variable>
#include <map>
#include <memory>
#include <atomic>
#include <functional>
#include <string>
#include "extraButtonCapture.hpp"
#include "nagaDotoolLib.hpp"
#include "getactivewindowWayland.hpp"
// #include "nagaUsbUnbindRebind.hpp" //todo, when sudo-rs is updated to allow systemctl to use sudoers or if a workaround is found
using namespace std;
using nagaDotool::closeNagaDotoolPipe;
using nagaDotool::initNagaDotoolPipe;
using nagaDotool::writeNagaDotoolCommand;
using getactivewindowWayland::getTitle;

static mutex configSwitcherMutex;
static const string conf_file = string(getenv("HOME")) + "/.naga/keyMapWayland.txt";

class IMacroEvent
{
public:
	virtual ~IMacroEvent() = default;
	virtual void runInternal() const = 0;
};

class nagaCommandClass
{
private:
	const string prefix, suffix;
	const bool onKeyPressed;
	void (*const internalFunction)(const string &c);

public:
	bool IsOnKeyPressed() const { return onKeyPressed; }
	void run(const string &content) const { internalFunction(content); }

	std::string generateCommand(const std::string &commandContent) const {
		return prefix + commandContent + suffix;
	}
	
	nagaCommandClass(const bool tonKeyPressed, void (*const tinternalF)(const string &cc), const string &tprefix = "", const string &tsuffix = "") : prefix(tprefix), suffix(tsuffix), onKeyPressed(tonKeyPressed), internalFunction(tinternalF)
	{
	}
};

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

class loop
{
private:
	mutable std::atomic<uint32_t> generation{0};
	mutable std::atomic<bool> globalStop{false};
	size_t eventCount{0};
	std::vector<IMacroEvent *> eventList;

public:
	void addEvent(IMacroEvent *const newEvent)
	{
		eventList.emplace_back(newEvent);
		++eventCount;
	}

	void stop() const noexcept
	{
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
					clog << "Invalid loop argument (zero): " << taNagaLoopArgument << '\n';
				}
			}
			catch (...)
			{
				clog << "Invalid loop argument: " << taNagaLoopArgument << '\n';
			}
		}
	}

	void runInternal() const override
	{
		loopAction();
	}
};

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

using IMacroEventKeyMap = map<int, map<bool, vector<IMacroEvent *>>>;
struct ParsedCommand
{
	bool isOnKeyPressed;
	IMacroEvent &macroEvent;
};

using ParsedCommandList = vector<ParsedCommand>;

static map<string, IMacroEventKeyMap> IMacroEventKeyMaps;
static map<string, loop *> loopsMap;
static map<string, vector<string>> contextMap;

class nagaFunction
{
public:
	vector<IMacroEvent *> eventList;
	void addEvent(IMacroEvent *const newEvent)
	{
		eventList.emplace_back(newEvent);
	}
};

static map<string, nagaFunction *> functionsMap;

struct WindowConfigLock
{
	bool isLocked;
	const string *lockedConfigName;
};

namespace configSwitcher
{
	using WindowConfigMap = map<const string, WindowConfigLock *>;

	static bool scheduledReMap = false, winConfigActive = false, scheduledUnlock = false, forceRecheck = false, notifyOnNextLoad = false;
	static const string *currentConfigName = nullptr, *scheduledReMapName = nullptr, *bckConfName = nullptr;
	static string lastLoggedWindow;
	static WindowConfigMap *configWindowAndLockMap = new WindowConfigMap();
	static IMacroEventKeyMap *currentConfigPtr;
	static WindowConfigMap::iterator currentWindowConfigPtr, scheduledUnlockWindowCfgPtr;
	static map<string, const char *> notifySendMap;

	static void loadConf(bool silent = false)
	{
		if (notifyOnNextLoad)
			silent = false;

		scheduledReMap = notifyOnNextLoad = false;
		if (!IMacroEventKeyMaps.contains(*scheduledReMapName))
		{
			clog << "Undefined profile : " << *scheduledReMapName << '\n';
			return;
		}
		currentConfigName = scheduledReMapName;
		currentConfigPtr = &IMacroEventKeyMaps[*scheduledReMapName];
		if (!silent)
			std::ignore = system(notifySendMap[*scheduledReMapName]);
	}

	static void checkForWindowConfig()
	{
		const string currAppClass(getTitle());
		if (currAppClass != lastLoggedWindow)
		{
			clog << "WindowNameLog : " << currAppClass << '\n';
			lastLoggedWindow.assign(currAppClass);
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
	static constexpr size_t BufferSize = 1024;
	static constexpr size_t DotoolCommandLimit = 65536;
	static map<string, nagaCommandClass *const> nagaCommandsMap;

	static constexpr size_t input_event_size = sizeof(input_event);
	static struct input_event side_ev[64];
	static struct input_event extra_ev[64];
	static const size_t side_ev_size = static_cast<size_t>(sizeof(side_ev));
	static const size_t extra_ev_size = static_cast<size_t>(sizeof(extra_ev));
	static vector<pair<const char *const, const char *const>> devices;
	static bool areSideBtnEnabled = true, areExtraBtnEnabled = true;

	static unique_ptr<UInputForwarder> extraForwarder;
	static bool extraDeviceGrabbed = false;

	static void initConf()
	{
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

		const std::function<void(std::string &)> normalizeCommandType = [&nukeWhitespaces](std::string &value)
		{
			nukeWhitespaces(value);
			std::transform(value.begin(), value.end(), value.begin(),
						   [](unsigned char c)
						   { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); });
		};

		const std::function<int(const std::string &)> getButtonNumber = [](const std::string &configLine) -> int
		{
			std::string::size_type equalPos = configLine.find('=');
			if (equalPos == std::string::npos)
				return -1;

			std::string beforeEqual = configLine.substr(0, equalPos);
			std::string::size_type dashPos = beforeEqual.find('-');
			if (dashPos == std::string::npos)
				return -1;

			try
			{
				return std::stoi(beforeEqual.substr(0, dashPos)) + 1;
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
				return "";

			std::string beforeEqual = configLine.substr(0, equalPos);
			std::string::size_type dashPos = beforeEqual.find('-');
			if (dashPos == std::string::npos)
				return beforeEqual; // No dash means button number already removed

			return beforeEqual.substr(dashPos + 1);
		};

		const std::function<void(std::string &)> cleaveButtonNumber = [](std::string &configLine)
		{
			std::string::size_type equalPos = configLine.find('=');
			if (equalPos == std::string::npos)
				return;

			std::string beforeEqual = configLine.substr(0, equalPos);
			std::string::size_type dashPos = beforeEqual.find('-');
			if (dashPos == std::string::npos)
				return;

			// Remove the button number and dash, keep everything after the dash
			std::string afterDash = beforeEqual.substr(dashPos + 1);
			std::string afterEqual = configLine.substr(equalPos);
			configLine = afterDash + afterEqual;
		};

		const std::function<void(std::string &)> cleaveCommandType = [](std::string &configLine)
		{
			std::string::size_type equalPos = configLine.find('=');
			if (equalPos == std::string::npos)
				return;

			// Remove everything before and including the '='
			configLine = configLine.substr(equalPos + 1);
		};

		const std::function<bool(const ParsedCommandList &)> hasAnyCommandOnKeyRelease = [](const ParsedCommandList &commands) -> bool
		{
			for (const ParsedCommand &command : commands)
			{
				if (!command.isOnKeyPressed)
					return true;
			}
			return false;
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
				commandContent = nagaCommandsMap[commandType]->generateCommand(commandContent);
				result.emplace_back(ParsedCommand{nagaCommandsMap[commandType]->IsOnKeyPressed(),
												  *(new MacroEvent(*nagaCommandsMap[commandType], *(new std::string(commandContent))))});
			}
			else if (commandType == "key")
			{
				std::string commandContent2 = nagaCommandsMap["keyreleaseonrelease"]->generateCommand(commandContent);
				commandContent = nagaCommandsMap["keypressonpress"]->generateCommand(commandContent);
				result.emplace_back(ParsedCommand{true, *(new MacroEvent(*nagaCommandsMap["keypressonpress"], *(new std::string(commandContent))))});
				result.emplace_back(ParsedCommand{false, *(new MacroEvent(*nagaCommandsMap["keyreleaseonrelease"], *(new std::string(commandContent2))))});
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
					normalizeCommandType(pressArgument); // Normalize loop arguments

					// Process argument and determine behavior
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

				std::map<std::string, loop *>::iterator loopIt = loopsMap.find(loopName);
				if (loopIt == loopsMap.end())
				{
					std::cerr << "Discarding loop binding, undefined loop: " << loopName << '\n';
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
				result.emplace_back(ParsedCommand{isOnPress, *makeLoopEvent(actualArgument)});
				if (shouldAddStop)
					result.emplace_back(ParsedCommand{false, *makeLoopEvent("stop")});
			}
			else if (commandType == "function" || commandType == "functionrelease")
			{
				nukeWhitespaces(commandContent);
				std::map<std::string, nagaFunction *>::iterator functionIt = functionsMap.find(commandContent);
				if (functionIt == functionsMap.end())
				{
					std::cerr << "Discarding function binding, undefined function: " << commandContent << '\n';
					return result;
				}
				bool isOnKeyPressed = commandType == "function";
				for (IMacroEvent *const funcEvent : functionIt->second->eventList)
				{
					result.emplace_back(ParsedCommand{isOnKeyPressed, *funcEvent});
				}
			}
			else
			{
				std::cerr << "Discarding : " << commandType << "=" << commandContent << '\n';
			}

			return result;
		};

		string commandContent, commandContent2;
		IMacroEventKeyMap *iteratedConfig;
		bool isIteratingConfig = false, isIteratingLoop = false, isIteratingFunction = false, isIteratingContext = false, isWindowConfig = false;

		ifstream in(conf_file.c_str(), ios::in);
		if (!in)
		{
			std::cerr << "Cannot open " << conf_file << ". Exiting.\n";
			std::exit(1);
		}

		while (getline(in, commandContent))
		{
			if (shouldIgnoreLine(commandContent))
				continue; // Ignore comments, empty lines

			if (isIteratingConfig)
			{
				if (commandContent.substr(0, 9) == "configEnd") // finding configEnd
				{
					isIteratingConfig = false;
				}
				else
				{
					const std::function<void(const std::string &)> processConfigLine = [&](const std::string &configLine)
					{
						// Extract button number and cleave it from the line
						int buttonNumberInt = getButtonNumber(configLine);
						if (buttonNumberInt == -1)
							return;

						std::string modifiedConfigLine = configLine;
						cleaveButtonNumber(modifiedConfigLine);

						ParsedCommandList commands = parseCommand(modifiedConfigLine);
						for (const ParsedCommand &command : commands)
						{
							(*iteratedConfig)[buttonNumberInt][command.isOnKeyPressed].emplace_back(&command.macroEvent);
						}
					};
					if (commandContent.substr(0, 8) == "context=")
					{
						cleaveCommandType(commandContent);
						nukeWhitespaces(commandContent);
						for (const string &contextItem : contextMap[commandContent])
						{
							processConfigLine(contextItem);
						}
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
				nagaFunction *newFunction = new nagaFunction();
				functionsMap.emplace(string(commandContent), newFunction);
				isIteratingFunction = true;
				while (isIteratingFunction && getline(in, commandContent2))
				{
					if (shouldIgnoreLine(commandContent2))
						continue;										// Ignore comments, empty lines
					if (commandContent2.substr(0, 11) == "functionEnd") // finding functionEnd
					{
						isIteratingFunction = false;
					}
					else
					{
						ParsedCommandList commands = parseCommand(commandContent2);

						// Check if any command is onKeyReleased - if so, discard the whole vector for functions
						if (hasAnyCommandOnKeyRelease(commands))
						{
							clog << "Discarding in function (contains onKeyReleased): " << commandContent2 << '\n';
							continue;
						} // Add all events from the vector to the function
						for (const ParsedCommand &command : commands)
						{
							newFunction->addEvent(&command.macroEvent);
						}
					}
				}
			}
			else if (commandContent.substr(0, 5) == "loop=")
			{
				cleaveCommandType(commandContent);
				nukeWhitespaces(commandContent);
				loop *newLoop = new loop();
				loopsMap.emplace(string(commandContent), newLoop);
				isIteratingLoop = true;
				while (isIteratingLoop && getline(in, commandContent2))
				{
					if (shouldIgnoreLine(commandContent2))
						continue;								   // Ignore comments, empty lines
					if (commandContent2.substr(0, 7) == "loopEnd") // finding loopEnd
					{
						isIteratingLoop = false;
					}
					else
					{
						ParsedCommandList commands = parseCommand(commandContent2);

						// Check if any command is onKeyReleased - if so, discard the whole vector for loops
						if (hasAnyCommandOnKeyRelease(commands))
						{
							clog << "Discarding in loop (contains onKeyReleased): " << commandContent2 << '\n';
							continue;
						} // Add all events from the vector to the loop
						for (const ParsedCommand &command : commands)
						{
							newLoop->addEvent(&command.macroEvent);
						}
					}
				}
			}
			else if (commandContent.substr(0, 8) == "context=")
			{
				cleaveCommandType(commandContent);
				nukeWhitespaces(commandContent);
				vector<string> newContext = vector<string>();
				contextMap.emplace(string(commandContent), newContext);
				isIteratingContext = true;
				while (isIteratingContext && getline(in, commandContent2))
				{
					if (shouldIgnoreLine(commandContent2))
						continue;									   // Ignore comments, empty lines
					if (commandContent2.substr(0, 10) == "contextEnd") // finding functionEnd
					{
						isIteratingContext = false;
					}
					else if (commandContent2.substr(0, 8) == "context=") // nested context
					{
						std::string &nestedContextName = commandContent2;
						cleaveCommandType(nestedContextName);
						nukeWhitespaces(nestedContextName);
						for (const std::string &contextItem : contextMap[nestedContextName])
						{
							contextMap[commandContent].emplace_back(contextItem);
						}
					}
					else
					{
						contextMap[commandContent].emplace_back(commandContent2);
					}
				}
			}
			else if ((isWindowConfig = (commandContent.substr(0, 13) == "configWindow=")) || commandContent.substr(0, 7) == "config=")
			{
				isIteratingConfig = true;
				// Save the original line for type check, but use cleaned name as key
				cleaveCommandType(commandContent);
				nukeWhitespaces(commandContent);
				iteratedConfig = &IMacroEventKeyMaps[commandContent];
				if (isWindowConfig)
					(*configSwitcher::configWindowAndLockMap)[commandContent] = new WindowConfigLock{false, new string("")};
				configSwitcher::notifySendMap.emplace(commandContent, (new string("notify-send -a Naga -t 300 \"Profile : " + commandContent + "\""))->c_str());
			}
		}
		in.close();
	}

	static int side_btn_fd, extra_btn_fd;

	static void runActions(const std::vector<IMacroEvent *> &relativeMacroEvents)
	{
		for (IMacroEvent *const macroEvent : relativeMacroEvents)
		{ // run all the events at Key
			macroEvent->runInternal();
		}
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
				std::cerr << "Error reading from side button device.\n";
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
				std::cerr << "Error reading from extra button device.\n";
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
				{
					extraForwarder->forward(event);
				}
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
		// Main thread blocks forever
		static std::mutex mtx;
		static std::condition_variable cv;
		std::unique_lock<std::mutex> lock(mtx);
		cv.wait(lock, []
				{ return false; });
	}

	// Functions that can be given to configKeys
	static void mouseClick(const string &macroContent)
	{
		writeNagaDotoolCommand("click " + macroContent);
	}

	static void chmapNow(const string &macroContent)
	{
		configSwitcher::scheduleReMap(macroContent); // schedule config switch/change
	}

	static void sleepNow(const string &macroContent)
	{
		usleep(static_cast<useconds_t>(stoul(macroContent) * 1000)); // microseconds make me dizzy in keymap.txt
	}

	static void randomSleepNow(const string &macroContent)
	{
		usleep(static_cast<useconds_t>((rand() % (stoul(macroContent) + 1)) * 1000));
	}

	static void nagaDotoolKeydown(const string &macroContent)
	{
		writeNagaDotoolCommand("keydown " + macroContent);
	}

	static void nagaDotoolKeyup(const string &macroContent)
	{
		writeNagaDotoolCommand("keyup " + macroContent);
	}

	static void nagaDotoolKey(const string &macroContent)
	{
		writeNagaDotoolCommand("key " + macroContent);
	}

	static void nagaDotoolType(const string &macroContent)
	{
		writeNagaDotoolCommand("type " + macroContent);
	}

	static void runAndWrite(const string &macroContent)
	{
		unique_ptr<FILE, int (*)(FILE *)> pipe(popen(macroContent.c_str(), "r"), &pclose);
		if (!pipe)
		{
			throw runtime_error("runAndWrite execution Failed !");
		}

		string currentLine;
		FILE *fp = pipe.get();
		size_t counter = 0;
		for (int ch = fgetc(fp); ch != EOF; ch = fgetc(fp))
		{
			if (ch == '\n')
			{
				if (!currentLine.empty())
				{
					writeNagaDotoolCommand("type " + currentLine);
					counter = 0;
					currentLine.clear();
				}
				writeNagaDotoolCommand("key enter");
				writeNagaDotoolCommand("key home");
			}
			else
			{
				currentLine.push_back(static_cast<char>(ch));
				counter++;
				if (counter >= DotoolCommandLimit)
				{
					writeNagaDotoolCommand("type " + currentLine);
					counter = 0;
					currentLine.clear();
				}
			}
		}

		if (!currentLine.empty())
		{
			writeNagaDotoolCommand("type " + currentLine);
		}
	}

	static void runAndWriteThread(const string &macroContent)
	{
		thread(runAndWrite, std::ref(macroContent)).detach();
	}

	static void executeNow(const string &macroContent)
	{
		std::ignore = system(macroContent.c_str());
	}
	static void executeThreadNow(const string &macroContent)
	{
		thread(executeNow, std::ref(macroContent)).detach();
	}

	static void unlockChmap(const string &macroContent)
	{
		configSwitcher::scheduleUnlockChmap(macroContent);
	}
	// end of configKeys functions

	static void emplaceConfigKey(const string &nagaCommand, bool onKeyPressed, void (*functionPtr)(const string &), const string &prefix = "", const string &suffix = "")
	{
		nagaCommandsMap.emplace(nagaCommand, new nagaCommandClass(onKeyPressed, functionPtr, prefix, suffix));
	}

	static void init(const string &mapConfig = "defaultConfig")
	{
		// Seed RNG for randomSleep
		srand((unsigned int)time(NULL) ^ (unsigned int)getpid());
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
		devices.emplace_back("/dev/input/by-id/usb-Razer_Razer_Naga_V2_HyperSpeed_000000000000-if02-event-kbd", "/dev/input/by-id/usb-Razer_Razer_Naga_V2_HyperSpeed_000000000000-event-mouse");	// Naga Hyperspeed USB MODE (add bluetooth files above this one)
		
		// devices.emplace_back("/dev/input/by-id/YOUR_DEVICE_FILE", "/dev/input/by-id/YOUR_DEVICE_FILE#2");		 // DUMMY EXAMPLE, ONE CAN BE EMPTY LIKE SUCH : ""  (for devices with no extra buttons)

		bool isThereADevice = false;
		const char *sideDevice;
		for (pair<const char *const, const char *const> &device : devices)
		{ // Setup check
			side_btn_fd = open(device.first, O_RDONLY);
			extra_btn_fd = open(device.second, O_RDONLY);

			if (side_btn_fd != -1 || extra_btn_fd != -1)
			{
				if (side_btn_fd == -1)
				{
					clog << "Reading from: " << device.second << '\n';
					areSideBtnEnabled = false;
				}
				else if (extra_btn_fd == -1)
				{
					clog << "Reading from: " << device.first << '\n';
					areExtraBtnEnabled = false;
					sideDevice = device.first;
				}
				else
				{
					clog << "Reading from: " << device.first << "\n and " << device.second << '\n';
					sideDevice = device.first;
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
			// Flush any pending events to prevent stuck keys/buttons (not enough)
			fcntl(side_btn_fd, F_SETFL, O_NONBLOCK);
			while (read(side_btn_fd, side_ev, side_ev_size) > 0)
			{
			}
			fcntl(side_btn_fd, F_SETFL, 0);
			// usb_unbind_rebind(sideDevice); // rebind to avoid stuck keys/mouse issues on startup, todo when sudo-rs is updated or workaround found to run as root usb_rebind_helper.sh
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
				clog << "[naga] failed to grab extra button device: " << strerror(errno) << '\n';
				extraForwarder.reset();
			}
			else
			{
				extraDeviceGrabbed = true;
				clog << "[naga] extra buttons grabbed; pointer events forwarded via uinput.\n";
				// Flush any pending events to prevent stuck keys/buttons (not enough)
				fcntl(extra_btn_fd, F_SETFL, O_NONBLOCK);
				while (read(extra_btn_fd, extra_ev, extra_ev_size) > 0)
				{
				}
				fcntl(extra_btn_fd, F_SETFL, 0);
			}
		}

		// modulable options list to manage internals inside runActions method arg1:COMMAND, arg2:onKeyPressed?, arg3:function to send prefix+config content.
		// options must be lowercase here don't get caught
#define ONKEYPRESSED true
#define ONKEYRELEASED false

		emplaceConfigKey("click", ONKEYPRESSED, mouseClick);
		emplaceConfigKey("clickrelease", ONKEYRELEASED, mouseClick);

		emplaceConfigKey("chmap", ONKEYPRESSED, chmapNow);
		emplaceConfigKey("chmaprelease", ONKEYRELEASED, chmapNow);

		emplaceConfigKey("sleep", ONKEYPRESSED, sleepNow);
		emplaceConfigKey("sleeprelease", ONKEYRELEASED, sleepNow);

		emplaceConfigKey("randomsleep", ONKEYPRESSED, randomSleepNow);
		emplaceConfigKey("randomsleeprelease", ONKEYRELEASED, randomSleepNow);

		emplaceConfigKey("run", ONKEYPRESSED, executeThreadNow);
		emplaceConfigKey("run2", ONKEYPRESSED, executeNow);

		emplaceConfigKey("runrelease", ONKEYRELEASED, executeThreadNow);
		emplaceConfigKey("runrelease2", ONKEYRELEASED, executeNow);

		emplaceConfigKey("runandwrite", ONKEYPRESSED, runAndWriteThread);
		emplaceConfigKey("runandwrite2", ONKEYPRESSED, runAndWrite);

		emplaceConfigKey("runandwriterelease", ONKEYRELEASED, runAndWriteThread);
		emplaceConfigKey("runandwriterelease2", ONKEYRELEASED, runAndWrite);

		emplaceConfigKey("unlockchmap", ONKEYPRESSED, unlockChmap);
		emplaceConfigKey("unlockchmaprelease", ONKEYRELEASED, unlockChmap);

		emplaceConfigKey("launch", ONKEYRELEASED, executeThreadNow, "gtk-launch ");
		emplaceConfigKey("launch2", ONKEYRELEASED, executeNow, "gtk-launch ");

		emplaceConfigKey("keypressonpress", ONKEYPRESSED, nagaDotoolKeydown);
		emplaceConfigKey("keypressonrelease", ONKEYRELEASED, nagaDotoolKeydown);

		emplaceConfigKey("keyreleaseonpress", ONKEYPRESSED, nagaDotoolKeyup);
		emplaceConfigKey("keyreleaseonrelease", ONKEYRELEASED, nagaDotoolKeyup);

		emplaceConfigKey("keyclick", ONKEYPRESSED, nagaDotoolKey);
		emplaceConfigKey("keyclickrelease", ONKEYRELEASED, nagaDotoolKey);

		emplaceConfigKey("string", ONKEYPRESSED, nagaDotoolType);
		emplaceConfigKey("stringrelease", ONKEYRELEASED, nagaDotoolType);

		initConf();

		configSwitcher::scheduleReMap(mapConfig);
		configSwitcher::loadConf(); // Initialize config

		run();
	}
}

void stopD()
{
	clog << "Stopping possible naga daemon\n";
	std::ignore = system(("/usr/local/bin/Naga_Linux/nagaKillroot.sh " + to_string((int)getpid())).c_str());
};

// arguments manage
int main(const int argc, const char *const argv[])
{
	if (argc > 1)
	{
		if (strstr(argv[1], "serviceHelper"))
		{
			stopD();
			initNagaDotoolPipe();
			NagaDaemon::init(argc > 2 && argv[2][0] != '\0' ? argv[2] : "defaultConfig"); // lets you configure a default profile in /etc/systemd/system/naga.service
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
			std::ignore = system((argc > 2 ? ("journalctl " + string(argv[2]) + " naga") : "journalctl -fu naga").c_str());
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
		clog << "Possible arguments : \n  start          Starts the daemon in hidden mode. (stops it before)\n  stop           Stops the daemon.\n  edit           Lets you edit the config.\n  debug\t\t Shows log.\n  fix            For dead keypad.\n  uninstall      Uninstalls the daemon.\n";
	}
	return 0;
}
