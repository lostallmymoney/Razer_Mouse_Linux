#pragma once

#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <stdexcept>
#include <string_view>

namespace nagaDotool
{
	inline std::mutex dotoolPipeMutex;
	inline FILE *dotoolPipe = nullptr;

	inline void closeDotoolPipe()
	{
		std::lock_guard<std::mutex> lock(dotoolPipeMutex);
		if (dotoolPipe)
		{
			pclose(dotoolPipe);
			dotoolPipe = nullptr;
		}
	}

	inline void writeDotoolCommand(std::string_view command)
	{
		std::lock_guard<std::mutex> lock(dotoolPipeMutex);
		if (fwrite(command.data(), 1, command.size(), dotoolPipe) != command.size() ||
			fputc('\n', dotoolPipe) == EOF)
		{
			throw std::runtime_error("Failed to write command to nagaDotoolc");
		}

		if (fflush(dotoolPipe) == EOF)
		{
			throw std::runtime_error("Failed to flush nagaDotoolc");
		}
	}

	inline void initDotoolPipe()
	{
		std::lock_guard<std::mutex> lock(dotoolPipeMutex);
		if (dotoolPipe)
		{
			return;
		}

		dotoolPipe = popen("nagaDotoolc", "w");
		if (!dotoolPipe)
		{
			throw std::runtime_error("Failed to start nagaDotoolc");
		}
		setvbuf(dotoolPipe, nullptr, _IOLBF, 0);
		atexit(closeDotoolPipe);
	}
}
