#ifndef CANVAS_H_
#define CANVAS_H_

// Open the canvas window and start the gui loop (in a separate thread)
void canvas_start(unsigned int texSize, void (*on_close)());

void canvas_setcb_key(void (*on_key)(int key, int scancode, int mods));
void canvas_setcb_resize(void (*on_resize)());

// Close the canvas window and free any resources and contexts
void canvas_close();

void canvas_fullscreen(int display);
int canvas_get_display();

void canvas_set_px(unsigned int x, unsigned int y, unsigned int rgba);
void canvas_get_px(unsigned int x, unsigned int y, unsigned int *rgba);

// get the current visible canvas size in pixel.
// The actual window might be bigger if scaling is enabled.
void canvas_get_size(unsigned int *width, unsigned int *height);


#endif /* CANVAS_H_ */
