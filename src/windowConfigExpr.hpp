#pragma once

#include <cctype>
#include <string>

namespace windowConfigExpr
{
	// Wildcard matcher used for configWindowExpr patterns.
	// '*' matches any run of characters (including none),
	// '?' matches exactly one character, and matching is
	// case-insensitive. A pattern without wildcards is an
	// exact (case-insensitive) comparison against the active
	// window class.
	inline bool matches(const std::string &pattern, const std::string &text)
	{
		size_t patternIndex = 0, textIndex = 0;
		size_t lastStarPattern = std::string::npos, lastStarText = 0;

		while (textIndex < text.size())
		{
			if (patternIndex < pattern.size() && pattern[patternIndex] == '*')
			{
				lastStarPattern = patternIndex++;
				lastStarText = textIndex;
			}
			else if (patternIndex < pattern.size() &&
					 (pattern[patternIndex] == '?' ||
						 tolower(static_cast<unsigned char>(pattern[patternIndex])) == tolower(static_cast<unsigned char>(text[textIndex]))))
			{
				++patternIndex;
				++textIndex;
			}
			else if (lastStarPattern != std::string::npos)
			{
				patternIndex = lastStarPattern + 1;
				textIndex = ++lastStarText;
			}
			else
			{
				return false;
			}
		}

		while (patternIndex < pattern.size() && pattern[patternIndex] == '*')
			++patternIndex;

		return patternIndex == pattern.size();
	}
}