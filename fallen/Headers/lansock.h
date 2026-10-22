//
// lansock.h
//
// Tiny TCP transport for LAN play (Winsock). One connection only: either
// this machine listens and accepts one peer (host) or it connects to a host
// (client). Messages are framed (2-byte length + payload) so the caller
// always gets whole messages.
//
// Deliberately free of any game headers - winsock2.h must not meet the
// game's own windows.h includes.
//

#ifndef LANSOCK_H
#define LANSOCK_H

#define LANSOCK_MAX_MSG		1024

//
// Startup / shutdown of Winsock. Safe to call more than once.
//

int  LANSOCK_init    (void);
void LANSOCK_shutdown(void);

//
// Host side: start listening, then poll until a peer arrives.
// LANSOCK_accept_poll() returns 1 when connected, 0 while waiting, -1 on error.
//

int  LANSOCK_listen     (unsigned short port);
int  LANSOCK_accept_poll(void);

//
// Client side: start a non-blocking connect, then poll.
// LANSOCK_connect_poll() returns 1 when connected, 0 while waiting, -1 on failure
// (the caller may simply call LANSOCK_connect_start() again to retry).
//

int  LANSOCK_connect_start(const char *address, unsigned short port);
int  LANSOCK_connect_poll (void);

//
// Messages. LANSOCK_send() returns 1 on success, 0 if the connection broke.
// LANSOCK_poll() returns the length of a complete message copied into 'buf',
// 0 if none is waiting yet, -1 if the connection is gone.
//

int  LANSOCK_send(const void *data, int len);
int  LANSOCK_poll(void *buf, int maxlen);

//
// Blocks until there is something to read (or 'ms' milliseconds pass).
//

void LANSOCK_wait(int ms);

int  LANSOCK_connected(void);
void LANSOCK_close    (void);

//
// The peer's address as text (for messages).
//

const char *LANSOCK_peer_name(void);

#endif
