#ifndef __WSAASYNCGETADDRINFO__
#define __WSAASYNCGETADDRINFO__

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

struct getaddrinfo_args {
	HWND hWnd;
	unsigned int wMsg;
	const char FAR * hostname;
	const char FAR * portname;
	struct addrinfo FAR * hints;
	struct addrinfo FAR * FAR * res;
	HANDLE FAR * lpHandle;
};

HANDLE WSAAsyncGetAddrInfo(HWND, unsigned int, const char FAR *, const char FAR *, struct addrinfo FAR *, struct addrinfo FAR * FAR *);

#endif /* __WSAASYNCGETADDRINFO__ */
