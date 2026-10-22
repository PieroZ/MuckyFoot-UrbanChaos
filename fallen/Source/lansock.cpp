//
// lansock.cpp
//
// Winsock TCP transport for LAN play. See lansock.h.
//
// NOTE: no game headers in here on purpose (winsock2.h has to come before
// windows.h, and the game headers pull windows.h in on their own).
//

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <string.h>

#include "lansock.h"

#pragma comment(lib, "ws2_32.lib")

static int    LANSOCK_started   = 0;
static SOCKET LANSOCK_listener  = INVALID_SOCKET;
static SOCKET LANSOCK_sock      = INVALID_SOCKET;
static int    LANSOCK_is_up     = 0;
static int    LANSOCK_connecting = 0;
static int    LANSOCK_peer_closed = 0;
static char   LANSOCK_peer[64]  = "";

//
// Receive buffer: bytes that arrived but don't form a whole message yet.
//

#define LANSOCK_RBUF (LANSOCK_MAX_MSG * 8)

static unsigned char LANSOCK_rbuf[LANSOCK_RBUF];
static int           LANSOCK_rlen = 0;

static void LANSOCK_set_options(SOCKET s)
{
	u_long nonblocking = 1;
	ioctlsocket(s, FIONBIO, &nonblocking);

	//
	// Lockstep sends lots of tiny packets - don't let Nagle sit on them.
	//

	BOOL nodelay = TRUE;
	setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (const char *) &nodelay, sizeof(nodelay));
}

int LANSOCK_init(void)
{
	if (LANSOCK_started)
	{
		return 1;
	}

	WSADATA wsa;

	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
	{
		return 0;
	}

	LANSOCK_started = 1;

	return 1;
}

void LANSOCK_close(void)
{
	if (LANSOCK_sock != INVALID_SOCKET)
	{
		shutdown(LANSOCK_sock, SD_BOTH);
		closesocket(LANSOCK_sock);
		LANSOCK_sock = INVALID_SOCKET;
	}

	if (LANSOCK_listener != INVALID_SOCKET)
	{
		closesocket(LANSOCK_listener);
		LANSOCK_listener = INVALID_SOCKET;
	}

	LANSOCK_is_up       = 0;
	LANSOCK_connecting  = 0;
	LANSOCK_peer_closed = 0;
	LANSOCK_rlen        = 0;
}

void LANSOCK_shutdown(void)
{
	LANSOCK_close();

	if (LANSOCK_started)
	{
		WSACleanup();
		LANSOCK_started = 0;
	}
}

int LANSOCK_listen(unsigned short port)
{
	LANSOCK_close();

	if (!LANSOCK_init())
	{
		return 0;
	}

	LANSOCK_listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	if (LANSOCK_listener == INVALID_SOCKET)
	{
		return 0;
	}

	BOOL reuse = TRUE;
	setsockopt(LANSOCK_listener, SOL_SOCKET, SO_REUSEADDR, (const char *) &reuse, sizeof(reuse));

	sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family      = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port        = htons(port);

	if (bind(LANSOCK_listener, (sockaddr *) &addr, sizeof(addr)) == SOCKET_ERROR ||
		listen(LANSOCK_listener, 1) == SOCKET_ERROR)
	{
		closesocket(LANSOCK_listener);
		LANSOCK_listener = INVALID_SOCKET;
		return 0;
	}

	u_long nonblocking = 1;
	ioctlsocket(LANSOCK_listener, FIONBIO, &nonblocking);

	return 1;
}

int LANSOCK_accept_poll(void)
{
	if (LANSOCK_is_up)
	{
		return 1;
	}

	if (LANSOCK_listener == INVALID_SOCKET)
	{
		return -1;
	}

	sockaddr_in from;
	int         fromlen = sizeof(from);

	SOCKET s = accept(LANSOCK_listener, (sockaddr *) &from, &fromlen);

	if (s == INVALID_SOCKET)
	{
		return (WSAGetLastError() == WSAEWOULDBLOCK) ? 0 : -1;
	}

	LANSOCK_set_options(s);

	LANSOCK_sock  = s;
	LANSOCK_is_up = 1;
	LANSOCK_rlen  = 0;

	inet_ntop(AF_INET, &from.sin_addr, LANSOCK_peer, sizeof(LANSOCK_peer));

	//
	// Only ever one peer.
	//

	closesocket(LANSOCK_listener);
	LANSOCK_listener = INVALID_SOCKET;

	return 1;
}

int LANSOCK_connect_start(const char *address, unsigned short port)
{
	LANSOCK_close();

	if (!LANSOCK_init())
	{
		return 0;
	}

	sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port   = htons(port);

	if (inet_pton(AF_INET, address, &addr.sin_addr) != 1)
	{
		//
		// Not a dotted address - try it as a machine name.
		//

		addrinfo  hints;
		addrinfo *res = NULL;

		memset(&hints, 0, sizeof(hints));
		hints.ai_family   = AF_INET;
		hints.ai_socktype = SOCK_STREAM;

		if (getaddrinfo(address, NULL, &hints, &res) != 0 || res == NULL)
		{
			return 0;
		}

		addr.sin_addr = ((sockaddr_in *) res->ai_addr)->sin_addr;

		freeaddrinfo(res);
	}

	LANSOCK_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	if (LANSOCK_sock == INVALID_SOCKET)
	{
		return 0;
	}

	LANSOCK_set_options(LANSOCK_sock);

	if (connect(LANSOCK_sock, (sockaddr *) &addr, sizeof(addr)) == SOCKET_ERROR)
	{
		int err = WSAGetLastError();

		if (err != WSAEWOULDBLOCK && err != WSAEINPROGRESS)
		{
			closesocket(LANSOCK_sock);
			LANSOCK_sock = INVALID_SOCKET;
			return 0;
		}
	}

	inet_ntop(AF_INET, &addr.sin_addr, LANSOCK_peer, sizeof(LANSOCK_peer));

	LANSOCK_connecting = 1;
	LANSOCK_rlen       = 0;

	return 1;
}

int LANSOCK_connect_poll(void)
{
	if (LANSOCK_is_up)
	{
		return 1;
	}

	if (!LANSOCK_connecting || LANSOCK_sock == INVALID_SOCKET)
	{
		return -1;
	}

	fd_set wset;
	fd_set eset;
	FD_ZERO(&wset);
	FD_ZERO(&eset);
	FD_SET(LANSOCK_sock, &wset);
	FD_SET(LANSOCK_sock, &eset);

	timeval tv;
	tv.tv_sec  = 0;
	tv.tv_usec = 0;

	int n = select(0, NULL, &wset, &eset, &tv);

	if (n == SOCKET_ERROR)
	{
		return -1;
	}

	if (n == 0)
	{
		return 0;	// Still trying.
	}

	if (FD_ISSET(LANSOCK_sock, &eset))
	{
		closesocket(LANSOCK_sock);
		LANSOCK_sock       = INVALID_SOCKET;
		LANSOCK_connecting = 0;
		return -1;	// Refused / unreachable.
	}

	LANSOCK_connecting = 0;
	LANSOCK_is_up      = 1;

	return 1;
}

int LANSOCK_connected(void)
{
	return LANSOCK_is_up;
}

const char *LANSOCK_peer_name(void)
{
	return LANSOCK_peer;
}

static int LANSOCK_send_all(const unsigned char *data, int len)
{
	int   sent  = 0;
	DWORD start = GetTickCount();

	while (sent < len)
	{
		int n = send(LANSOCK_sock, (const char *) data + sent, len - sent, 0);

		if (n == SOCKET_ERROR)
		{
			if (WSAGetLastError() != WSAEWOULDBLOCK)
			{
				return 0;
			}

			if (GetTickCount() - start > 5000)
			{
				return 0;	// The peer stopped reading for 5 seconds.
			}

			fd_set wset;
			FD_ZERO(&wset);
			FD_SET(LANSOCK_sock, &wset);

			timeval tv;
			tv.tv_sec  = 0;
			tv.tv_usec = 10000;

			select(0, NULL, &wset, NULL, &tv);

			continue;
		}

		sent += n;
	}

	return 1;
}

int LANSOCK_send(const void *data, int len)
{
	if (!LANSOCK_is_up || len <= 0 || len > LANSOCK_MAX_MSG)
	{
		return 0;
	}

	unsigned char frame[LANSOCK_MAX_MSG + 2];

	frame[0] = (unsigned char) (len & 0xff);
	frame[1] = (unsigned char) (len >> 8);

	memcpy(frame + 2, data, len);

	if (!LANSOCK_send_all(frame, len + 2))
	{
		LANSOCK_close();
		return 0;
	}

	return 1;
}

int LANSOCK_poll(void *buf, int maxlen)
{
	if (!LANSOCK_is_up)
	{
		return -1;
	}

	//
	// Pull in whatever has arrived.
	//

	while (!LANSOCK_peer_closed && LANSOCK_rlen < LANSOCK_RBUF)
	{
		int n = recv(LANSOCK_sock, (char *) LANSOCK_rbuf + LANSOCK_rlen, LANSOCK_RBUF - LANSOCK_rlen, 0);

		if (n == 0)
		{
			//
			// Orderly close by the peer - but hand out what we still have first.
			//

			LANSOCK_peer_closed = 1;

			break;
		}

		if (n == SOCKET_ERROR)
		{
			if (WSAGetLastError() == WSAEWOULDBLOCK)
			{
				break;
			}

			LANSOCK_close();
			return -1;
		}

		LANSOCK_rlen += n;
	}

	if (LANSOCK_rlen < 2)
	{
		if (LANSOCK_peer_closed)
		{
			LANSOCK_close();
			return -1;
		}

		return 0;
	}

	int len = LANSOCK_rbuf[0] | (LANSOCK_rbuf[1] << 8);

	if (len <= 0 || len > LANSOCK_MAX_MSG)
	{
		LANSOCK_close();	// Garbage on the wire.
		return -1;
	}

	if (LANSOCK_rlen < len + 2)
	{
		if (LANSOCK_peer_closed)
		{
			LANSOCK_close();
			return -1;
		}

		return 0;
	}

	if (len > maxlen)
	{
		LANSOCK_close();
		return -1;
	}

	memcpy(buf, LANSOCK_rbuf + 2, len);

	LANSOCK_rlen -= len + 2;

	memmove(LANSOCK_rbuf, LANSOCK_rbuf + len + 2, LANSOCK_rlen);

	return len;
}

void LANSOCK_wait(int ms)
{
	if (!LANSOCK_is_up || LANSOCK_sock == INVALID_SOCKET)
	{
		Sleep(ms);
		return;
	}

	if (LANSOCK_rlen >= 2)
	{
		return;	// Something buffered already.
	}

	fd_set rset;
	FD_ZERO(&rset);
	FD_SET(LANSOCK_sock, &rset);

	timeval tv;
	tv.tv_sec  = ms / 1000;
	tv.tv_usec = (ms % 1000) * 1000;

	select(0, &rset, NULL, NULL, &tv);
}
