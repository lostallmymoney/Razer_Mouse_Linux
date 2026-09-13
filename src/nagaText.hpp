#pragma once

#include <algorithm>
#include <cctype>
#include <initializer_list>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace nagaText
{
	// Stateless string/CLI helpers shared by nagaSettings and nagaCore's config parser.
	// These reject unquoted shell metacharacters to prevent injection.

	// Strip surrounding whitespace; "" when fully blank.
	inline std::string trimWhiteSpaces(const std::string_view text)
	{
		const std::string_view::size_type trimStart = text.find_first_not_of(" \t\r\n");
		if (trimStart == std::string_view::npos)
			return "";

		const std::string_view::size_type trimEnd = text.find_last_not_of(" \t\r\n");
		return std::string(text.substr(trimStart, trimEnd - trimStart + 1));
	}

	// Remove every whitespace character (joins all tokens into one run-on word).
	inline std::string stripAllWhitespaces(std::string text)
	{
		text.erase(std::remove_if(text.begin(), text.end(),
								  [](const unsigned char character)
								  { return std::isspace(character); }),
				   text.end());
		return text;
	}

	// Lowercase form with all whitespace removed (normalized command-type token).
	inline std::string normalizeCommandType(std::string text)
	{
		text = stripAllWhitespaces(text);
		std::transform(text.begin(), text.end(), text.begin(),
					   [](const unsigned char character)
					   { return static_cast<char>(std::tolower(character)); });
		return text;
	}

	// True when the line is empty or a '#' comment. Leading TABs are intentionally
	// not trimmed (matches the config parser's ignore rules).
	inline bool isBlankOrComment(const std::string_view line)
	{
		const std::string_view::size_type first = line.find_first_not_of(' ');
		return first == std::string_view::npos || line[first] == '#';
	}

	inline int getIndentLevel(const std::string_view line)
	{
		int indent = 0;
		for (const char character : line)
		{
			if (character == '\t')
				indent += 8;
			else if (character == ' ')
				++indent;
			else
				break;
		}
		return indent;
	}

	inline int getButtonNumber(const std::string_view configLine)
	{
		const std::string_view::size_type dashPosition = configLine.find('-');
		if (dashPosition == std::string_view::npos)
			return -1;

		try
		{
			return std::stoi(std::string(configLine.substr(0, dashPosition))) + 1;
		}
		catch (...)
		{
			return -1;
		}
	}

	// Everything before the first separator; the whole text when absent.
	inline std::string textBefore(const std::string_view text, const char separator)
	{
		const std::string_view::size_type separatorPos = text.find(separator);
		return std::string(separatorPos == std::string_view::npos ? text : text.substr(0, separatorPos));
	}

	// Everything after the first separator; the whole text when absent.
	inline std::string textAfter(const std::string_view text, const char separator)
	{
		const std::string_view::size_type separatorPos = text.find(separator);
		return std::string(separatorPos == std::string_view::npos ? text : text.substr(separatorPos + 1));
	}

	// Single-quote wrap for embedding inside a shell string; embedded ' → '\''.
	inline std::string shellQuote(const std::string_view text)
	{
		std::string quoted = "'";
		for (const char character : text)
		{
			if (character == '\'')
				quoted += "'\\''";
			else
				quoted += character;
		}
		quoted += '\'';
		return quoted;
	}

	// "$key" → value substitution pair.
	struct StringKeyAndValue
	{
		std::string key;
		std::string value;
	};

	// Substitute every "$key" with value; a preceding backslash skips the token
	// (backslash kept). Replaced text is not rescanned (no recursion).
	inline std::string replaceVariables(std::string text,
	                                    const std::initializer_list<StringKeyAndValue> &replacements)
	{
		for (const StringKeyAndValue &replacement : replacements)
		{
			const std::string token = "$" + replacement.key;
			std::size_t position = 0;
			while ((position = text.find(token, position)) != std::string::npos)
			{
				if (position > 0 && text[position - 1] == '\\')
				{
					position += token.size();
					continue;
				}
				text.replace(position, token.size(), replacement.value);
				position += replacement.value.size();
			}
		}
		return text;
	}

	// Single or double quote characters.
	inline bool isQuote(const unsigned char character)
	{
		return character == '\'' || character == '"';
	}

	// Shell metacharacters forbidden when unquoted (prevents shell injection).
	inline bool isShellMetacharacter(const unsigned char character)
	{
		return character == ';' || character == '|' || character == '&' ||
		       character == '<' || character == '>' || character == '`';
	}

	// Shell-like tokenizer (single/double quotes, backslash escaping).
	// Returns false on blank input, unclosed quotes, or unquoted shell
	// metacharacters (;|&<>`) — injection guard. On false callers must
	// discard the partially-filled arguments vector.
	inline bool parseCommandLine(const std::string_view text, std::vector<std::string> &arguments)
	{
		std::string argument;
		char activeQuote = 0;
		bool escapeNext = false;

		for (const unsigned char character : text)
		{
			if (escapeNext)
			{
				argument += static_cast<char>(character);
				escapeNext = false;
				continue;
			}

			if (character == '\\' && activeQuote != '\'')
			{
				escapeNext = true;
				continue;
			}

			if (isQuote(character))
			{
				if (character == activeQuote)
					activeQuote = 0;
				else if (activeQuote == 0)
					activeQuote = static_cast<char>(character);
				continue;
			}

			if (activeQuote == 0 && std::isspace(character))
			{
				if (!argument.empty())
				{
					arguments.emplace_back(argument);
					argument.clear();
				}
				continue;
			}

			if (activeQuote == 0 && isShellMetacharacter(character))
				return false;

			argument += static_cast<char>(character);
		}

		if (escapeNext || activeQuote != 0)
			return false;

		if (!argument.empty())
			arguments.emplace_back(argument);
		return !arguments.empty();
	}
}