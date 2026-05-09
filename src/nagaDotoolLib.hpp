#pragma once

#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <stdexcept>
#include <string>

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

	inline void writeNagaDotoolCommand(const std::string &command)
	{
		std::lock_guard<std::mutex> lock(nagaDotoolPipeMutex);
		if (fwrite(command.data(), 1, command.size(), nagaDotoolPipe) != command.size() ||
			fputc('\n', nagaDotoolPipe) == EOF)
		{
			throw std::runtime_error("\033[91mError : Failed to write command to nagaDotoolc\033[0m");
		}

		if (fflush(nagaDotoolPipe) == EOF)
		{
			throw std::runtime_error("\033[91mError : Failed to flush nagaDotoolc\033[0m");
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
			throw std::runtime_error("\033[91mError : Failed to start nagaDotoolc\033[0m");
		}
		setvbuf(nagaDotoolPipe, nullptr, _IOLBF, 0);
		atexit(closeNagaDotoolPipe);
	}
}
