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

// global state
static struct event_base *base;

// User defined callbacks
static net_on_connect netcb_on_connect = NULL;
static net_on_read netcb_on_read = NULL;
static net_on_close netcb_on_close = NULL;



// libevent callbacks

static void netev_on_read(struct bufferevent *bev, void *ctx) {
	struct evbuffer *input;
	char *line;
	size_t n;
	NetClient *client = ctx;

	input = bufferevent_get_input(bev);

	// Change while->if for less throughput but more fair pixel distribution across client connections.
	while ((line = evbuffer_readln(input, &n, EVBUFFER_EOL_LF))) {
		(*netcb_on_read)(client, line);
		free(line);
	}

	if (evbuffer_get_length(input) >= NET_MAX_LINE) {
		net_err(client, "Line to long");
	}
}

static void netev_on_write(struct bufferevent *bev, void *arg) {
	NetClient *client = arg;

	if (client->state == NET_CSTATE_CLOSING && evbuffer_get_length(bufferevent_get_output(bev)) == 0) {

		if(netcb_on_close)
			(*netcb_on_close)(client, 0);

		bufferevent_free(bev);
		free(client);
	}
}

static void netev_on_error(struct bufferevent *bev, short error, void *arg) {
	NetClient *client = arg;

	// TODO: Some logging?
	if (error & BEV_EVENT_EOF) {
	} else if (error & BEV_EVENT_ERROR) {
	} else if (error & BEV_EVENT_TIMEOUT) {
	}

	client->state = NET_CSTATE_CLOSING;
	if(netcb_on_close)
		(*netcb_on_close)(client, error);

	bufferevent_free(bev);
	free(client);
}

static void on_accept(evutil_socket_t listener, short event, void *arg) {
	struct event_base *base = arg;
	struct sockaddr_storage ss;
	socklen_t slen = sizeof(ss);
	int fd = accept(listener, (struct sockaddr*) &ss, &slen);
	if (fd < 0) {
		perror("accept failed");
	} else if (fd > FD_SETSIZE) {
		close(fd); // TODO: verify if this is needed. Only for select()? But libevent uses poll/kqueue?
	} else {
		NetClient *client = calloc(1, sizeof(NetClient));
		if (client == NULL) {
			perror("client malloc failed");
			close(fd);
			return;
		}

		client->sock_fd = fd;
		client->buf_ev = bufferevent_socket_new(base, fd, BEV_OPT_CLOSE_ON_FREE);

		evutil_make_socket_nonblocking(fd);
		bufferevent_setcb(client->buf_ev, netev_on_read, netev_on_write, netev_on_error, client);
		bufferevent_setwatermark(client->buf_ev, EV_READ, 0, NET_MAX_BUFFER);

		if(netcb_on_connect)
			(*netcb_on_connect)(client);

		if(client->state == NET_CSTATE_OPEN)
			bufferevent_enable(client->buf_ev, EV_READ | EV_WRITE);
	}
}


// Public functions

void net_start(int port, net_on_connect on_connect, net_on_read on_read, net_on_close on_close) {
	evutil_socket_t listener;
	struct sockaddr_in sin;
	struct event *listener_event;

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
	sin.sin_port = htons(1337);
	listener = socket(AF_INET, SOCK_STREAM, 0);
	evutil_make_socket_nonblocking(listener);
	evutil_make_listen_socket_reuseable(listener);

	if (bind(listener, (struct sockaddr*) &sin, sizeof(sin)) < 0) {
		err(1, "bind failed");
	}

	if (listen(listener, 16) < 0) {
		err(1, "listen failed");
	}

	listener_event = event_new(base, listener, EV_READ | EV_PERSIST, on_accept, (void*) base);
	event_add(listener_event, NULL);

	event_base_dispatch(base);
}


void net_stop() {
	event_base_loopbreak(base);
}


void net_send(NetClient *client, const char * msg) {
	struct evbuffer *output = bufferevent_get_output(client->buf_ev);
	evbuffer_add(output, msg, strlen(msg));
	evbuffer_add(output, "\n", 1);
}

void net_close(NetClient *client) {
	if(client->state == NET_CSTATE_OPEN) {
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

