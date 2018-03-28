#include "net.h"
#include "canvas.h"

#include <stdlib.h>
#include <errno.h>
#include <stdio.h> //sprintf

int px_width = 1024;
int px_height = 1024;

const char * px_help_text =
		"\
PX x y: Get color at position (x,y)\n\
PX x y rrggbb(aa): Draw a pixel (with optional alpha channel)\n\
SIZE: Get canvas size";

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
		if (*endptr == 0) {
			char str[64];
			sprintf(str, "PX %u %u %x", x, y, 0xABCDEF);
			net_send(client, str);
			return;
		}
		unsigned int c = strtoul((ptr = endptr), &endptr, 16);
		if (endptr == ptr || errno) {
			net_err(client,
					"Third parameter missing or invalid (should be hex color)");
			return;
		}
		printf("%d %d %u=%x\n", x, y, c, c);
	} else if (util_str_starts_with("SIZE", line)) {
		unsigned int w, h;
		canvas_get_size(&w, &h);
		char str[64];
		sprintf(str, "SIZE %d %d", w, h);
		net_send(client, str);
	} else if (util_str_starts_with("HELP", line)) {
		net_send(client, px_help_text);
	}
}

void px_on_close(NetClient *client, int error) {
}

void px_on_key(int key, int scancode, int mods) {
	if (key == 300) { // F11
		int display = canvas_get_display();
		if(display<0)
			canvas_fullscreen(0);
		else
			canvas_fullscreen(-1);
	} else if (key == 301) { // F12
		canvas_fullscreen(canvas_get_display()+1);
	} else if (key == 81 || key == 256) { // q or ESC
		canvas_close();
	}
}

void px_on_resize() {}
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

