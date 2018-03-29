#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <unistd.h> // usleep
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h> //memcpy

#include "canvas.h"

typedef struct CanvasLayer {
	GLuint size;
	GLenum format;
	GLuint tex;
	GLuint pbo1;
	GLuint pbo2;
	GLubyte *data;
	size_t mem;
} CanvasLayer;

// Global state

static int canvas_display = -1;
static unsigned int canvas_tex_size = 1024;
static GLFWwindow* canvas_win;
static CanvasLayer *canvas_base;
static CanvasLayer *canvas_overlay;

pthread_t canvas_thread;

void glfw_error_callback(int error, const char* description) {
	printf("GLFW Error: %d %s", error, description);
}

// User callbacks

void (*canvas_on_close_cb)();
void (*canvas_on_resize_cb)();
void (*canvas_on_key_cb)(int, int, int);

static int canvas_do_layout = 0;

static CanvasLayer* canvas_layer_alloc(int size, int alpha) {
	CanvasLayer * layer = malloc(sizeof(CanvasLayer));
	layer->size = size;
	layer->format = alpha ? GL_RGBA : GL_RGB;
	layer->mem = size * size * (alpha ? 4 : 3);
	layer->data = malloc(sizeof(GLubyte) * layer->mem);
	memset(layer->data, 0, layer->mem);
	return layer;
}

static void canvas_layer_bind(CanvasLayer* layer) {

	// Create texture object
	glGenTextures(1, &(layer->tex));
	glBindTexture( GL_TEXTURE_2D, layer->tex);
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glBindTexture( GL_TEXTURE_2D, 0);

	// Create two PBOs
	glGenBuffers(1, &(layer->pbo1));
	glBindBuffer( GL_PIXEL_UNPACK_BUFFER, layer->pbo1);
	glBufferData( GL_PIXEL_UNPACK_BUFFER, layer->mem, NULL, GL_STREAM_DRAW);
	glGenBuffers(1, &(layer->pbo2));
	glBindBuffer( GL_PIXEL_UNPACK_BUFFER, layer->pbo2);
	glBufferData( GL_PIXEL_UNPACK_BUFFER, layer->mem, NULL, GL_STREAM_DRAW);
	glBindBuffer( GL_PIXEL_UNPACK_BUFFER, 0);
}


static void canvas_layer_unbind(CanvasLayer * layer) {
	if (layer->tex) {
		glDeleteTextures(1, &(layer->tex));
		glDeleteBuffers(1, &(layer->pbo1));
		glDeleteBuffers(1, &(layer->pbo2));
	}
}

static void canvas_layer_free(CanvasLayer * layer) {
	canvas_layer_unbind(layer);
	free(layer->data);
	free(layer);
}

static void canvas_on_resize(GLFWwindow* window, int w, int h);
static void canvas_on_key(GLFWwindow* window, int key, int scancode, int action,
		int mods);

static void canvas_on_key(GLFWwindow* window, int key, int scancode, int action,
		int mods) {
	if (action == GLFW_PRESS && canvas_on_key_cb)
		(*canvas_on_key_cb)(key, scancode, mods);
}

static void canvas_on_resize(GLFWwindow* window, int w, int h) {
	if(canvas_on_resize_cb)
		(*canvas_on_resize_cb)();
}

static void canvas_window_setup() {

	if(canvas_win) {
		glfwDestroyWindow(canvas_win);
	}

	glfwWindowHint(GLFW_DOUBLEBUFFER, 1);
	if (canvas_display >= 0) {
		int mcount;
		GLFWmonitor** monitors = glfwGetMonitors(&mcount);
		canvas_display %= mcount;
		GLFWmonitor* monitor = monitors[canvas_display];
		const GLFWvidmode* mode = glfwGetVideoMode(monitor);
		glfwWindowHint(GLFW_RED_BITS, mode->redBits);
		glfwWindowHint(GLFW_GREEN_BITS, mode->greenBits);
		glfwWindowHint(GLFW_BLUE_BITS, mode->blueBits);
		glfwWindowHint(GLFW_REFRESH_RATE, mode->refreshRate);
		canvas_win = glfwCreateWindow(mode->width, mode->height, "Pixelflut", monitor, NULL);
	} else {
		canvas_win = glfwCreateWindow(800, 600, "Pixelflut", NULL, NULL);
	}

	if (!canvas_win) {
		printf("Could not create OpenGL context and/or window");
		return;
	}

	glfwMakeContextCurrent(canvas_win);

	// TODO: Move GL stuff to better place
	//glShadeModel(GL_FLAT);            // shading mathod: GL_SMOOTH or GL_FLAT
	glPixelStorei(GL_UNPACK_ALIGNMENT, 4); // 4-byte pixel alignment
	//glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
	//glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
	//glHint(GL_POLYGON_SMOOTH_HINT, GL_NICEST);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_LIGHTING);
	glDisable(GL_CULL_FACE);
	glEnable(GL_TEXTURE_2D);

	//glfwSetWindowUserPointer(canvas_win, (void*) this);
	glfwSwapInterval(1);
	glfwSetKeyCallback(canvas_win, &canvas_on_key);
	glfwSetFramebufferSizeCallback(canvas_win, &canvas_on_resize);

	canvas_do_layout = 0;
}

static void canvas_draw_layer(CanvasLayer * layer) {
	if (!layer || !layer->data)
		return;

	GLuint pboNext = layer->pbo1;
	GLuint pboIndex = layer->pbo2;
	layer->pbo1 = pboIndex;
	layer->pbo2 = pboNext;

	// Switch PBOs on each call. One is updated, one is drawn.
	// Update texture from first PBO
	glBindBuffer( GL_PIXEL_UNPACK_BUFFER, pboIndex);
	glBindTexture( GL_TEXTURE_2D, layer->tex);
	glTexImage2D( GL_TEXTURE_2D, 0, layer->format, layer->size, layer->size, 0,
			layer->format, GL_UNSIGNED_BYTE, 0);
	glBindTexture( GL_TEXTURE_2D, 0);

	// Update second PBO with new pixel data
	glBindBuffer( GL_PIXEL_UNPACK_BUFFER, pboNext);
	GLubyte *ptr = (GLubyte*) glMapBuffer( GL_PIXEL_UNPACK_BUFFER,
			GL_WRITE_ONLY);
	memcpy(ptr, layer->data, layer->mem);
	glUnmapBuffer( GL_PIXEL_UNPACK_BUFFER);
	glBindBuffer( GL_PIXEL_UNPACK_BUFFER, 0);

	//// Actually draw stuff. The texture should be updated in the meantime.

	if (layer->format == GL_RGBA) {
		glEnable( GL_BLEND);
		glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	} else {
		glDisable( GL_BLEND);
	}

	glPushMatrix();
	glBindTexture( GL_TEXTURE_2D, layer->tex);
	glBegin( GL_QUADS);
	glTexCoord2f(0, 0);
	glVertex3f(0.0f, 0.0f, 0.0f);
	glTexCoord2f(0, 1);
	glVertex3f(0.0f, layer->size, 0.0f);
	glTexCoord2f(1, 1);
	glVertex3f(layer->size, layer->size, 0.0f);
	glTexCoord2f(1, 0);
	glVertex3f(layer->size, 0.0f, 0.0f);
	glEnd();
	glBindTexture( GL_TEXTURE_2D, 0);
	glPopMatrix();
}

static void* canvas_render_loop(void * arg) {

	glfwSetErrorCallback(glfw_error_callback);
	if (!glfwInit()) {
		puts("GLFW initialization failed");
		if(canvas_on_close_cb)
			(*canvas_on_close_cb)();
		glfwTerminate();
		return NULL;
	}

	canvas_window_setup();

	int err = glewInit();
	if (err != GLEW_OK) {
		puts("GLEW initialization failed");
		printf("Error: %s\n", glewGetErrorString(err));
		if(canvas_on_close_cb)
			(*canvas_on_close_cb)();
		return NULL;
	}

	canvas_layer_bind(canvas_base);
	canvas_layer_bind(canvas_overlay);

	double last_frame = glfwGetTime();

	while ("pixels are coming") {

		if (canvas_do_layout) {
			canvas_layer_unbind(canvas_base);
			canvas_layer_unbind(canvas_overlay);
			canvas_window_setup();
			canvas_layer_bind(canvas_base);
			canvas_layer_bind(canvas_overlay);
		}

		if (glfwWindowShouldClose(canvas_win))
			break;

		int w, h;
		glfwGetFramebufferSize(canvas_win, &w, &h);
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glOrtho(0, w, h, 0, -1, 1);
		glViewport(0, 0, (GLsizei) w, (GLsizei) h);
		glClearColor(0, 0, 0, 1);
		glClear(GL_COLOR_BUFFER_BIT);
		glPushMatrix();

		GLuint texSize = canvas_base->size;
		if(w > texSize || h > texSize) {
		    float scale = ((float) (w>h?w:h)) / (float) texSize;
		    glScalef(scale, scale, 1);
		}

		canvas_draw_layer(canvas_base);
		// TODO: Overlay is not used yet
		//canvas_draw_layer(canvas_overlay);

		glPopMatrix();
		glfwPollEvents();
		glfwSwapBuffers(canvas_win);

		double now = glfwGetTime();
		double dt = now - last_frame;
		last_frame = now;
		double sleep = 1.0 / 30 - dt;
		if (sleep > 0) {
			usleep(sleep * 1000000);
		}
	}

	if(canvas_on_close_cb)
		(*canvas_on_close_cb)();

	glfwTerminate();
	canvas_layer_free(canvas_base);
	canvas_layer_free(canvas_overlay);

	return NULL;
}

// Public functions



void canvas_start(unsigned int texSize, void (*on_close)()) {

	canvas_on_close_cb = on_close;
	canvas_tex_size = texSize;
	canvas_base = canvas_layer_alloc(canvas_tex_size, 0);
	canvas_overlay = canvas_layer_alloc(canvas_tex_size, 1);

	if (pthread_create(&canvas_thread, NULL, canvas_render_loop, NULL)) {
		puts("Failed to start render thread");
		exit(1);
	}
}

void canvas_setcb_key(void (*on_key)(int key, int scancode, int mods)) {
	canvas_on_key_cb = on_key;
}

void canvas_setcb_resize(void (*on_resize)()) {
	canvas_on_resize_cb = on_resize;
}

void canvas_close() {
	glfwSetWindowShouldClose(canvas_win, 1);
}

void canvas_fullscreen(int display) {
	canvas_display = display;
	canvas_do_layout = 1;
}

int canvas_get_display() {
	return canvas_display;
}

// Return a pointer to the GLubyte for a given pixel, or NULL for out of bound coordinates.
static inline GLubyte* canvas_offset(CanvasLayer * layer, unsigned int x,
		unsigned int y) {
	if (x < 0 || y < 0 || x >= layer->size || y >= layer->size || layer->data == NULL)
		return NULL;
	return layer->data
			+ ((y * layer->size) + x) * (layer->format == GL_RGBA ? 4 : 3);
}

void canvas_set_px(unsigned int x, unsigned int y, uint32_t rgba) {
	CanvasLayer * layer = canvas_base;
	GLubyte* ptr = canvas_offset(layer, x, y);

	if (ptr == NULL) {
		return;
	}

	GLubyte r = (rgba & 0xff000000) >> 24;
	GLubyte g = (rgba & 0x00ff0000) >> 16;
	GLubyte b = (rgba & 0x0000ff00) >> 8;
	GLubyte a = (rgba & 0x000000ff) >> 0;

	if (layer->format == GL_RGBA) {
		ptr[0] = r;
		ptr[1] = g;
		ptr[2] = b;
		ptr[3] = a;
		return;
	}
	if (a == 0) {
		return;
	}
	if (a < 0xff) {
		GLuint na = 0xff - a;
		r = (a * r + na * (ptr[0])) / 0xff;
		g = (a * g + na * (ptr[1])) / 0xff;
		b = (a * b + na * (ptr[2])) / 0xff;
		return;
	}
	ptr[0] = r;
	ptr[1] = g;
	ptr[2] = b;
}

void canvas_get_px(unsigned int x, unsigned int y, uint32_t *rgba) {
	CanvasLayer * layer = canvas_base;
	GLubyte* ptr = canvas_offset(layer, x, y);
	if (ptr == NULL) {
		*rgba = 0x000000;
	} else {
		*rgba = (ptr[0] << 24) + (ptr[1] << 16) + (ptr[2] << 8) + 0xff;
	}
}

void canvas_get_size(unsigned int *w, unsigned int *h) {
	// TODO: Clip on window size
	*w = canvas_base->size;
	*h = canvas_base->size;
}

