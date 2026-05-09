// This is lostallmymoney's remake of RaulPPelaez's original tool.
// RaulPPelaez, et. al wrote the original file.  As long as you retain this notice you
// can do whatever you want with this stuff.

#include "nagaCore.hpp"
#include "nagaDotoolLib.hpp"
#include "getactivewindowWayland.hpp"

using namespace std;
using nagaDotool::initNagaDotoolPipe;
using nagaDotool::writeNagaDotoolCommand;

string conf_file = string(getenv("HOME")) + "/.naga/keyMapWayland.txt";

void platformRunAndWrite(const string &macroContent)
{
	unique_ptr<FILE, int (*)(FILE *)> pipe(popen(macroContent.c_str(), "r"), &pclose);
	if (!pipe)
	{
		throw runtime_error("runAndWrite execution Failed !");
	}

	constexpr size_t DotoolCommandSizeLimit = 65536;
	string currentLine;
	FILE *fp = pipe.get();
	size_t counter = 0;
	for (int ch = fgetc(fp); ch != EOF; ch = fgetc(fp))
	{
		if (ch == '\n')
		{
			if (!currentLine.empty())
			{
				writeNagaDotoolCommand("type " + currentLine);
				counter = 0;
				currentLine.clear();
			}
			writeNagaDotoolCommand("key enter");
			writeNagaDotoolCommand("key home");
		}
		else
		{
			currentLine.push_back(static_cast<char>(ch));
			counter++;
			if (counter >= DotoolCommandSizeLimit)
			{
				writeNagaDotoolCommand("type " + currentLine);
				counter = 0;
				currentLine.clear();
			}
		}
	}

	if (!currentLine.empty())
	{
		writeNagaDotoolCommand("type " + currentLine);
	}
}

void initAndRegisterPlatformCommands()
{
	initNagaDotoolPipe();

	NagaDaemon::emplaceConfigKey("click", NagaDaemon::OnKeyPressed, writeNagaDotoolCommand, "click ");
	NagaDaemon::emplaceConfigKey("clickrelease", NagaDaemon::OnKeyReleased, writeNagaDotoolCommand, "click ");
	NagaDaemon::emplaceConfigKey("keypressonpress", NagaDaemon::OnKeyPressed, writeNagaDotoolCommand, "keydown ");
	NagaDaemon::emplaceConfigKey("keypressonrelease", NagaDaemon::OnKeyReleased, writeNagaDotoolCommand, "keydown ");
	NagaDaemon::emplaceConfigKey("keyreleaseonpress", NagaDaemon::OnKeyPressed, writeNagaDotoolCommand, "keyup ");
	NagaDaemon::emplaceConfigKey("keyreleaseonrelease", NagaDaemon::OnKeyReleased, writeNagaDotoolCommand, "keyup ");
	NagaDaemon::emplaceConfigKey("keyclick", NagaDaemon::OnKeyPressed, writeNagaDotoolCommand, "key ");
	NagaDaemon::emplaceConfigKey("keyclickrelease", NagaDaemon::OnKeyReleased, writeNagaDotoolCommand, "key ");
	NagaDaemon::emplaceConfigKey("string", NagaDaemon::OnKeyPressed, writeNagaDotoolCommand, "type ");
	NagaDaemon::emplaceConfigKey("stringrelease", NagaDaemon::OnKeyReleased, writeNagaDotoolCommand, "type ");
	NagaDaemon::emplaceConfigKey("dotool", NagaDaemon::OnKeyPressed, writeNagaDotoolCommand);
	NagaDaemon::emplaceConfigKey("dotoolrelease", NagaDaemon::OnKeyReleased, writeNagaDotoolCommand);
}

int main(const int argc, const char *const argv[])
{
	return nagaMain(argc, argv);
}
