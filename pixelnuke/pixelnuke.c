#include "net.h"
#include "canvas.h"

#include <stdlib.h>
#include <errno.h>
#include <stdio.h> //sprintf

unsigned int px_width = 1024;
unsigned int px_height = 1024;

// Helper functions

static int util_str_starts_with(const char* prefix, const char* str) {
	char cp, cs;
	while ((cp = *prefix++) == (cs = *str++)) {
		if (cp == 0)
			return 1;
	}
	return !cp;
}

// server callbacks
void px_on_connect(NetClient *client) {

}

void px_on_read(NetClient *client, char *line) {
	if (util_str_starts_with("PX ", line)) {
		const char * ptr = line + 3;
		char * endptr = (char*) ptr;
		errno = 0;

		unsigned int x = strtoul(ptr, &endptr, 10);
		if (endptr == ptr || errno) {
			net_err(client,
					"First parameter missing or invalid (should be decimal)");
			return;
		}

		unsigned int y = strtoul((ptr = endptr), &endptr, 10);
		if (endptr == ptr || errno) {
			net_err(client,
					"Second parameter missing or invalid (should be decimal)");
			return;
		}

		// PX <x> <y> -> Get RGB color at position (x,y) or '0x000000' for out-of-range queries
		if (*endptr == 0) {
			uint32_t c;
			canvas_get_px(x, y, &c);
			char str[64];
			sprintf(str, "PX %u %u %06X", x, y, (c >> 8));
			net_send(client, str);
			return;
		}

		// PX <x> <y> BB|RRGGBB|RRGGBBAA
		unsigned long int c = strtoul((ptr = endptr), &endptr, 16);
		if (endptr == ptr || errno) {
			net_err(client,
					"Third parameter missing or invalid (should be hex color)");
			return;
		}

		if (endptr - 1 - 6 == ptr) {
			// RGB -> RGBA
			c = (c << 8) + 0xff;
		} else if (endptr - 1 - 8 == ptr) {
			// done
		} else if (endptr - 1 - 2 == ptr) {
			// WW -> RGBA
			c = (c << 24) + (c << 16) + (c << 8) + 0xff;
		} else {
			net_err(client,
					"Color hex code must be 2, 6 or 8 characters long (WW, RGB or RGBA)");
			return;
		}

		canvas_set_px(x, y, c);

	} else if (util_str_starts_with("SIZE", line)) {

		char str[64];
		sprintf(str, "SIZE %d %d", px_width, px_height);
		net_send(client, str);

	} else if (util_str_starts_with("HELP", line)) {

		net_send(client,
"\
PX x y: Get color at position (x,y)\n\
PX x y rrggbb(aa): Draw a pixel (with optional alpha channel)\n\
SIZE: Get canvas size");

	} else {

		net_err(client, "Unknown command");

	}
}

void px_on_close(NetClient *client, int error) {
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
	canvas_start(1024, &px_on_window_close);
	canvas_setcb_key(&px_on_key);
	canvas_setcb_resize(&px_on_resize);
	net_start(1337, &px_on_connect, &px_on_read, &px_on_close);
	return 0;
}

