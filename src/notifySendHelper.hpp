#pragma once

#include <chrono>
#include <condition_variable>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>

namespace notifySendHelper
{
	// D-Bus notification pipeline: executes one argv as notify-send (or a custom
	// handler verbatim), keeps at most one notification alive, and force-closes it
	// via gdbus after its "-t <ms>" delay. Custom utilities are never modified.

	inline constexpr char notifySendCommand[] = "notify-send";
	inline constexpr char notifySendPrintIdFlag[] = "-p";
	inline constexpr char gdbusCommand[] = "gdbus";
	inline constexpr char notificationsBusName[] = "org.freedesktop.Notifications";
	inline constexpr char notificationsObjectPath[] = "/org/freedesktop/Notifications";
	inline constexpr char closeNotificationMethod[] = "org.freedesktop.Notifications.CloseNotification";
	inline constexpr std::size_t pipelineBufferSize = 64;
	inline constexpr unsigned defaultCloseDelayMs = 1000;

	inline std::mutex notificationMutex;
	inline std::condition_variable notificationCv;
	inline std::string activeNotificationId;

	// Precalculated notification ready to be executed without runtime argv construction.
	struct NotificationCommand
	{
		std::vector<std::string> arguments;
		std::vector<char *> argv;
		unsigned timeoutMs = defaultCloseDelayMs;
		bool captureOutput = false;

		bool empty() const
		{
			return arguments.empty() || argv.empty() || argv.front() == nullptr;
		}
	};

	inline std::shared_ptr<const NotificationCommand> pendingNotification;

	// Auto-close delay = the notify-send convention "-t <ms>" token; malformed or
	// absent (custom handler) → defaultCloseDelayMs.
	inline unsigned closeDelayMs(const std::vector<std::string> &notificationArguments)
	{
		for (std::size_t index = 0; index + 1 < notificationArguments.size(); ++index)
		{
			if (notificationArguments[index] == "-t")
			{
				try
				{
					return static_cast<unsigned>(std::stoul(notificationArguments[index + 1]));
				}
				catch (...)
				{
					return defaultCloseDelayMs;
				}
			}
		}
		return defaultCloseDelayMs;
	}

	// Basename-only "notify-send" match; any path prefix is accepted.
	inline bool isNotifySendCommand(const std::string_view executable)
	{
		const std::size_t lastSlash = executable.rfind('/');
		const std::string_view baseName = (lastSlash == std::string_view::npos)
			? executable : executable.substr(lastSlash + 1);
		return baseName == notifySendCommand;
	}

	// Precalculate notification command structure at setting initialization time.
	// Builds the complete argument list, resolves executable/-p flags, caches timeout,
	// and prepares the NULL-terminated argv vector so runtime execution does zero allocations.
	inline std::shared_ptr<NotificationCommand> prepare(const std::vector<std::string> &tokens)
	{
		if (tokens.empty() || tokens.front().empty())
			return nullptr;

		const std::shared_ptr<NotificationCommand> command = std::make_shared<NotificationCommand>();
		const bool defaultNotifySend = tokens.front().front() == '-';
		const bool explicitNotifySend = isNotifySendCommand(tokens.front());
		command->captureOutput = defaultNotifySend || explicitNotifySend;

		command->arguments.reserve(tokens.size() + 2);
		if (defaultNotifySend || explicitNotifySend)
		{
			command->arguments.emplace_back(defaultNotifySend
				? notifySendCommand : tokens.front());
			command->arguments.emplace_back(notifySendPrintIdFlag);
		}
		for (std::size_t index = explicitNotifySend ? 1 : 0; index < tokens.size(); ++index)
			command->arguments.emplace_back(tokens[index]);

		command->timeoutMs = closeDelayMs(command->arguments);

		command->argv.reserve(command->arguments.size() + 1);
		for (std::string &arg : command->arguments)
			command->argv.emplace_back(arg.data());
		command->argv.emplace_back(nullptr);

		return command;
	}

	// Forced dismissal of a prior notification id via gdbus CloseNotification
	// (fork+exec, no pipe needed).
	inline void closeNotification(const std::string &notificationId)
	{
		if (notificationId.empty())
			return;

		const pid_t childPid = fork();
		if (childPid == 0)
		{
			const int devNull = open("/dev/null", O_WRONLY);
			if (devNull != -1)
			{
				dup2(devNull, STDOUT_FILENO);
				dup2(devNull, STDERR_FILENO);
				close(devNull);
			}
			execlp(gdbusCommand, gdbusCommand, "call", "--session", "--dest",
				notificationsBusName, "--object-path", notificationsObjectPath, "--method",
				closeNotificationMethod, notificationId.c_str(), nullptr);
			_exit(127);
		}

		if (childPid > 0)
			waitpid(childPid, nullptr, 0);
	}

	// Execute precalculated argv directly without building any argv at runtime.
	inline std::string sendNow(const NotificationCommand &command)
	{
		if (command.empty())
			return "";

		int stdoutPipe[2];
		if (command.captureOutput && pipe(stdoutPipe) == -1)
			return "";

		const pid_t childPid = fork();
		if (childPid == -1)
		{
			if (command.captureOutput)
			{
				close(stdoutPipe[0]);
				close(stdoutPipe[1]);
			}
			return "";
		}

		if (childPid == 0)
		{
			if (command.captureOutput)
			{
				dup2(stdoutPipe[1], STDOUT_FILENO);
				close(stdoutPipe[0]);
				close(stdoutPipe[1]);
			}

			execvp(command.argv[0], command.argv.data());
			_exit(127);
		}

		// Custom utility: no capture needed; just waitpid and return.
		if (!command.captureOutput)
		{
			waitpid(childPid, nullptr, 0);
			return "";
		}

		close(stdoutPipe[1]);
		std::string notificationId;
		char readBuffer[pipelineBufferSize];
		ssize_t bytesRead;
		while ((bytesRead = read(stdoutPipe[0], readBuffer, sizeof(readBuffer))) > 0)
			notificationId.append(readBuffer, static_cast<std::size_t>(bytesRead));
		close(stdoutPipe[0]);

		int childStatus = 0;
		waitpid(childPid, &childStatus, 0);

		while (!notificationId.empty() &&
			(notificationId.back() == '\n' || notificationId.back() == '\r'))
			notificationId.pop_back();

		if (!WIFEXITED(childStatus) || WEXITSTATUS(childStatus) != 0)
			return "";

		for (const char character : notificationId)
		{
			if (character < '0' || character > '9')
				return "";
		}

		return notificationId;
	}

	inline void notificationWorker()
	{
		while (true)
		{
			std::shared_ptr<const NotificationCommand> notification;
			{
				std::unique_lock<std::mutex> lock(notificationMutex);
				notificationCv.wait(lock, []()
				{
					return pendingNotification != nullptr;
				});
				notification = std::move(pendingNotification);
			}

			if (!activeNotificationId.empty())
			{
				closeNotification(activeNotificationId);
				activeNotificationId.clear();
			}

			const std::string notificationId = sendNow(*notification);
			if (notificationId.empty())
				continue;

			activeNotificationId = notificationId;
			{
				std::unique_lock<std::mutex> lock(notificationMutex);
				notificationCv.wait_for(lock, std::chrono::milliseconds(notification->timeoutMs), []()
				{
					return pendingNotification != nullptr;
				});
			}
			closeNotification(activeNotificationId);
			activeNotificationId.clear();
		}
	}

	// Keep only the newest pending notification; newer profiles supersede older ones.
	inline void sendNotification(const std::shared_ptr<const NotificationCommand> &notification)
	{
		if (!notification || notification->empty())
			return;

		static std::once_flag workerFlag;
		std::call_once(workerFlag, []()
		{
			std::thread(notificationWorker).detach();
		});
		{
			std::lock_guard<std::mutex> lock(notificationMutex);
			pendingNotification = notification;
		}
		notificationCv.notify_one();
	}
}
