#include "net.h"
#include "canvas.h"

#include <stdlib.h>
#include <errno.h>
#include <stdio.h> //sprintf

unsigned int px_width = 1024;
unsigned int px_height = 1024;
unsigned int px_pixelcount = 0;
unsigned int px_clientcount = 0;

// User sessions
typedef struct PxSession {

} PxSession;

// Helper functions

static inline int fast_str_startswith(const char* prefix, const char* str) {
	char cp, cs;
	while ((cp = *prefix++) == (cs = *str++)) {
		if (cp == 0)
			return 1;
	}
	return !cp;
}

// Decimal string to unsigned int. This variant does NOT consume +, - or whitespace.
// If **endptr is not NULL, it will point to the first non-decimal character, which
// may be \0 at the end of the string.
static inline uint32_t fast_strtoul10(const char *str, const char **endptr) {
	uint32_t result = 0;
	unsigned char c;
	for (; (c = *str - '0') <= 9; str++)
		result = result * 10 + c;
	if (endptr)
		*endptr = str;
	return result;
}

// Same as fast_strtoul10, but for hex strings.
static inline uint32_t fast_strtoul16(const char *str, const char **endptr) {
	uint32_t result = 0;
	unsigned char c;
	while ((c = *str - '0') <= 9 // 0-9
			|| ((c -= 7) >= 10 && c <= 15) // A-F
			|| ((c -= 32) >= 10 && c <= 15)) { // a-f
		result = result * 16 + c;
		str++;
	}
	if (endptr)
		*endptr = str;
	return result;
}

// server callbacks
void px_on_connect(NetClient *client) {
	px_clientcount++;
}

void px_on_close(NetClient *client, int error) {
	px_clientcount--;
}

void px_on_read(NetClient *client, char *line) {
	if (fast_str_startswith("PX ", line)) {
		const char * ptr = line + 3;
		const char * endptr = ptr;
		errno = 0;

		uint32_t x = fast_strtoul10(ptr, &endptr);
		if (endptr == ptr) {
			net_err(client,
					"Invalid command (expected decimal as first parameter)");
			return;
		}
		if (*endptr == '\0') {
			net_err(client, "Invalid command (second parameter required)");
			return;
		}

		endptr++; // eat space (or whatever non-decimal is found here)

		uint32_t y = fast_strtoul10((ptr = endptr), &endptr);
		if (endptr == ptr) {
			net_err(client,
					"Invalid command (expected decimal as second parameter)");
			return;
		}

		// PX <x> <y> -> Get RGB color at position (x,y) or '0x000000' for out-of-range queries
		if (*endptr == '\0') {
			uint32_t c;
			canvas_get_px(x, y, &c);
			char str[64];
			sprintf(str, "PX %u %u %06X", x, y, (c >> 8));
			net_send(client, str);
			return;
		}

		endptr++; // eat space (or whatever non-decimal is found here)

		// PX <x> <y> BB|RRGGBB|RRGGBBAA
		uint32_t c = fast_strtoul16((ptr = endptr), &endptr);
		if (endptr == ptr) {
			net_err(client,
					"Third parameter missing or invalid (should be hex color)");
			return;
		}

		if (endptr - ptr == 6) {
			// RGB -> RGBA (most common)
			c = (c << 8) + 0xff;
		} else if (endptr - ptr == 8) {
			// done
		} else if (endptr - ptr == 2) {
			// WW -> RGBA
			c = (c << 24) + (c << 16) + (c << 8) + 0xff;
		} else {
			net_err(client,
					"Color hex code must be 2, 6 or 8 characters long (WW, RGB or RGBA)");
			return;
		}

		px_pixelcount++;
		canvas_set_px(x, y, c);

	} else if (fast_str_startswith("SIZE", line)) {

		char str[64];
		snprintf(str, 64, "SIZE %d %d", px_width, px_height);
		net_send(client, str);

	} else if (fast_str_startswith("STATS", line)) {

		char str[128];
		snprintf(str, 128, "STATS px:%u conn:%u", px_pixelcount,
				px_clientcount);
		net_send(client, str);

	} else if (fast_str_startswith("HELP", line)) {

		net_send(client,
				"\
PX x y: Get color at position (x,y)\n\
PX x y rrggbb(aa): Draw a pixel (with optional alpha channel)\n\
SIZE: Get canvas size\n\
STATS: Return statistics");

	} else {

		net_err(client, "Unknown command");

	}
}

void px_on_key(int key, int scancode, int mods) {

	printf("Key pressed: key:%d scancode:%d mods:%d\n", key, scancode, mods);

	if (key == 300) { // F11
		int display = canvas_get_display();
		if (display < 0)
			canvas_fullscreen(0);
		else
			canvas_fullscreen(-1);
	} else if (key == 301) { // F12
		canvas_fullscreen(canvas_get_display() + 1);
	} else if (key == 67) { // c
		canvas_fill(0x00000088);
	} else if (key == 81 || key == 256) { // q or ESC
		canvas_close();
	}
}

void px_on_resize() {
	canvas_get_size(&px_width, &px_height);
}

void px_on_window_close() {
	printf("Window closed\n");
	net_stop();
}

int main(int argc, char **argv) {
	canvas_setcb_key(&px_on_key);
	canvas_setcb_resize(&px_on_resize);

	canvas_start(1024, &px_on_window_close);

	net_start(1337, &px_on_connect, &px_on_read, &px_on_close);
	return 0;
}

