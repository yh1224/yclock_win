#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <process.h>
#include "WSAASyncGetAddrInfo.h"

//static void getaddrinfo_thread(void FAR *);
static unsigned int CALLBACK getaddrinfo_thread(void FAR *);

HANDLE
WSAAsyncGetAddrInfo(
	HWND hWnd,						/* 結果通知先ウィンドウハンドル */
	unsigned int wMsg,				/* 結果通知用メッセージID */

	const char FAR * hostname,		/* 以下、getaddrinfo() の引数 */
	const char FAR * portname,
	struct addrinfo FAR * hints,
	struct addrinfo FAR * FAR * res
)
{
	unsigned long thread;
	struct getaddrinfo_args FAR * ga;

	/*
	 *  スレッドへ渡す構造体を動的に確保する
	 */
	ga = (struct getaddrinfo_args FAR *)malloc(sizeof(struct getaddrinfo_args));
	if (ga == NULL) {
		return NULL;
	}

	/* addrinfo_args に引数を設定 */
	ga->hWnd = hWnd;
	ga->wMsg = wMsg;
	ga->hostname = hostname;
	ga->portname = portname;
	ga->hints = hints;
	ga->res = res;

	ga->lpHandle = (HANDLE FAR *)malloc(sizeof(HANDLE));
	if (ga->lpHandle == NULL) {
		return NULL;
	}

	/* getaddrinfo() を実行するスレッドを生成する */
	//thread = _beginthread(getaddrinfo_thread, 0, ga);	※これでは落ちる
	thread = _beginthreadex(NULL, 0, getaddrinfo_thread, ga, CREATE_SUSPENDED, NULL);
	if (thread == -1) {
		/* 失敗 */
		free(ga->lpHandle);
		free(ga);
		return NULL;
	}

	*ga->lpHandle = (HANDLE)thread;
	ResumeThread((HANDLE)thread);	/* ここでスレッド実行開始 */
	return (HANDLE)thread;
}

//static void getaddrinfo_thread(void FAR * p)
static unsigned int CALLBACK
getaddrinfo_thread(void FAR * p) {
	int gai;
	HWND hWnd;
	unsigned int wMsg;
	const char FAR * hostname;
	const char FAR * portname;
	struct addrinfo FAR * hints;
	struct addrinfo FAR * FAR * res;
	struct getaddrinfo_args FAR * ga;

	/* 引数を展開 */
	ga = (struct getaddrinfo_args FAR *)p;
	hWnd = ga->hWnd;
	wMsg = ga->wMsg;
	hostname = ga->hostname;
	portname = ga->portname;
	hints = ga->hints;
	res = ga->res;

	/* getaddrinfo()呼び出し */
	gai = getaddrinfo(hostname, portname, hints, res);

	/* 結果を hWnd にメッセージ通知する */
	PostMessage(hWnd, wMsg, (WPARAM)*ga->lpHandle, MAKELPARAM(0, gai));

	free(ga->lpHandle);
	free(p);

	return 0;
}
