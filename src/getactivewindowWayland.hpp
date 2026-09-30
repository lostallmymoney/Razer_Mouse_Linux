#pragma once

#include <dirent.h>
#include <string>
#include <unistd.h>

#include "nagaStreamWatcher.hpp"

namespace
{
	constexpr const char *FOCUS_CLASS_SIGNAL_NAME = "focusClassFetcher-pipe";

	// Root has no session of its own: find the human login's runtime dir by
	// scanning /run/user. UIDs below 1000 are system users (gdm, ...), so the
	// smallest UID >= 1000 wins. Empty while nobody is logged in.
	std::string loginUserRuntimeDir()
	{
		std::string loginUid;
		DIR *runUserDir = opendir("/run/user");
		if (runUserDir != nullptr)
		{
			while (const dirent *entry = readdir(runUserDir))
			{
				const char *name = entry->d_name;
				bool numeric = name[0] != '\0';
				for (const char *digit = name; numeric && *digit != '\0'; ++digit)
					numeric = *digit >= '0' && *digit <= '9';
				if (numeric)
				{
					const unsigned long uid = std::stoul(name);
					if (uid >= 1000 && (loginUid.empty() || uid < std::stoul(loginUid)))
						loginUid = name;
				}
			}
			closedir(runUserDir);
		}
		return loginUid.empty() ? std::string() : "/run/user/" + loginUid + "/focusClassFetcher";
	}

	// Resolves the signal directory for the watcher thread: the thread calls this
	// until it returns a valid path, then caches it. Empty while nobody is logged
	// in (root service case). A root service has no session of its own and must
	// never trust $XDG_RUNTIME_DIR (systemd doesn't set it for plain system
	// services, but a stray Environment= or sudo -E could plant a wrong one).
	std::string resolveFocusClassSignalDir()
	{
		if (getuid() != 0)
		{
			const char *runtimeDir = getenv("XDG_RUNTIME_DIR");
			if (runtimeDir != nullptr && runtimeDir[0] != '\0')
				return std::string(runtimeDir) + "/focusClassFetcher";
			return "/run/user/" + std::to_string(getuid()) + "/focusClassFetcher";
		}
		return loginUserRuntimeDir();
	}

	// Built once; the watcher thread resolves the directory itself and waits for
	// it to exist. Single-threaded by contract (checkForWindowConfig holds
	// configSwitcherMutex), so a plain pointer is enough. Outlives the process.
	NagaStreamWatcher *windowClassStreamWatcher = nullptr;

	NagaStreamWatcher *windowClassWatcher()
	{
		return windowClassStreamWatcher;
	}
}

inline bool windowClassChanged()
{
	return windowClassStreamWatcher->changed();
}

inline std::string getActiveWindowTitle()
{
	return windowClassStreamWatcher->cachedContents();
}

inline void newWindowClassBaseline()
{
	windowClassStreamWatcher->renewBaseline();
}

// First resolution attempt at daemon startup; the poll loop keeps retrying
// until the directory exists, then the watcher thread runs for the life of
// the process.
inline void initWindowClassWatcher()
{
	if (windowClassStreamWatcher == nullptr)
	{
		windowClassStreamWatcher = new NagaStreamWatcher(resolveFocusClassSignalDir, FOCUS_CLASS_SIGNAL_NAME);
		windowClassStreamWatcher->startWatching();
	}
}
