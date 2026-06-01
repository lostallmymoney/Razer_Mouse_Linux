#pragma once

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

extern std::string conf_file;

namespace nagaSettings
{
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

	inline std::string trim(std::string value)
	{
		const std::string::size_type start = value.find_first_not_of(" \t\r\n");

		if (start == std::string::npos)
			return "";

		const std::string::size_type end = value.find_last_not_of(" \t\r\n");
		return value.substr(start, end - start + 1);
	}

	inline void readTextFile(const std::string &filePath, const bool fatalIfMissing, TextFile &file)
	{
		file = TextFile{};
		std::ifstream in(filePath.c_str(), std::ios::binary | std::ios::ate);

		if (!in)
		{
			if (fatalIfMissing)
			{
				std::cerr << "\033[91mError : Cannot open " << filePath << ". Exiting.\033[0m\n";
				std::exit(1);
			}

			return;
		}

		const std::streamsize fileSize = in.tellg();
		in.seekg(0);

		file.contents.assign(fileSize, '\0');

		if (!in.read(file.contents.data(), fileSize))
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

		std::size_t pos = 0;

		while (pos < file.contents.size())
		{
			std::size_t nextPos = file.contents.find('\n', pos);

			if (nextPos == std::string::npos)
				nextPos = file.contents.size();

			std::string_view line(file.contents.data() + pos, nextPos - pos);

			if (!line.empty() && line.back() == '\r')
				line.remove_suffix(1);

			file.lines.emplace_back(line);

			pos = nextPos + 1;
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

	inline std::string shellQuote(const std::string &value)
	{
		std::string quoted = "'";

		for (const char c : value)
		{
			if (c == '\'')
				quoted += "'\\''";
			else
				quoted += c;
		}

		quoted += "'";
		return quoted;
	}

	inline std::string doubleQuoteSafe(const std::string &value)
	{
		std::string escaped;

		for (const char c : value)
		{
			if (c == '\\' || c == '"' || c == '$' || c == '`')
				escaped += '\\';

			escaped += c;
		}

		return escaped;
	}

	inline std::string settingValue(const std::string &settingName,
	                                const std::string &variableName = "",
	                                const std::string &compiledValue = "")
	{
		TextFile &settingsFile = cachedSettingsFile();

		for (const std::string_view line : settingsFile.lines)
		{
			const std::string trimmedLine = trim(std::string(line));

			if (trimmedLine.empty() || trimmedLine[0] == '#')
				continue;

			const std::string::size_type equalPos = trimmedLine.find('=');
			if (equalPos == std::string::npos)
				continue;

			if (trim(trimmedLine.substr(0, equalPos)) != settingName)
				continue;

			std::string value = trim(trimmedLine.substr(equalPos + 1));

			if (variableName.empty())
				return value;

			const std::string variable = "$" + variableName;
			std::string::size_type pos = 0;

			while ((pos = value.find(variable, pos)) != std::string::npos)
			{
				value.replace(pos, variable.length(), compiledValue);
				pos += compiledValue.length();
			}

			return value;
		}

		return "";
	}

	inline int editFile(const int argc, const char *const argv[], const std::string &targetFile)
	{
		const std::string editorCommand = argc > 2
			? std::string(argv[2]) + " " + shellQuote(targetFile)
			: settingValue("nagaEditCommand", "nagaConfigFile", shellQuote(targetFile));

		if (editorCommand.empty())
		{
			std::cerr << "\033[91mError : Missing nagaEditCommand in ~/.naga/nagaSettings.txt\033[0m\n";
			return 1;
		}

		const std::string quotedTargetFile = shellQuote(targetFile);

		const std::string editScript =
			"orig_sum=\"$(sudo md5sum " + quotedTargetFile + " 2>/dev/null)\"; " +
			editorCommand +
			"; [[ \"$(sudo md5sum " + quotedTargetFile + " 2>/dev/null)\" != \"$orig_sum\" ]] && sudo systemctl restart naga";

		std::ignore = system(("sudo bash -c " + shellQuote(editScript)).c_str());
		return 0;
	}

	inline std::string buildNotifyCommand(const std::string &commandContent)
	{
		return settingValue("nagaNotifyCommand", "profileName", doubleQuoteSafe(commandContent));
	}
}