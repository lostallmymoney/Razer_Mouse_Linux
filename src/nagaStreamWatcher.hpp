#pragma once

#include <atomic>
#include <chrono>
#include <cstring>
#include <mutex>
#include <string>
#include <sys/inotify.h>
#include <thread>
#include <unistd.h>

#include "nagaSettings.hpp"

// NagaStreamWatcher keeps the latest contents of one regularly-rewritten
// file cached in memory. A detached thread follows the file with inotify and
// re-reads it on every change, so changed() is a single atomic load on the
// hot path: no syscalls, no locks.
//
// The containing directory (not the file itself) is watched, so writers that
// replace the file via temp-file + rename never orphan the watch. The
// directory itself is resolved by a callback running inside the thread: it is
// asked until it returns a valid path, then never again. If the directory
// does not exist yet, the thread retries once a second, sleeping with zero
// CPU cost in between.
//
// changed() and renewBaseline() must be called from the same thread; the
// watcher thread only touches the cache under its own lock. The instance must
// live as long as the process: the watcher thread is detached, never joined.
class NagaStreamWatcher
{
public:
	NagaStreamWatcher(std::string (*directoryResolver)(), const char *watchedFileName)
		: directoryResolver(directoryResolver),
		  watchedFileName(watchedFileName)
	{
	}

	// Spawns the watcher thread on first call; later calls cost one atomic load.
	void startWatching()
	{
		std::call_once(watcherStarted, [this] { std::thread(&NagaStreamWatcher::watcherThread, this).detach(); });
	}

	// True when the file was rewritten since the last renewBaseline().
	bool changed() const
	{
		return cachedVersion.load(std::memory_order_acquire) != baselineVersion;
	}

	// A copy of the latest file contents.
	std::string cachedContents() const
	{
		std::lock_guard<std::mutex> guard(cacheMutex);
		return cachedFileContents;
	}

	// Treat the current contents as seen. The next changed() only fires on a
	// newer write.
	void renewBaseline()
	{
		baselineVersion = cachedVersion.load(std::memory_order_acquire);
	}

private:
	void refreshCache(const std::string &watchedFilePath)
	{
		nagaSettings::TextFile signalFile;
		nagaSettings::readTextFile(watchedFilePath.c_str(), false, signalFile);
		std::lock_guard<std::mutex> guard(cacheMutex);
		cachedFileContents = signalFile.contents;
		cachedVersion.fetch_add(1, std::memory_order_release);
	}

	void watcherThread()
	{
		// The directory may not exist yet (root daemon waiting for a login):
		// resolve it here and cache the valid result.
		std::string resolvedDirectory;
		while (resolvedDirectory.empty())
		{
			resolvedDirectory = directoryResolver();
			if (resolvedDirectory.empty())
				std::this_thread::sleep_for(std::chrono::seconds(1));
		}
		const std::string watchedFilePath = resolvedDirectory + "/" + watchedFileName;
		while (true)
		{
			const int watcherFd = inotify_init1(IN_CLOEXEC);
			bool watching = watcherFd >= 0 &&
							inotify_add_watch(watcherFd, resolvedDirectory.c_str(), IN_CLOSE_WRITE | IN_MOVED_TO) >= 0;
			if (watching)
			{
				refreshCache(watchedFilePath); // the writer may have written before we started
				char eventBuffer[4096];
				while (watching)
				{
					const ssize_t bytesRead = read(watcherFd, eventBuffer, sizeof(eventBuffer));
					if (bytesRead <= 0)
						continue;
					bool relevant = false;
					for (const char *eventPtr = eventBuffer; eventPtr < eventBuffer + bytesRead;)
					{
						const struct inotify_event *event = reinterpret_cast<const struct inotify_event *>(eventPtr);
						if ((event->mask & IN_IGNORED) != 0)
							watching = false; // directory went away: drop everything, re-establish below
						else if ((event->mask & IN_Q_OVERFLOW) != 0)
							relevant = true; // may have missed an event: re-read to be safe
						else if (event->len > 0 && (event->mask & (IN_CLOSE_WRITE | IN_MOVED_TO)) != 0 &&
								 std::strcmp(event->name, watchedFileName.c_str()) == 0)
							relevant = true;
						eventPtr += sizeof(struct inotify_event) + event->len;
					}
					if (relevant)
						refreshCache(watchedFilePath);
				}
			}
			if (watcherFd >= 0)
				close(watcherFd);
			// Directory missing (writer not installed yet) or watch lost:
			// wait a beat and try again. Zero CPU while waiting.
			std::this_thread::sleep_for(std::chrono::seconds(1));
		}
	}

	std::string (*directoryResolver)();
	const std::string watchedFileName;

	mutable std::mutex cacheMutex;
	std::string cachedFileContents; // guarded by cacheMutex
	std::atomic<unsigned long> cachedVersion{0};
	unsigned long baselineVersion = 0; // only touched by the calling thread
	std::once_flag watcherStarted;
};
