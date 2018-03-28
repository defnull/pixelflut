#ifndef NET_H_
#define NET_H_

typedef struct NetClient NetClient;

#define NET_CSTATE_OPEN 0
#define NET_CSTATE_CLOSING 1

// Callback called immediately after a client connects
typedef void (*net_on_connect)(NetClient *client);

// Callback called for each line of input.
// The char array is null-terminated and does not include the final line break.
// It is NOT owned by the callback and freed as soon as the callback returned.
typedef void (*net_on_read)(NetClient *client, char* line);

// Callback called after a client disconnects.
// The second parameter is 0 for a normal client-induced disconnect and != 0 on errors.
typedef void (*net_on_close)(NetClient *client, int error);

// Start the server and block until it is closed again.
void net_start(int port, net_on_connect on_connect, net_on_read on_read, net_on_close on_close);

// Stop the server as soon as possible
void net_stop();

// Send a string to the client. A newline is added automatically.
void net_send(NetClient *client, const char * msg);
// Stop reading from this clients socket, send all bytes still in the output buffer, then close the connection.
void net_close(NetClient *client);
// Send an error message to the client, then close the connection.
void net_err(NetClient *client, const char * msg);

// Get or set the user attachment, a pointer to an arbitrary data structure or NULL
void net_set_user(NetClient *client, void *user);
void net_get_user(NetClient *client, void **user);

#endif /* NET_H_ */
