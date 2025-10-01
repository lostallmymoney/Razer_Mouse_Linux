#include <X11/keysym.h>
#include <X11/XKBlib.h>
#include <X11/extensions/XTest.h>

#include <string.h>
#include <stdlib.h>

#define N_MODIFIER_INDEXES (Mod5MapIndex + 1)

struct FakeKey
{
	Display *xdpy;
	int min_keycode, max_keycode, n_keysyms_per_keycode, held_keycode, held_state_flags, alt_mod_index;
	KeySym *keysyms;
	KeyCode modifier_table[N_MODIFIER_INDEXES];

	~FakeKey()
	{
		XFree(keysyms);
	}
};

static int utf8_to_ucs4(const unsigned char *src_orig, unsigned int *dst, int len)
{
	const unsigned char *src = src_orig;
	unsigned char s;
	int extra;
	unsigned int result;

	if (len == 0)
		return 0;

	s = *src++;
	len--;

	if (!(s & 0x80))
	{
		result = s;
		extra = 0;
	}
	else if (!(s & 0x40))
	{
		return -1;
	}
	else if (!(s & 0x20))
	{
		result = s & 0x1f;
		extra = 1;
	}
	else if (!(s & 0x10))
	{
		result = s & 0xf;
		extra = 2;
	}
	else if (!(s & 0x08))
	{
		result = s & 0x07;
		extra = 3;
	}
	else if (!(s & 0x04))
	{
		result = s & 0x03;
		extra = 4;
	}
	else if (!(s & 0x02))
	{
		result = s & 0x01;
		extra = 5;
	}
	else
	{
		return -1;
	}
	if (extra > len)
		return -1;

	while (extra--)
	{
		result <<= 6;
		s = *src++;

		if ((s & 0xc0) != 0x80)
			return -1;

		result |= s & 0x3f;
	}
	*dst = result;
	return src - src_orig;
}


FakeKey *fakekey_init(Display *xdpy)
{
	FakeKey *fk = nullptr;
	int event, error, major, minor, mod_index, mod_key;
	XModifierKeymap *modifiers;
	KeyCode *kp;

	if (xdpy == NULL || !XTestQueryExtension(xdpy, &event, &error, &major, &minor))
		return nullptr;

	fk = new FakeKey{};

	fk->xdpy = xdpy;

	/* Find keycode limits */

	XDisplayKeycodes(fk->xdpy, &fk->min_keycode, &fk->max_keycode);

	/* Get the mapping */

	fk->keysyms = XGetKeyboardMapping(fk->xdpy, fk->min_keycode, fk->max_keycode - fk->min_keycode + 1, &fk->n_keysyms_per_keycode);

	modifiers = XGetModifierMapping(fk->xdpy);

	kp = modifiers->modifiermap;

	for (mod_index = 0; mod_index < 8; mod_index++)
	{
		fk->modifier_table[mod_index] = 0;

		for (mod_key = 0; mod_key < modifiers->max_keypermod; mod_key++)
		{
			int keycode = kp[mod_index * modifiers->max_keypermod + mod_key];

			if (keycode != 0)
			{
				fk->modifier_table[mod_index] = keycode;
				break;
			}
		}
	}

	if (modifiers)
		XFreeModifiermap(modifiers);

	return fk;
}

int fakekey_send_keyevent(FakeKey *fk, KeyCode keycode, Bool is_press, int flags)
{
	if (flags)
	{
		if (flags & LockMask)
			XTestFakeKeyEvent(fk->xdpy, fk->modifier_table[ShiftMapIndex],
							  is_press, CurrentTime);

		if (flags & ControlMask)
			XTestFakeKeyEvent(fk->xdpy, fk->modifier_table[ControlMapIndex],
							  is_press, CurrentTime);

		if (flags & Mod1Mask) // ALT MASK
			XTestFakeKeyEvent(fk->xdpy, fk->modifier_table[Mod1MapIndex],
							  is_press, CurrentTime);

		XSync(fk->xdpy, False);
	}

	XTestFakeKeyEvent(fk->xdpy, keycode, is_press, CurrentTime);

	XSync(fk->xdpy, False);

	return 1;
}

int fakekey_press_keysym(FakeKey *fk, KeySym keysym, int flags)
{
	static int modifiedkey;
	KeyCode code = 0;

	if ((code = XKeysymToKeycode(fk->xdpy, keysym)) != 0)
	{
		if (XkbKeycodeToKeysym(fk->xdpy, code, 0, 0) != keysym)
		{
			if (XkbKeycodeToKeysym(fk->xdpy, code, 0, 1) == keysym)
				flags |= LockMask;
			else
				code = 0;
		}
		else
		{
			flags &= ~LockMask;
		}
	}

	if (!code)
	{
		modifiedkey = (modifiedkey + 1) % 10;

		int index = (fk->max_keycode - fk->min_keycode - modifiedkey - 1) * fk->n_keysyms_per_keycode;

		fk->keysyms[index] = keysym;

		XChangeKeyboardMapping(fk->xdpy,
							   fk->min_keycode,
							   fk->n_keysyms_per_keycode,
							   fk->keysyms,
							   (fk->max_keycode - fk->min_keycode));

		XSync(fk->xdpy, False);

		code = fk->max_keycode - modifiedkey - 1;

		if (XkbKeycodeToKeysym(fk->xdpy, code, 0, 0) != keysym && XkbKeycodeToKeysym(fk->xdpy, code, 0, 1) == keysym)
		{
			flags |= LockMask;
		}
	}

	if (code != 0)
	{
		fakekey_send_keyevent(fk, code, True, flags);

		fk->held_state_flags = flags;
		fk->held_keycode = code;

		return 1;
	}

	fk->held_state_flags = 0;
	fk->held_keycode = 0;

	return 0;
}

int fakekey_press(FakeKey *fk, const unsigned char *utf8_char_in, int len_bytes, int flags)
{
	unsigned int ucs4_out;

	if (fk->held_keycode) /* key is already held down */
		return 0;

	if (len_bytes < 0)
	{
		len_bytes = strlen((const char *)utf8_char_in);
	}

	if (utf8_to_ucs4(utf8_char_in, &ucs4_out, len_bytes) < 1)
	{
		return 0;
	}

	if (ucs4_out > 0x00ff)				  /* < 0xff assume Latin-1 1:1 mapping */
		ucs4_out = ucs4_out | 0x01000000; /* This gives us the magic X keysym */

	return fakekey_press_keysym(fk, (KeySym)ucs4_out, flags);
}

void fakekey_release(FakeKey *fk)
{
	if (!fk->held_keycode)
		return;

	fakekey_send_keyevent(fk, fk->held_keycode, False, fk->held_state_flags);

	fk->held_state_flags = 0;
	fk->held_keycode = 0;
}
