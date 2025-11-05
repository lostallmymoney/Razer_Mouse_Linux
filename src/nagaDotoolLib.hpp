#pragma once

#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <stdexcept>
#include <string_view>

namespace nagaDotool
{
	inline std::mutex nagaDotoolPipeMutex;
	inline FILE *nagaDotoolPipe = nullptr;

	inline void closeNagaDotoolPipe()
	{
		std::lock_guard<std::mutex> lock(nagaDotoolPipeMutex);
		if (nagaDotoolPipe)
		{
			pclose(nagaDotoolPipe);
			nagaDotoolPipe = nullptr;
		}
	}

	inline void writeNagaDotoolCommand(std::string_view command)
	{
		std::lock_guard<std::mutex> lock(nagaDotoolPipeMutex);
		if (fwrite(command.data(), 1, command.size(), nagaDotoolPipe) != command.size() ||
			fputc('\n', nagaDotoolPipe) == EOF)
		{
			throw std::runtime_error("Failed to write command to nagaDotoolc");
		}

		if (fflush(nagaDotoolPipe) == EOF)
		{
			throw std::runtime_error("Failed to flush nagaDotoolc");
		}
	}

	inline void initNagaDotoolPipe()
	{
		std::lock_guard<std::mutex> lock(nagaDotoolPipeMutex);
		if (nagaDotoolPipe)
		{
			return;
		}

		nagaDotoolPipe = popen("nagaDotoolc", "w");
		if (!nagaDotoolPipe)
		{
			throw std::runtime_error("Failed to start nagaDotoolc");
		}
		setvbuf(nagaDotoolPipe, nullptr, _IOLBF, 0);
		atexit(closeNagaDotoolPipe);
	}
}
