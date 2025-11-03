// This is lostallmymoney's remake of RaulPPelaez's original tool.
// RaulPPelaez, et. al wrote the original file.  As long as you retain this notice you
// can do whatever you want with this stuff.

#include <algorithm>
#include <iostream>
#include <vector>
#include <cstring>
#include <cctype>
#include <fstream>
#include <fcntl.h>
#include <unistd.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <sys/ioctl.h>
#include <cerrno>
#include "extraButtonCapture.hpp"
#include <thread>
#include <mutex>
#include <map>
#include <memory>
#include <atomic>
#include <utility>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <tuple>
#include <functional>
#include <string>
#include "nagaDotoolLib.hpp"
#include "waylandWindowExtLib.hpp"
using namespace std;
using nagaDotool::closeDotoolPipe;
using nagaDotool::initDotoolPipe;
using nagaDotool::writeDotoolCommand;
using nagaWaylandWindowExt::getTitle;

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
	const string *const prefix, *const suffix;
	const bool onKeyPressed;
	void (*const internalFunction)(const string *const c);

public:
	bool IsOnKeyPressed() const { return onKeyPressed; }
	void run(const string *const content) const { internalFunction(content); }
	const string *Prefix() const { return prefix; }
	const string *Suffix() const { return suffix; }

	nagaCommandClass(const bool tonKeyPressed, void (*const tinternalF)(const string *const cc), const string tprefix = "", const string tsuffix = "") : prefix(new string(tprefix)), suffix(new string(tsuffix)), onKeyPressed(tonKeyPressed), internalFunction(tinternalF)
	{
	}
};

class MacroEvent : public IMacroEvent
{
private:
	const nagaCommandClass *const command;
	const string *const commandArgument;

public:
	MacroEvent(const nagaCommandClass *const commandPtr, const string *const commandArgumentPtr) : command(commandPtr), commandArgument(commandArgumentPtr)
	{
	}
	void runInternal() const override
	{
		command->run(commandArgument);
	}
};

class loop {
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
	const loop *const aNagaLoop;
	function<void()> loopAction;

public:
	loopMacroEvent(const loop *const taNagaLoop, const string *const taNagaLoopArgument) : aNagaLoop(taNagaLoop)
	{
		if (*taNagaLoopArgument == "start")
		{
			loopAction = [loopPtr = aNagaLoop]()
			{ loopPtr->run(); };
		}
		else if (*taNagaLoopArgument == "stop")
		{
			loopAction = [loopPtr = aNagaLoop]()
			{ loopPtr->stop(); };
		}
		else
		{
			try
			{
				const long long parsedTimes = stoll(*taNagaLoopArgument);
				if (parsedTimes > 0)
				{
					const size_t convertedTimes = static_cast<size_t>(parsedTimes);
					loopAction = [loopPtr = aNagaLoop, convertedTimes]()
					{ loopPtr->runThisManyTimes(convertedTimes); };
				}
				else if (parsedTimes < 0)
				{
					const size_t convertedTimes = static_cast<size_t>(-parsedTimes);
					loopAction = [loopPtr = aNagaLoop, convertedTimes]()
					{ loopPtr->runThisManyTimesWithoutStop(convertedTimes); };
				}
				else
				{
					clog << "Invalid loop argument (zero): " << *taNagaLoopArgument << endl;
				}
			}
			catch (...)
			{
				clog << "Invalid loop argument: " << *taNagaLoopArgument << endl;
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
using IMacroEventKeyMaps = map<string, IMacroEventKeyMap>;

static IMacroEventKeyMaps macroEventKeyMaps;
static map<string, loop *> loopsMap;
static map<string, vector<string>> contextMap;

class nagaFunction
{
public:
	vector<MacroEvent *> eventList;
	void addEvent(MacroEvent *const newEvent)
	{
		eventList.emplace_back(newEvent);
	}
	void run()
	{
		for (MacroEvent *const macroEvent : eventList)
		{
			macroEvent->runInternal();
		}
	}
};

static map<string, nagaFunction *> functionsMap;

class configSwitchScheduler
{
private:
	bool scheduledReMap = false, winConfigActive = false, scheduledUnlock = false, forceRecheck = false, notifyOnNextLoad = false;
	const string *currentConfigName = nullptr, *scheduledReMapName = nullptr, *bckConfName = nullptr;
	string lastLoggedWindow;

public:
	map<const string, pair<bool, const string *> *> *configWindowAndLockMap = new map<const string, pair<bool, const string *> *>();
	IMacroEventKeyMap *currentConfigPtr;
	map<const string, pair<bool, const string *> *>::iterator currentWindowConfigPtr, scheduledUnlockWindowCfgPtr;
	map<string, const char *> notifySendMap;
	void loadConf(bool silent = false)
	{
		if (notifyOnNextLoad)
			silent = false;

		scheduledReMap = notifyOnNextLoad = false;
		if (!macroEventKeyMaps.contains(*scheduledReMapName))
		{
			clog << "Undefined profile : " << *scheduledReMapName << endl;
			return;
		}
		currentConfigName = scheduledReMapName;
		currentConfigPtr = &macroEventKeyMaps[*scheduledReMapName];
		if (!silent)
			std::ignore = system(notifySendMap[*scheduledReMapName]);
	}

	void checkForWindowConfig()
	{
		const string currAppClass(getTitle());
		if (currAppClass != lastLoggedWindow)
		{
			clog << "WindowNameLog : " << currAppClass << endl;
			lastLoggedWindow.assign(currAppClass);
		}
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

	void remapRoutine()
	{
		lock_guard<mutex> guard(configSwitcherMutex);
		if (scheduledUnlock)
		{
			scheduledUnlockWindowCfgPtr->second->first = scheduledUnlock = false;
			forceRecheck = notifyOnNextLoad = true;
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
			currentWindowConfigPtr->second->first = forceRecheck = notifyOnNextLoad = true;
			currentWindowConfigPtr->second->second = reMapStr;
		}
		else
		{
			scheduledReMapName = reMapStr;
			scheduledReMap = true;
		}
	}

	void scheduleUnlockChmap(const string *const unlockStr)
	{
		bool shouldRecheck = false;
		{
			lock_guard<mutex> guard(configSwitcherMutex);
			scheduledUnlockWindowCfgPtr = configWindowAndLockMap->find(*unlockStr);
			if (scheduledUnlockWindowCfgPtr != configWindowAndLockMap->end() && scheduledUnlockWindowCfgPtr->second->first)
			{
				scheduledUnlock = shouldRecheck = true;
			}
		}
		if (shouldRecheck)
			checkForWindowConfig();
	}
};

static configSwitchScheduler *const configSwitcher = new configSwitchScheduler();

class NagaDaemon
{
private:
	static constexpr size_t BufferSize = 1024;
	static constexpr size_t DotoolCommandLimit = 65536;
	map<string, nagaCommandClass *const> nagaCommandsMap;

	struct input_event ev1[64];
	const int size = sizeof(ev1);
	vector<pair<const char *const, const char *const>> devices;
	bool areSideBtnEnabled = true, areExtraBtnEnabled = true;

	unique_ptr<UInputForwarder> extraForwarder;
	bool extraDeviceGrabbed = false;

	void initConf()
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

		string commandContent, commandContent2;
		IMacroEventKeyMap *iteratedConfig;
		nagaFunction *currentFunction = nullptr;
		bool isIteratingConfig = false, isIteratingLoop = false, isIteratingFunction = false, isIteratingContext = false;

		ifstream in(conf_file.c_str(), ios::in);
		if (!in)
		{
			cerr << "Cannot open " << conf_file << ". Exiting." << endl;
			exit(1);
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
					std::function<void(std::string)> parseCommand = [&](std::string commandContent)
					{
						std::string::size_type pos = commandContent.find('=');
						std::string commandType = commandContent.substr(0, pos);
						commandContent.erase(0, pos + 1);
						normalizeCommandType(commandType);
						pos = commandType.find("-");
						const std::string buttonNumber = commandType.substr(0, pos);
						commandType = commandType.substr(pos + 1);

						int buttonNumberInt;
						try
						{
							buttonNumberInt = std::stoi(buttonNumber) + 1;
						}
						catch (...)
						{
							std::clog << "CONFIG ERROR : " << commandContent << std::endl;
							std::exit(1);
						}

						std::map<bool, std::vector<IMacroEvent *>> *iteratedButtonConfig = &(*iteratedConfig)[buttonNumberInt];

						if (nagaCommandsMap.contains(commandType))
						{
							if (!nagaCommandsMap[commandType]->Prefix()->empty())
								commandContent = *nagaCommandsMap[commandType]->Prefix() + commandContent;

							if (!nagaCommandsMap[commandType]->Suffix()->empty())
								commandContent = commandContent + *nagaCommandsMap[commandType]->Suffix();

							(*iteratedButtonConfig)[nagaCommandsMap[commandType]->IsOnKeyPressed()]
								.emplace_back(new MacroEvent(nagaCommandsMap[commandType], new std::string(commandContent)));
						}
						else if (commandType == "key")
						{
							std::string commandContent2 =
								*nagaCommandsMap["keyreleaseonrelease"]->Prefix() + commandContent +
								*nagaCommandsMap["keyreleaseonrelease"]->Suffix();

							commandContent =
								*nagaCommandsMap["keypressonpress"]->Prefix() + commandContent +
								*nagaCommandsMap["keypressonpress"]->Suffix();

							(*iteratedButtonConfig)[true]
								.emplace_back(new MacroEvent(nagaCommandsMap["keypressonpress"], new std::string(commandContent)));
							(*iteratedButtonConfig)[false]
								.emplace_back(new MacroEvent(nagaCommandsMap["keyreleaseonrelease"], new std::string(commandContent2)));
						}
						else if (commandType == "loop" || commandType == "loop2")
						{
							std::string loopName = commandContent;
							std::string pressArgument = "start";
							const std::string::size_type loopArgPos = commandContent.find('=');
							bool shouldAddStop = true;

							if (loopArgPos != std::string::npos)
							{
								loopName = commandContent.substr(0, loopArgPos);
								nukeWhitespaces(loopName);
								pressArgument = commandContent.substr(loopArgPos + 1);

								try
								{
									if (std::stoll(pressArgument) < 0)
										shouldAddStop = false;
								}
								catch (...)
								{
								}
							}
							else
							{
								nukeWhitespaces(loopName);
							}

							std::map<std::string, loop *>::iterator loopIt = loopsMap.find(loopName);
							if (loopIt == loopsMap.end())
							{
								std::clog << "Discarding loop binding, undefined loop: " << loopName << std::endl;
								return;
							}

							const loop *const loopPtr = loopIt->second;

							if (commandType == "loop2")
							{
								(*iteratedButtonConfig)[true]
									.emplace_back(new ThreadedLoopMacroEvent(loopPtr, new std::string(pressArgument)));

								if (shouldAddStop)
								{
									(*iteratedButtonConfig)[false]
										.emplace_back(new ThreadedLoopMacroEvent(loopPtr, new std::string("stop")));
								}
							}
							else
							{
								(*iteratedButtonConfig)[true]
									.emplace_back(new loopMacroEvent(loopPtr, new std::string(pressArgument)));

								if (shouldAddStop)
								{
									(*iteratedButtonConfig)[false]
										.emplace_back(new loopMacroEvent(loopPtr, new std::string("stop")));
								}
							}
						}
						else if (commandType == "function" || commandType == "functionrelease")
						{
							nukeWhitespaces(commandContent);
							std::map<std::string, nagaFunction *>::iterator functionIt = functionsMap.find(commandContent);
							if (functionIt == functionsMap.end())
							{
								std::clog << "Discarding function binding, undefined function: " << commandContent << std::endl;
								return;
							}
							for (MacroEvent *const funcEvent : functionIt->second->eventList)
							{
								if(commandType == "function")
									(*iteratedButtonConfig)[true].emplace_back(funcEvent);
								else
									(*iteratedButtonConfig)[false].emplace_back(funcEvent);
							}
						}
						else
						{
							std::clog << "Discarding : " << commandType << "=" << commandContent << std::endl;
						}
					};

					if (commandContent.substr(0, 8) == "context=")
					{
						commandContent.erase(0, 8);
						nukeWhitespaces(commandContent);
						for (const string &contextItem : contextMap[commandContent])
						{
							parseCommand(contextItem);
						}
					}
					else
					{
						parseCommand(commandContent);
					}
				}
			}
			else if (commandContent.substr(0, 9) == "function=")
			{
				commandContent.erase(0, 9);
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
						string::size_type pos = commandContent2.find('=');
						string commandType2 = commandContent2.substr(0, pos);
						commandContent2.erase(0, pos + 1);
						normalizeCommandType(commandType2); // Normalize command type
						if (commandType2 == "function")//function in function
						{
							nukeWhitespaces(commandContent2);
							std::map<std::string, nagaFunction *>::iterator functionIt = functionsMap.find(commandContent2);
							if (functionIt == functionsMap.end())
							{
								clog << "Discarding in function: undefined function " << commandContent2 << endl;
								continue;
							}

							for (MacroEvent *const funcEvent : functionIt->second->eventList)
								newFunction->addEvent(funcEvent);
							continue;
						}

						if (nagaCommandsMap.contains(commandType2) && nagaCommandsMap[commandType2]->IsOnKeyPressed() == true)
						{
							newFunction->addEvent(new MacroEvent(nagaCommandsMap[commandType2], new string(commandContent2)));
						}
						else
						{
							clog << "Discarding in function: " << commandType2 << "=" << commandContent2 << endl;
						}
					}
				}
			}
			else if (commandContent.substr(0, 5) == "loop=")
			{
				commandContent.erase(0, 5);
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
						const string::size_type pos = commandContent2.find('=');

						string commandType2 = commandContent2.substr(0, pos);
						string commandValue2 = commandContent2.substr(pos + 1);
						normalizeCommandType(commandType2);

						if (nagaCommandsMap.contains(commandType2) && nagaCommandsMap[commandType2]->IsOnKeyPressed() == true)
						{
							newLoop->addEvent(new MacroEvent(nagaCommandsMap[commandType2], new string(commandValue2)));
						}
						else if (commandType2 == "function")
						{
							nukeWhitespaces(commandValue2);

							std::map<std::string, nagaFunction *>::iterator functionIt = functionsMap.find(commandValue2);
							if (functionIt == functionsMap.end())
							{
								clog << "Discarding in loop: undefined function " << commandValue2 << endl;
								continue;
							}

							for (MacroEvent *const funcEvent : functionIt->second->eventList)
							{
								newLoop->addEvent(funcEvent);
							}
						}
						else if (commandType2 == "loop" || commandType2 == "loop2") // nested loops
						{
							std::string loopCommand = commandValue2;
							std::string loopName = loopCommand;
							std::string loopArgument = "start";
							bool shouldAddStop = true;
							const std::string::size_type loopArgPos = loopCommand.find('=');

							if (loopArgPos != std::string::npos)
							{
								loopName = loopCommand.substr(0, loopArgPos);
								nukeWhitespaces(loopName);
								loopArgument = loopCommand.substr(loopArgPos + 1);

								try
								{
									if (std::stoll(loopArgument) < 0)
										shouldAddStop = false;
								}
								catch (...)
								{
								}
							}
							else
							{
								nukeWhitespaces(loopName);
							}

							std::map<std::string, loop *>::iterator loopIt = loopsMap.find(loopName);
							if (loopIt == loopsMap.end())
							{
								clog << "Discarding in loop: undefined nested loop " << loopName << endl;
								continue;
							}

							const loop *const loopPtr = loopIt->second;

							if (commandType2 == "loop2")
							{
								newLoop->addEvent(new ThreadedLoopMacroEvent(loopPtr, new std::string(loopArgument)));

								if (shouldAddStop)
								{
									newLoop->addEvent(new ThreadedLoopMacroEvent(loopPtr, new std::string("stop")));
								}
							}
							else
							{
								newLoop->addEvent(new loopMacroEvent(loopPtr, new std::string(loopArgument)));

								if (shouldAddStop)
								{
									newLoop->addEvent(new loopMacroEvent(loopPtr, new std::string("stop")));
								}
							}
						}
						else
						{
							clog << "Discarding in loop: " << commandType2 << "=" << commandValue2 << endl;
						}
					}
				}
			}
			else if (commandContent.substr(0, 8) == "context=")
			{
				commandContent.erase(0, 8);
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
						std::string nestedContextName = commandContent2.erase(0, 8);
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
			else if (commandContent.substr(0, 13) == "configWindow=")
			{
				isIteratingConfig = true;
				commandContent.erase(0, 13);
				nukeWhitespaces(commandContent);
				iteratedConfig = &macroEventKeyMaps[commandContent];
				(*configSwitcher->configWindowAndLockMap)[commandContent] = new pair<bool, const string *>(false, new string(""));
				configSwitcher->notifySendMap.emplace(commandContent, (new string("notify-send -a Naga \"Profile : " + commandContent + "\""))->c_str());
			}
			else if (commandContent.substr(0, 7) == "config=")
			{
				isIteratingConfig = true;
				commandContent.erase(0, 7);
				nukeWhitespaces(commandContent);
				iteratedConfig = &macroEventKeyMaps[commandContent];
				configSwitcher->notifySendMap.emplace(commandContent, (new string("notify-send -a Naga \"Profile : " + commandContent + "\""))->c_str());
			}
		}
		in.close();
	}

	int side_btn_fd, extra_btn_fd;
	fd_set readset;

	void run()
	{
		while (true)
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
				ssize_t bytesRead = read(side_btn_fd, ev1, static_cast<size_t>(size));
				if (bytesRead == -1)
					exit(2);
				if (static_cast<size_t>(bytesRead) % sizeof(input_event) != 0)
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
				ssize_t bytesRead = read(extra_btn_fd, ev1, static_cast<size_t>(size));
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

	static void mouseClick(const string *const macroContent)
	{
		writeDotoolCommand("click " + *macroContent);
	}

	static void chmapNow(const string *const macroContent)
	{
		configSwitcher->scheduleReMap(macroContent); // schedule config switch/change
	}

	static void sleepNow(const string *const macroContent)
	{
		usleep(static_cast<useconds_t>(stoul(*macroContent) * 1000)); // microseconds make me dizzy in keymap.txt
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
		FILE *fp = pipe.get();
		for (int ch = fgetc(fp), counter = 0; ch != EOF; ch = fgetc(fp))
		{
			if (ch == '\n')
			{
				if (!currentLine.empty())
				{
					writeDotoolCommand("type " + currentLine);
					counter = 0;
					currentLine.clear();
				}
				writeDotoolCommand("key enter");
				writeDotoolCommand("key home");
			}
			else
			{
				currentLine.push_back(static_cast<char>(ch));
				counter++;
				if (counter >= DotoolCommandLimit)
				{
					writeDotoolCommand("type " + currentLine);
					counter = 0;
					currentLine.clear();
				}
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
		std::ignore = system(macroContent->c_str());
	}
	static void executeThreadNow(const string *const macroContent)
	{
		thread(executeNow, macroContent).detach();
	}

	static void unlockChmap(const string *const macroContent)
	{
		configSwitcher->scheduleUnlockChmap(macroContent);
	}
	// end of configKeys functions

	static void runActions(vector<IMacroEvent *> *const relativeMacroEventsPointer)
	{
		for (IMacroEvent *const macroEvent : *relativeMacroEventsPointer)
		{ // run all the events at Key
			macroEvent->runInternal();
		}
	}

	void emplaceConfigKey(const string &nagaCommand, bool onKeyPressed, void (*functionPtr)(const string *const), const string &prefix = "", const string &suffix = "")
	{
		nagaCommandsMap.emplace(nagaCommand, new nagaCommandClass(onKeyPressed, functionPtr, prefix, suffix));
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
			side_btn_fd = open(device.first, O_RDONLY);
			extra_btn_fd = open(device.second, O_RDONLY);

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

		if (areSideBtnEnabled)
			ioctl(side_btn_fd, EVIOCGRAB, 1); // Give application exclusive control over side buttons.

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
		// options must be lowercase here don't get caught
#define ONKEYPRESSED true
#define ONKEYRELEASED false

		emplaceConfigKey("click", ONKEYPRESSED, mouseClick);
		emplaceConfigKey("clickrelease", ONKEYRELEASED, mouseClick);

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

		emplaceConfigKey("unlockchmap", ONKEYPRESSED, unlockChmap);
		emplaceConfigKey("unlockchmaprelease", ONKEYRELEASED, unlockChmap);

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
			std::ignore = system("/usr/local/bin/Naga_Linux/nagaXinputStart.sh");
			initDotoolPipe();
			if (argc > 2 && argv[2][0] != '\0')
				NagaDaemon nagaDaemon(argv[2]); // lets you configure a default profile in /etc/systemd/system/naga.service
			else
				NagaDaemon nagaDaemon;
		}
		else if (strstr(argv[1], "start"))
		{
			clog << "Starting naga daemon as service, naga debug to see logs..." << endl;
			usleep(100000);
			std::ignore = system("sudo systemctl restart naga");
		}
		else if (strstr(argv[1], "debug"))
		{
			clog << "Starting naga debug, logs :" << endl;
			if (argc > 2)
			{
				std::ignore = system(("journalctl " + string(argv[2]) + " naga").c_str());
			}
			else
			{
				std::ignore = system("journalctl -fu naga");
			}
		}
		else if (strstr(argv[1], "kill"))
		{
			clog << "Killing naga daemon processes:" << endl;
			std::ignore = system(("sudo sh /usr/local/bin/Naga_Linux/nagaKillroot.sh " + to_string((int)getpid())).c_str());
		}
		else if (strstr(argv[1], "stop"))
		{
			clog << "Stopping possible naga daemon" << endl;
			std::ignore = system("sudo systemctl stop naga");
		}
		else if (strstr(argv[1], "disable") || strstr(argv[1], "stop"))
		{
			clog << "Disabling naga daemon" << endl;
			std::ignore = system("sudo systemctl disable naga");
		}
		else if (strstr(argv[1], "enable") || strstr(argv[1], "stop"))
		{
			clog << "Enabling naga daemon" << endl;
			std::ignore = system("sudo systemctl enable naga");
		}
		else if (strstr(argv[1], "repair") || strstr(argv[1], "tame") || strstr(argv[1], "fix"))
		{
			clog << "Fixing dead keypad syndrome... STUTTER!!" << endl;
			std::ignore = system("sudo bash -c \"sh /usr/local/bin/Naga_Linux/nagaKillroot.sh && modprobe -r usbhid && modprobe -r psmouse && modprobe usbhid && modprobe psmouse && sleep 1 && sudo systemctl start naga\"");
		}
		else if (strstr(argv[1], "edit"))
		{
			if (argc > 2)
			{
				std::ignore = system(("sudo bash -c 'orig_sum=\"$(sudo md5sum " + conf_file + ")\"; " + string(argv[2]) + " " + conf_file + "; [[ \"$(sudo md5sum " + conf_file + ")\" != \"$orig_sum\" ]] && sudo systemctl restart naga'").c_str());
			}
			else
			{
				std::ignore = system(("sudo bash -c 'orig_sum=\"$(sudo md5sum " + conf_file + ")\"; sudo nano -m " + conf_file + "; [[ \"$(sudo md5sum " + conf_file + ")\" != \"$orig_sum\" ]] && sudo systemctl restart naga'").c_str());
			}
		}
		else if (strstr(argv[1], "uninstall"))
		{
			string answer;
			clog << "Are you sure you want to uninstall ? y/n" << endl;
			cin >> answer;
			if (answer.size() != 1 || (answer[0] != 'y' && answer[0] != 'Y'))
			{
				clog << "Aborting" << endl;
			}
			else
			{
				std::ignore = system("/usr/local/bin/Naga_Linux/nagaUninstall.sh");
			}
		}
	}
	else
	{
		clog << "Possible arguments : \n  start          Starts the daemon in hidden mode. (stops it before)\n  stop           Stops the daemon.\n  edit           Lets you edit the config.\n  debug		 Shows log.\n  fix            For dead keypad.\n  uninstall      Uninstalls the daemon." << endl;
	}
	return 0;
}
