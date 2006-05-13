#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <process.h>
#include "WSAASyncGetAddrInfo.h"

//static void getaddrinfo_thread(void FAR *);
static unsigned int CALLBACK getaddrinfo_thread(void FAR *);

HANDLE
WSAAsyncGetAddrInfo(
	HWND hWnd,						/* ���ʒʒm��E�B���h�E�n���h�� */
	unsigned int wMsg,				/* ���ʒʒm�p���b�Z�[�WID */

	const char FAR * hostname,		/* �ȉ��Agetaddrinfo() �̈��� */
	const char FAR * portname,
	struct addrinfo FAR * hints,
	struct addrinfo FAR * FAR * res
)
{
	unsigned long thread;
	struct getaddrinfo_args FAR * ga;

	/*
	 *  �X���b�h�֓n���\���̂𓮓I�Ɋm�ۂ���
	 */
	ga = (struct getaddrinfo_args FAR *)malloc(sizeof(struct getaddrinfo_args));
	if (ga == NULL) {
		return NULL;
	}

	/* addrinfo_args �Ɉ�����ݒ� */
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

	/* getaddrinfo() �����s����X���b�h�𐶐����� */
	//thread = _beginthread(getaddrinfo_thread, 0, ga);	������ł͗�����
	thread = _beginthreadex(NULL, 0, getaddrinfo_thread, ga, CREATE_SUSPENDED, NULL);
	if (thread == -1) {
		/* ���s */
		free(ga->lpHandle);
		free(ga);
		return NULL;
	}

	*ga->lpHandle = (HANDLE)thread;
	ResumeThread((HANDLE)thread);	/* �����ŃX���b�h���s�J�n */
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

	/* ������W�J */
	ga = (struct getaddrinfo_args FAR *)p;
	hWnd = ga->hWnd;
	wMsg = ga->wMsg;
	hostname = ga->hostname;
	portname = ga->portname;
	hints = ga->hints;
	res = ga->res;

	/* getaddrinfo()�Ăяo�� */
	gai = getaddrinfo(hostname, portname, hints, res);

	/* ���ʂ� hWnd �Ƀ��b�Z�[�W�ʒm���� */
	PostMessage(hWnd, wMsg, (WPARAM)*ga->lpHandle, MAKELPARAM(0, gai));

	free(ga->lpHandle);
	free(p);

	return 0;
}
