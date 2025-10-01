#pragma once

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <linux/input.h>
#include <linux/uinput.h>
#include <cstdio>
#include <sys/ioctl.h>
#include <unistd.h>

class UInputForwarder
{
private:
	int fd = -1;
	mutable bool warned = false;

	bool fail(const char *step)
	{
		std::clog << "[naga] uinput " << step << " failed: " << std::strerror(errno) << std::endl;
		if (fd >= 0)
		{
			ioctl(fd, UI_DEV_DESTROY);
			close(fd);
			fd = -1;
		}
		return false;
	}

public:
	~UInputForwarder()
	{
		if (fd >= 0)
		{
			ioctl(fd, UI_DEV_DESTROY);
			close(fd);
		}
	}

	bool init(int sourceFd)
	{
		fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
		if (fd < 0)
		{
			std::clog << "[naga] unable to open /dev/uinput: " << std::strerror(errno) << std::endl;
			return false;
		}

		warned = false;

		if (ioctl(fd, UI_SET_EVBIT, EV_KEY) == -1 || ioctl(fd, UI_SET_EVBIT, EV_REL) == -1 || ioctl(fd, UI_SET_EVBIT, EV_SYN) == -1)
		{
			return fail("UI_SET_EVBIT");
		}

		for (int code : {BTN_LEFT, BTN_RIGHT, BTN_MIDDLE, BTN_SIDE, BTN_EXTRA, BTN_FORWARD, BTN_BACK, BTN_TASK})
		{
			if (ioctl(fd, UI_SET_KEYBIT, code) == -1)
			{
				return fail("UI_SET_KEYBIT");
			}
		}

		for (int code : {REL_X, REL_Y, REL_WHEEL, REL_HWHEEL})
		{
			if (ioctl(fd, UI_SET_RELBIT, code) == -1)
			{
				return fail("UI_SET_RELBIT");
			}
		}

		input_id id{};
		if (ioctl(sourceFd, EVIOCGID, &id) == -1)
		{
			id.bustype = BUS_USB;
			id.vendor = 0x0000;
			id.product = 0x0000;
			id.version = 0;
		}

		uinput_user_dev dev{};
		std::snprintf(dev.name, UINPUT_MAX_NAME_SIZE, "naga-virtual-pointer");
		dev.id = id;

		if (write(fd, &dev, sizeof(dev)) != sizeof(dev))
		{
			return fail("write device");
		}

		if (ioctl(fd, UI_DEV_CREATE) == -1)
		{
			return fail("UI_DEV_CREATE");
		}

		return true;
	}

	void forward(const input_event &ev) const
	{
		if (fd < 0)
		{
			return;
		}
		if (write(fd, &ev, sizeof(ev)) != sizeof(ev) && !warned)
		{
			warned = true;
			std::clog << "[naga] failed to forward pointer event: " << std::strerror(errno) << std::endl;
		}
	}
};
