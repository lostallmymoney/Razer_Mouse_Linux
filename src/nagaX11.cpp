// This is lostallmymoney's remake of RaulPPelaez's original tool.
// RaulPPelaez, et. al wrote the original file.  As long as you retain this notice you
// can do whatever you want with this stuff.

#include "nagaCore.hpp"
#include "fakeKeysX11.hpp"
#include "getactivewindowX11.hpp"

using namespace std;

static mutex fakeKeyFollowUpsMutex;
string conf_file = string(getenv("HOME")) + "/.naga/keyMapX11.txt";

static map<string, FakeKey *> fakeKeyFollowUps;

static void writeStringNow(const string &macroContent)
{
	lock_guard<mutex> guard(fakeKeyFollowUpsMutex);
	FakeKey *const aKeyFaker = fakekey_init(XOpenDisplay(nullptr));
	for (const char &c : macroContent)
	{
		if (c == '\n')
		{
			fakekey_press_keysym(aKeyFaker, XK_Return, 0);
		}
		else
		{
			fakekey_press(aKeyFaker, reinterpret_cast<const unsigned char *>(&c), 8, 0);
		}

		fakekey_release(aKeyFaker);
	}
	XFlush(aKeyFaker->xdpy);
	XCloseDisplay(aKeyFaker->xdpy);
	delete aKeyFaker;
}

static void specialPressNow(const string &macroContent)
{
	lock_guard<mutex> guard(fakeKeyFollowUpsMutex);
	FakeKey *const keyFaker = fakekey_init(XOpenDisplay(nullptr));
	if (keyFaker == nullptr)
	{
		clog << "\033[91mError : Could not open display for special key press\033[0m\n";
		return;
	}
	fakekey_press(keyFaker, reinterpret_cast<const unsigned char *>(macroContent.c_str()), 8, 0);

	XFlush(keyFaker->xdpy);
	fakeKeyFollowUps.emplace(macroContent, keyFaker);
}

static void specialReleaseNow(const string &macroContent)
{
	lock_guard<mutex> guard(fakeKeyFollowUpsMutex);
	map<string, FakeKey *>::iterator followUp = fakeKeyFollowUps.find(macroContent);
	if (followUp == fakeKeyFollowUps.end())
	{
		clog << "\033[93mWarning : No candidate for key release\033[0m\n";
		return;
	}
	FakeKey *const keyFaker = followUp->second;
	fakeKeyFollowUps.erase(followUp);
	fakekey_release(keyFaker);
	XFlush(keyFaker->xdpy);
	XCloseDisplay(keyFaker->xdpy);
	delete keyFaker;
}

void platformRunAndWrite(const string &macroContent)
{
	unique_ptr<FILE, int (*)(FILE *)> pipe(popen(macroContent.c_str(), "r"), &pclose);
	if (!pipe)
	{
		clog << "\033[91mError : runAndWrite failed to start process\033[0m\n";
		return;
	}

	constexpr size_t BufferSize = 1024;
	char buffer[BufferSize];
	size_t bytesRead;
	string chunk;
	chunk.reserve(BufferSize);
	while ((bytesRead = fread(buffer, 1, BufferSize, pipe.get())) > 0)
	{
		chunk.assign(buffer, bytesRead);
		writeStringNow(chunk);
	}
}

void initAndRegisterPlatformCommands()
{
	std::ignore = system("/usr/local/bin/Naga_Linux/nagaXinputStart.sh");

	NagaDaemon::emplaceConfigKey("keypressonpress", NagaDaemon::OnKeyPressed, NagaDaemon::executeThreadNow, "xdotool keydown --window getactivewindow ");
	NagaDaemon::emplaceConfigKey("keypressonrelease", NagaDaemon::OnKeyReleased, NagaDaemon::executeThreadNow, "xdotool keydown --window getactivewindow ");
	NagaDaemon::emplaceConfigKey("keyreleaseonpress", NagaDaemon::OnKeyPressed, NagaDaemon::executeThreadNow, "xdotool keyup --window getactivewindow ");
	NagaDaemon::emplaceConfigKey("keyreleaseonrelease", NagaDaemon::OnKeyReleased, NagaDaemon::executeThreadNow, "xdotool keyup --window getactivewindow ");
	NagaDaemon::emplaceConfigKey("keyclick", NagaDaemon::OnKeyPressed, NagaDaemon::executeThreadNow, "xdotool key --window getactivewindow ");
	NagaDaemon::emplaceConfigKey("keyclickonrelease", NagaDaemon::OnKeyReleased, NagaDaemon::executeThreadNow, "xdotool key --window getactivewindow ");
	NagaDaemon::emplaceConfigKey("string", NagaDaemon::OnKeyPressed, writeStringNow);
	NagaDaemon::emplaceConfigKey("stringonrelease", NagaDaemon::OnKeyReleased, writeStringNow);
	NagaDaemon::emplaceConfigKey("specialpressonpress", NagaDaemon::OnKeyPressed, specialPressNow);
	NagaDaemon::emplaceConfigKey("specialpressonrelease", NagaDaemon::OnKeyReleased, specialPressNow);
	NagaDaemon::emplaceConfigKey("specialreleaseonpress", NagaDaemon::OnKeyPressed, specialReleaseNow);
	NagaDaemon::emplaceConfigKey("specialreleaseonrelease", NagaDaemon::OnKeyReleased, specialReleaseNow);
	// Add more commands here if needed. emplaceConfigKey parameters are : command name, isOnKeyPressed, function pointer, optional prefix, optional suffix,
	// and will pass a string to the function pointer in the form of prefix + command content + suffix.
}

// X11 ONLY COMBO-COMMANDS
NagaDaemon::ParsedCommandList NagaDaemon::platformComboKeyParser(const std::string &commandType, const std::string &commandContent)
{
	NagaDaemon::ParsedCommandList results;
	if (commandType == "specialkey")
	{
		NagaDaemon::emplaceMacroEvent(results, "specialpressonpress", commandContent);
		NagaDaemon::emplaceMacroEvent(results, "specialreleaseonrelease", commandContent);
	}
	// Fit additional combo-commands here..
	return results;
}

int main(const int argc, const char *const argv[])
{
	return nagaMain(argc, argv);
}
