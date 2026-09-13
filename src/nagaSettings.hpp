#pragma once

#include "nagaText.hpp"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace nagaSettings
{
	// Accessors for ~/.naga/nagaSettings.txt (flat "key = value" lines).
	// Unset/missing settings read as "" (or a provided default).
	// The file is read once and cached: live edits are only seen after reload.

	struct TextFile
	{
		std::string contents;
		std::vector<std::string_view> lines;
	};

	inline std::string settingsPath()
	{
		const char *const home = getenv("HOME");
		return home == nullptr ? "" : std::string(home) + "/.naga/nagaSettings.txt";
	}

	inline void readTextFile(const std::string &filePath, const bool fatalIfMissing, TextFile &file)
	{
		file = TextFile{};
		std::ifstream input(filePath, std::ios::binary | std::ios::ate);
		if (!input)
		{
			if (fatalIfMissing)
			{
				std::cerr << "\033[91mError : Cannot open " << filePath << ". Exiting.\033[0m\n";
				std::exit(1);
			}
			return;
		}

		const std::streamsize fileSize = input.tellg();
		input.seekg(0);
		file.contents.assign(fileSize, '\0');
		if (!input.read(file.contents.data(), fileSize))
		{
			if (fatalIfMissing)
			{
				std::cerr << "\033[91mError : Failed to read " << filePath << ". Exiting.\033[0m\n";
				std::exit(1);
			}
			file = TextFile{};
			return;
		}

		file.lines.reserve(std::count(file.contents.begin(), file.contents.end(), '\n') + 1);
		std::size_t position = 0;
		while (position < file.contents.size())
		{
			const std::size_t newlinePos = file.contents.find('\n', position);
			const std::size_t lineEnd = newlinePos == std::string::npos ? file.contents.size() : newlinePos;
			std::string_view line(file.contents.data() + position, lineEnd - position);
			if (!line.empty() && line.back() == '\r')
				line.remove_suffix(1);
			file.lines.emplace_back(line);
			position = newlinePos == std::string::npos ? file.contents.size() : newlinePos + 1;
		}
	}

	inline TextFile &cachedSettingsFile()
	{
		static TextFile file;
		static bool loaded = false;
		if (!loaded)
		{
			readTextFile(settingsPath(), false, file);
			loaded = true;
		}
		return file;
	}

	// First matching line wins; skips blank, # comment, and '='-less lines.
	inline std::string readSetting(const std::string_view name)
	{
		for (const std::string_view line : cachedSettingsFile().lines)
		{
			const std::string text = nagaText::trimWhiteSpaces(line);
			if (nagaText::isBlankOrComment(text) || text.find('=') == std::string::npos)
				continue;
			if (nagaText::trimWhiteSpaces(nagaText::textBefore(text, '=')) == name)
				return nagaText::trimWhiteSpaces(nagaText::textAfter(text, '='));
		}
		return "";
	}

	// readSetting(name) with one "$key" expanded to replacementValue (e.g. "$nagaConfigFile").
	inline std::string readSetting(const std::string_view name,
	                                const std::string &key,
	                                const std::string &replacementValue)
	{
		return nagaText::replaceVariables(readSetting(name), {{key, replacementValue}});
	}

	// Literal "true" → true; absent/empty → defaultValue (notification flags default true).
	inline bool isSettingTrue(const std::string_view name, const bool defaultValue = false)
	{
		const std::string value = readSetting(name);
		return value.empty() ? defaultValue : value == "true";
	}

	// Unset/empty value → fallback.
	inline std::string readSettingOrDefault(const std::string_view name, const std::string &fallback)
	{
		const std::string value = readSetting(name);
		return value.empty() ? fallback : value;
	}

	// Ms as unsigned; non-numeric setting → defaultNotificationTimeoutMs.
	inline constexpr unsigned defaultNotificationTimeoutMs = 1000;
	inline unsigned notificationTimeoutMs()
	{
		try
		{
			return static_cast<unsigned>(
				std::stoul(readSettingOrDefault("notification_timeout",
					std::to_string(defaultNotificationTimeoutMs))));
		}
		catch (...)
		{
			return defaultNotificationTimeoutMs;
		}
	}

	// Use notification_icon path, else base64-decode notification_icon_base64 once into
	// "<settings dir>/naga-notification-icon". system() is injection-safe via shellQuote.
	inline const std::string &notificationIconPath()
	{
		static const std::string path = []()
		{
			const std::string configured = readSetting("notification_icon");
			if (!configured.empty())
				return configured;

			const std::string encoded = readSetting("notification_icon_base64");
			const std::string settingsFile = settingsPath();
			if (encoded.empty() || settingsFile.empty())
				return std::string();

			const std::string iconPath = settingsFile.substr(0, settingsFile.rfind('/') + 1) +
				"naga-notification-icon";
			const std::string command = "printf %s " + nagaText::shellQuote(encoded) +
				" | base64 -d > " + nagaText::shellQuote(iconPath);
			return system(command.c_str()) == 0 ? iconPath : std::string();
		}();
		return path;
	}

	// $notifyOptions argv: -t <ms>, optional -e (expire on dismiss), optional -i <icon>.
	// Order matters to notify-send's option parser.
	inline std::vector<std::string> notificationOptions()
	{
		std::vector<std::string> options;
		options.reserve(6);
		options.emplace_back("-t");
		options.emplace_back(std::to_string(notificationTimeoutMs()));
		if (isSettingTrue("notification_disappear", true))
			options.emplace_back("-e");
		const std::string icon = notificationIconPath();
		if (!icon.empty())
		{
			options.emplace_back("-i");
			options.emplace_back(icon);
		}
		return options;
	}

	// Token filter: drop every staleOption (and its argument when takesArgument),
	// otherwise swap it for replacement. Only used by migrateLegacyNotifyCommand.
	inline std::vector<std::string>
		unstaleOptions(const std::vector<std::string> &tokens, const std::string &staleOption,
		               const std::string &replacement, const bool takesArgument)
	{
		std::vector<std::string> remaining;
		remaining.reserve(tokens.size());
		for (std::size_t index = 0; index < tokens.size(); ++index)
		{
			if (tokens[index] != staleOption)
			{
				remaining.emplace_back(tokens[index]);
				continue;
			}

			if (takesArgument)
				++index;

			if (!replacement.empty())
				remaining.emplace_back(replacement);
		}
		return remaining;
	}

	// Upgrade pre-$notifyOptions commands ("notify-send -a Naga -t 300 "Profile : $profileName""):
	// drop leading notify-send and stale -e, swap stale "-t <ms>" for $notifyOptions.
	// Applies only when the command targets that exact profile tail and has no $notifyOptions.
	inline void migrateLegacyNotifyCommand(std::vector<std::string> &arguments)
	{
		if (arguments.size() < 2 || arguments.front() != "notify-send" ||
			arguments.back() != "Profile : $profileName" ||
			std::find(arguments.begin(), arguments.end(), "$notifyOptions") != arguments.end())
			return;

		std::vector<std::string> options(arguments.begin() + 1, arguments.end() - 1);
		const std::vector<std::string> unstaled =
			unstaleOptions(options, "-t", "$notifyOptions", true);
		if (unstaled == options)
			return;

		std::vector<std::string> migrated =
			unstaleOptions(unstaled, "-e", "", false);
		migrated.emplace_back("$notifyStatus : $profileName");
		arguments = migrated;
	}

	// Final argv for one notification: parse config line → migrate legacy command →
	// splice $notifyOptions → expand $notifyStatus and $profileName.
	inline std::vector<std::string> buildNotifyCommand(const std::string &profileName,
	                                                   const std::string &notifyStatus = "Profile")
	{
		std::vector<std::string> arguments;
		if (!isSettingTrue("notification_enabled", true))
			return arguments;

		const std::string notifyCommandSetting = readSetting("nagaNotifyCommand");
		if (notifyCommandSetting.empty() || notifyCommandSetting == "\"\"" || notifyCommandSetting == "''")
			return {};

		if (!nagaText::parseCommandLine(notifyCommandSetting, arguments))
			return {};

		migrateLegacyNotifyCommand(arguments);

		const std::vector<std::string> options = notificationOptions();
		std::vector<std::string> expanded;
		expanded.reserve(arguments.size() + options.size());
		for (const std::string &argument : arguments)
		{
			if (argument == "$notifyOptions")
			{
				expanded.insert(expanded.end(), options.begin(), options.end());
				continue;
			}
			expanded.emplace_back(nagaText::replaceVariables(argument,
				{{"notifyStatus", notifyStatus}, {"profileName", profileName}}));
		}
		return expanded;
	}

	inline std::vector<std::string> buildUnlockedNotifyCommand(const std::string &profileName)
	{
		return buildNotifyCommand(profileName, "Unlocked");
	}

	// Open target in the editor ($nagaConfigFile or argv[2]); restart the naga service
	// via sudo systemctl if the file content actually changed (md5sum compare).
	inline int editFile(const int argc, const char *const argv[], const std::string &targetFile)
	{
		const std::string editor = argc > 2
			? std::string(argv[2]) + " " + nagaText::shellQuote(targetFile)
			: readSetting("nagaEditCommand", "nagaConfigFile", nagaText::shellQuote(targetFile));
		if (editor.empty())
		{
			std::cerr << "\033[91mError : Missing nagaEditCommand in ~/.naga/nagaSettings.txt\033[0m\n";
			return 1;
		}

		const std::string quotedTarget = nagaText::shellQuote(targetFile);
		const std::string script = "last_sum=\"$(sudo md5sum " + quotedTarget +
			" 2>/dev/null)\"; bash -c " + nagaText::shellQuote(editor) +
			" & editor_pid=$!; while kill -0 \"$editor_pid\" 2>/dev/null; do "
			"current_sum=\"$(sudo md5sum " + quotedTarget +
			" 2>/dev/null)\"; if [[ \"$current_sum\" != \"$last_sum\" ]]; then "
			"sudo systemctl restart naga; last_sum=\"$current_sum\"; fi; sleep 1; done; "
			"current_sum=\"$(sudo md5sum " + quotedTarget +
			" 2>/dev/null)\"; if [[ \"$current_sum\" != \"$last_sum\" ]]; then "
			"sudo systemctl restart naga; fi; wait \"$editor_pid\"";
		std::ignore = system(("sudo bash -c " + nagaText::shellQuote(script)).c_str());
		return 0;
	}

	// Supported variables in ~/.naga/nagaSettings.txt:
	// nagaEditCommand          - Command used to open config files for editing (e.g. "sudo gnome-text-editor $nagaConfigFile")
	// notification_enabled     - Master toggle to enable/disable profile-switch notifications (true / false, defaults to true)
	// notification_timeout     - Duration in milliseconds before notification auto-closes (defaults to 1000)
	// notification_disappear   - Expire notification on dismiss by appending the -e flag (true / false, defaults to true)
	// notification_icon        - File path to the icon displayed in notifications (optional)
	// notification_icon_base64 - Base64 encoded icon fallback written to ~/.naga/naga-notification-icon
	// nagaNotifyCommand        - Custom command or arguments for notifications (supports $notifyOptions, $notifyStatus, $profileName)
}