#include <netinet/in.h>
#include <sys/socket.h>
#include <fcntl.h>

#include <event2/event.h>
#include <event2/buffer.h>
#include <event2/thread.h>
#include <event2/bufferevent.h>

#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <err.h>

#include "net.h"

// Lines longer than this are considered an error.
#define NET_MAX_LINE 1024

// The server buffers up to NET_READ_BUFFER bytes per client connection.
// Lower values allow lots of clients to draw at the same time, each with a fair share.
// Higher values increase throughput but fast clients might be able to draw large batches at once.
#define NET_MAX_BUFFER 10240

typedef struct NetClient {
	int sock_fd;
	struct bufferevent *buf_ev;
	int state;
	void *user;
} NetClient;

#define NET_CSTATE_OPEN 0
#define NET_CSTATE_CLOSING 1

static inline int min(int a, int b) {
	return a < b ? a : b;
}

// global state
static struct event_base *base;
static char * line_buffer;

// User defined callbacks
static net_on_connect netcb_on_connect = NULL;
static net_on_read netcb_on_read = NULL;
static net_on_close netcb_on_close = NULL;

// libevent callbacks


// Like evbuffer_readln, but read into an existing string instead of allocating a new one.
// Return 1 if a line ending was found and the line was read completely.
// Return 2 if a line ending was found but the line was read only partially (longer than maxread-1).
// Return 0 if no line ending not found within the buffer.
int net_evbuffer_readln(struct evbuffer *buffer, char *line, size_t maxread, size_t * read_out, enum evbuffer_eol_style eol_style) {
	struct evbuffer_ptr it;
	size_t read_len=0, eol_len=0;

	it = evbuffer_search_eol(buffer, NULL, &eol_len, eol_style);
	if (it.pos < 0)
		return 0;

	read_len = min(maxread-1, it.pos);

	if(read_out)
		*read_out = read_len;

	evbuffer_remove(buffer, line, read_len);
	line[read_len] = '\0';

	// Eat EOL if we have read all bytes before it
	if(read_len == it.pos) {
		evbuffer_drain(buffer, eol_len);
		return 1;
	} else {
		return 2;
	}
}

static void netev_on_read(struct bufferevent *bev, void *ctx) {
	struct evbuffer *input;
	NetClient *client = ctx;
	int r = 0;

	input = bufferevent_get_input(bev);

	// Change while->if for less throughput but more fair pixel distribution across client connections.
	while ((r = net_evbuffer_readln(input, line_buffer, NET_MAX_LINE, NULL, EVBUFFER_EOL_LF)) == 1) {
		(*netcb_on_read)(client, line_buffer);
	}

	if (r == 2) {
		net_err(client, "Line to long");
	}
}


static void netev_on_write(struct bufferevent *bev, void *arg) {
	NetClient *client = arg;

	if (client->state == NET_CSTATE_CLOSING
			&& evbuffer_get_length(bufferevent_get_output(bev)) == 0) {

		if (netcb_on_close)
			(*netcb_on_close)(client, 0);

		bufferevent_free(bev);
		printf("bufferevent_free: closed\n");
		free(client);
	}
}

static void netev_on_error(struct bufferevent *bev, short error, void *arg) {
	NetClient *client = arg;

	// TODO: Some logging?
	if (error & BEV_EVENT_EOF) {
		printf("error EOF\n");
	} else if (error & BEV_EVENT_ERROR) {
		printf("error ERROR\n");
	} else if (error & BEV_EVENT_TIMEOUT) {
		printf("error TIMEOUT\n");
	}

	client->state = NET_CSTATE_CLOSING;
	if (netcb_on_close)
		(*netcb_on_close)(client, error);

	bufferevent_free(bev);
	printf("bufferevent_free: closed\n");
	free(client);
}

static void on_accept(evutil_socket_t listener, short event, void *arg) {
	struct event_base *base = arg;
	struct sockaddr_storage ss;
	socklen_t slen = sizeof(ss);
	int fd = accept(listener, (struct sockaddr*) &ss, &slen);
	if (fd < 0) {
		perror("Failed to accept");
	} else if (fd > FD_SETSIZE) {
		close(fd); // TODO: verify if this is needed. Only for select()? But libevent uses poll/kqueue?
		printf("fd closed\n");
	} else {
		NetClient *client = calloc(1, sizeof(NetClient));
		if (client == NULL) {
			perror("Failed to calloc client");
			close(fd);
			return;
		}

		client->sock_fd = fd;
		evutil_make_socket_nonblocking(fd);
		client->buf_ev = bufferevent_socket_new(base, fd,
				BEV_OPT_CLOSE_ON_FREE);

		bufferevent_setcb(client->buf_ev, netev_on_read, netev_on_write,
				netev_on_error, client);
		bufferevent_setwatermark(client->buf_ev, EV_READ, 0, NET_MAX_BUFFER);

		if (netcb_on_connect)
			(*netcb_on_connect)(client);

		if (client->state == NET_CSTATE_OPEN)
			bufferevent_enable(client->buf_ev, EV_READ | EV_WRITE);
	}
}

// Public functions

void net_start(int port, net_on_connect on_connect, net_on_read on_read,
		net_on_close on_close) {
	evutil_socket_t listener;
	struct sockaddr_in sin;
	struct event *listener_event;

	line_buffer = malloc(sizeof(char)*NET_MAX_LINE);
	if (!line_buffer)
		err(1, "Failed to malloc");

	event_enable_debug_mode();
	evthread_use_pthreads();

	//setvbuf(stdout, NULL, _IONBF, 0);

	netcb_on_connect = on_connect;
	netcb_on_read = on_read;
	netcb_on_close = on_close;

	base = event_base_new();
	if (!base)
		err(1, "Failed to create event_base");

	//evthread_make_base_notifiable(base);

	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = 0;
	sin.sin_port = htons(port);
	listener = socket(AF_INET, SOCK_STREAM, 0);
	evutil_make_socket_nonblocking(listener);
	evutil_make_listen_socket_reuseable(listener);

	if (bind(listener, (struct sockaddr*) &sin, sizeof(sin)) < 0) {
		err(1, "Failed to bind");
	}

	if (listen(listener, 16) < 0) {
		err(1, "Failed to listen");
	}

	listener_event = event_new(base, listener, EV_READ | EV_PERSIST, on_accept,
			(void*) base);
	event_add(listener_event, NULL);

	event_base_dispatch(base);
}

void net_stop() {
	event_base_loopbreak(base);
	//	we cannot free the line_buffer here because it may be needed (doc says
	//	"will abort the loop *after* the next event is completed").
	//if(line_buffer) {
	//	free(line_buffer);
	//	line_buffer = NULL;
	//}
}

void net_send(NetClient *client, const char * msg) {
	struct evbuffer *output = bufferevent_get_output(client->buf_ev);
	evbuffer_add(output, msg, strlen(msg));
	evbuffer_add(output, "\n", 1);
}

void net_close(NetClient *client) {
	if (client->state == NET_CSTATE_OPEN) {
		client->state = NET_CSTATE_CLOSING;
		bufferevent_disable(client->buf_ev, EV_READ);
	}
}

void net_err(NetClient *client, const char * msg) {
	struct evbuffer *output = bufferevent_get_output(client->buf_ev);
	evbuffer_add(output, "ERROR: ", 7);
	evbuffer_add(output, msg, strlen(msg));
	evbuffer_add(output, "\n", 1);
	net_close(client);
}

void net_set_user(NetClient *client, void *user) {
	client->user = user;
}

void net_get_user(NetClient *client, void **user) {
	*user = client->user;
}

