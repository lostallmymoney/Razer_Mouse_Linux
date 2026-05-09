// This is lostallmymoney's remake of RaulPPelaez's original tool.
// RaulPPelaez, et. al wrote the original file.  As long as you retain this notice you
// can do whatever you want with this stuff.

#include "nagaCore.hpp"
#include "fakeKeysX11.hpp"
#include "getactivewindowX11.hpp"

using namespace std;

static mutex fakeKeyFollowUpsMutex;
string conf_file = string(getenv("HOME")) + "/.naga/keyMap.txt";

static map<const char *const, FakeKey *const> *const fakeKeyFollowUps = new map<const char *const, FakeKey *const>();

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
	FakeKey *const aKeyFaker = fakekey_init(XOpenDisplay(nullptr));
	const char *const keyCodeChar = &macroContent[0];
	fakekey_press(aKeyFaker, reinterpret_cast<const unsigned char *>(keyCodeChar), 8, 0);
	XFlush(aKeyFaker->xdpy);
	fakeKeyFollowUps->emplace(keyCodeChar, aKeyFaker);
}

static void specialReleaseNow(const string &macroContent)
{
	const char *const targetChar = &macroContent[0];
	for (map<const char *const, FakeKey *const>::iterator aKeyFollowUpPair = fakeKeyFollowUps->begin(); aKeyFollowUpPair != fakeKeyFollowUps->end(); ++aKeyFollowUpPair)
	{
		if (*aKeyFollowUpPair->first == *targetChar)
		{
			lock_guard<mutex> guard(fakeKeyFollowUpsMutex);
			FakeKey *const aKeyFaker = aKeyFollowUpPair->second;
			fakekey_release(aKeyFaker);
			XFlush(aKeyFaker->xdpy);
			XCloseDisplay(aKeyFaker->xdpy);
			fakeKeyFollowUps->erase(aKeyFollowUpPair);
			delete aKeyFaker;
			return;
		}
	}
	clog << "No candidate for key release\n";
}

void platformRunAndWrite(const string &macroContent)
{
	unique_ptr<FILE, int (*)(FILE *)> pipe(popen(macroContent.c_str(), "r"), &pclose);
	if (!pipe)
	{
		throw runtime_error("runAndWrite Failed !");
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
	NagaDaemon::emplaceConfigKey("keyclickrelease", NagaDaemon::OnKeyReleased, NagaDaemon::executeThreadNow, "xdotool key --window getactivewindow ");

	NagaDaemon::emplaceConfigKey("string", NagaDaemon::OnKeyPressed, writeStringNow);
	NagaDaemon::emplaceConfigKey("stringrelease", NagaDaemon::OnKeyReleased, writeStringNow);

	NagaDaemon::emplaceConfigKey("specialpressonpress", NagaDaemon::OnKeyPressed, specialPressNow);
	NagaDaemon::emplaceConfigKey("specialpressonrelease", NagaDaemon::OnKeyReleased, specialPressNow);
	NagaDaemon::emplaceConfigKey("specialreleaseonpress", NagaDaemon::OnKeyPressed, specialReleaseNow);
	NagaDaemon::emplaceConfigKey("specialreleaseonrelease", NagaDaemon::OnKeyReleased, specialReleaseNow);
}

int main(const int argc, const char *const argv[])
{
	return nagaMain(argc, argv);
}
