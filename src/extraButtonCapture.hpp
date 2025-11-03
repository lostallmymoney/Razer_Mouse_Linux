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

	static constexpr size_t bitsToLongs(size_t bits)
	{
		return (bits + (sizeof(unsigned long) * 8) - 1) / (sizeof(unsigned long) * 8);
	}

	static bool isBitSet(const unsigned long *bits, int bit, size_t bitsPerLong)
	{
		return (bits[bit / bitsPerLong] & (1UL << (bit % bitsPerLong))) != 0UL;
	}

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

		constexpr size_t bitsPerLong = sizeof(unsigned long) * 8;
		const size_t evBitsLen = bitsToLongs(EV_MAX + 1);
		unsigned long evBits[evBitsLen];
		std::memset(evBits, 0, sizeof(evBits));

		bool mirroredCapabilities = ioctl(sourceFd, EVIOCGBIT(0, sizeof(evBits)), evBits) != -1;
		if (mirroredCapabilities)
		{
			for (int type = 0; type <= EV_MAX; ++type)
			{
				if (!isBitSet(evBits, type, bitsPerLong))
				{
					continue;
				}
				if (ioctl(fd, UI_SET_EVBIT, type) == -1)
				{
					return fail("UI_SET_EVBIT");
				}

				if (type == EV_KEY)
				{
					const size_t keyBitsLen = bitsToLongs(KEY_MAX + 1);
					unsigned long keyBits[keyBitsLen];
					std::memset(keyBits, 0, sizeof(keyBits));
					if (ioctl(sourceFd, EVIOCGBIT(EV_KEY, sizeof(keyBits)), keyBits) != -1)
					{
						for (int code = 0; code <= KEY_MAX; ++code)
						{
							if (isBitSet(keyBits, code, bitsPerLong) && ioctl(fd, UI_SET_KEYBIT, code) == -1)
							{
								return fail("UI_SET_KEYBIT");
							}
						}
					}
				}
				else if (type == EV_REL)
				{
					const size_t relBitsLen = bitsToLongs(REL_MAX + 1);
					unsigned long relBits[relBitsLen];
					std::memset(relBits, 0, sizeof(relBits));
					if (ioctl(sourceFd, EVIOCGBIT(EV_REL, sizeof(relBits)), relBits) != -1)
					{
						for (int code = 0; code <= REL_MAX; ++code)
						{
							if (isBitSet(relBits, code, bitsPerLong) && ioctl(fd, UI_SET_RELBIT, code) == -1)
							{
								return fail("UI_SET_RELBIT");
							}
						}
					}
				}
				else if (type == EV_MSC)
				{
					const size_t mscBitsLen = bitsToLongs(MSC_MAX + 1);
					unsigned long mscBits[mscBitsLen];
					std::memset(mscBits, 0, sizeof(mscBits));
					if (ioctl(sourceFd, EVIOCGBIT(EV_MSC, sizeof(mscBits)), mscBits) != -1)
					{
						for (int code = 0; code <= MSC_MAX; ++code)
						{
							if (isBitSet(mscBits, code, bitsPerLong) && ioctl(fd, UI_SET_MSCBIT, code) == -1)
							{
								return fail("UI_SET_MSCBIT");
							}
						}
					}
				}
			}
		}
		else
		{
			if (ioctl(fd, UI_SET_EVBIT, EV_KEY) == -1 || ioctl(fd, UI_SET_EVBIT, EV_REL) == -1 || ioctl(fd, UI_SET_EVBIT, EV_SYN) == -1)
			{
				return fail("UI_SET_EVBIT");
			}
			if (ioctl(fd, UI_SET_EVBIT, EV_MSC) == 0 && ioctl(fd, UI_SET_MSCBIT, MSC_SCAN) == -1)
			{
				return fail("UI_SET_MSCBIT");
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
