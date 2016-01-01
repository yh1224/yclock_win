#include <winsock2.h>
#include <ws2tcpip.h>
#include <wininet.h>
#include <time.h>
#include "WSAASyncGetAddrInfo.h"
#include "misc.h"
#include "conf.h"
#include "debug.h"
#include "resource.h"
#include "ntp.h"
#include "ntp_timestamp.h"
#include "sntp.h"

// link with ws2_32.lib
#pragma comment(lib, "Ws2_32.lib")

extern HINSTANCE g_hInst;
extern struct conf g_Conf;

/* �O�񎞍��������� */
static struct tm s_LastSync;

/* Window Message */
#define WM_SYNC_INIT	WM_USER + 1		/* ������           */
#define WM_SYNC_START	WM_USER + 2		/* ���������J�n     */
#define WM_SYNC_GETHOST	WM_USER + 3		/* GetHostbyName()  */
#define WM_SYNC_SELECT	WM_USER + 4		/* �\�P�b�g�C�x���g */

/* Timer ID */
#define ID_TIMER_SYNC		1

/*
======================================================================
���������_�C�A���O
----------------------------------------------------------------------
*/
static HWND s_hwndSyncDlg = 0;
static SOCKET s_sockSync = INVALID_SOCKET;
static HANDLE s_hGetHost = 0;

static void
logSync(UINT uID, ...)
{
	char szForm[257];
	char szMsg[257];
	va_list ap;

	va_start(ap, uID);

	LoadString(g_hInst, uID, szForm, sizeof(szForm));
	wvsprintf(szMsg, szForm, ap);
	SendMessage(GetDlgItem(s_hwndSyncDlg, ID_SYNC_DETAILMSG), LB_ADDSTRING, 0, (LPARAM)szMsg);
//	SetDlgItemText(s_hwndSyncDlg, ID_SYNC_PROCMSG, szMsg);

	return;
}

static BOOL
syncTime(struct pkt *pkt, struct sync_param *syncParam)
{
	Timestamp* ts_org;	/* T1: Originated Timestamp */
	Timestamp* ts_rec;	/* T2: Received Timestamp */
	Timestamp* ts_xmt;	/* T3: Transmit Timestamp */
	Timestamp* ts_delay;	/* �����x������ */
	Timestamp* ts_got;	/* �擾�������� */
	Timestamp* ts_error;	/* ���ݎ����Ǝ擾���������Ƃ̌덷 */
	Timestamp* ts_shift;	/* �V�t�g���鎞�� */
	Timestamp* ts_set;	/* �ݒ肷�鎞�� */
	Timestamp* ts1;
	Timestamp* ts2;
	SYSTEMTIME st_set;
	BOOL rtn = TRUE;

	logSync(IDS_NTP_RECVPKTINFO,
		(pkt->li_vn_mode >> 6) & 0x03,	/* Location Information */
		(pkt->li_vn_mode >> 3) & 0x07,	/* Version Number */
		pkt->li_vn_mode & 0x07,		/* Mode */
		pkt->stratum,			/* Stratum */
		pkt->ppoll,			/* Peer Poll */
		pkt->precision);		/* Peer Precision */

	if ((pkt->li_vn_mode & 0xc0) == 0xc0) {
		logSync(IDS_NTP_NOTREADY);
		return FALSE;
	}

	ts_org = new Timestamp((char *)&pkt->org);
	ts_rec = new Timestamp((char *)&pkt->rec);
	ts_xmt = new Timestamp((char *)&pkt->xmt);

	SYSLOG((LOG_DEBUG, "T1: org = %lu.%06lu", ts_org->getSec(), ts_org->getUSec()));
	SYSLOG((LOG_DEBUG, "T2: rec = %lu.%06lu", ts_rec->getSec(), ts_rec->getUSec()));
	SYSLOG((LOG_DEBUG, "T3: xmt = %lu.%06lu", ts_xmt->getSec(), ts_xmt->getUSec()));

	/* �����x�� delay = (T4 - T1) - (T3 - T2) */
	ts1 = new Timestamp();	/* T4 */
	SYSLOG((LOG_DEBUG, "T4      = %lu.%06lu", ts1->getSec(), ts1->getUSec()));
	ts1->sub(ts_org);
	SYSLOG((LOG_DEBUG, "T4 - T1 = %lu.%06lu", ts1->getSec(), ts1->getUSec()));
	ts2 = new Timestamp(ts_xmt);
	ts2->sub(ts_rec);
	SYSLOG((LOG_DEBUG, "T3 - T2 = %lu.%06lu", ts2->getSec(), ts2->getUSec()));
	ts1->sub(ts2);
	delete ts2;
	ts_delay = ts1;

	if (ts_delay->getOverflow() < 0) {
		ts_delay->set(0, 0);
	}
	SYSLOG((LOG_DEBUG, "Roundtrip Delay = %lu.%06lu", ts_delay->getSec(), ts_delay->getUSec()));
	logSync(IDS_NTP_DELAY, ts_delay->getSec(), ts_delay->getUSec() / 1000);
	if (ts_delay->getSec() > 0 || ts_delay->getUSec() > (unsigned long)syncParam->nMaxDelay * 1000) {
		/* ���e�x�����Ԓ��� */
		return FALSE;
	}

	/* ���ݎ��� = T3 + d / 2 */
	ts_got = new Timestamp(ts_xmt);
	ts_delay->half();
	ts_got->add(ts_delay);
	SYSLOG((LOG_DEBUG, "Result Timestamp = %lu.%06lu", ts_got->getSec(), ts_got->getUSec()));

	/* �̈ӂɎ��Ԃ����炷 */
	ts_shift = new Timestamp(syncParam->nTimeShift, 0);
	ts_set = new Timestamp(ts_got);
	ts_set->sub(ts_shift);

	/* �덷 */
	ts_error = new Timestamp();
	ts_error->sub(ts_set);
	ts_error->abs();
	SYSLOG((LOG_DEBUG, "Clock difference = %lu.%06lu", ts_error->getSec(), ts_error->getUSec()));
	logSync(IDS_NTP_ERROR, ts_error->getSec(), ts_error->getUSec() / 1000);
	if (ts_error->getSec() == 0 && ts_error->getUSec() < (unsigned long)syncParam->nTolerance * 1000) {
		/* ���e�덷�͈͓��Ȃ̂Ŏ��ۂɂ͐ݒ肵�Ȃ� */
		syncParam->bSync = FALSE;
	}

	/* NTP�^�C���X�^���v���� SYSTEMTIME �ɕϊ� */
	if (NULL == ts_set->getSystemTime(&st_set)) {
		/* �ϊ��s�\�B���Ԃ�ςȃf�[�^ */
		SYSLOG((LOG_ERR, "syncTime(): Couldn't convert timestamp."));
		logSync(IDS_NTP_SYNC_NG);
		rtn = FALSE;
	}else{
		SYSLOG((LOG_DEBUG, "syncTime(): Result Time = %4d-%02d-%02d %d:%02d:%02d.%03d",
			st_set.wYear, st_set.wMonth, st_set.wDay,
			st_set.wHour, st_set.wMinute, st_set.wSecond, st_set.wMilliseconds));
		logSync(IDS_NTP_RECVTIME, st_set.wYear, st_set.wMonth, st_set.wDay,
			st_set.wHour, st_set.wMinute, st_set.wSecond, st_set.wMilliseconds);
		if (syncParam->bSync != FALSE) {
			/* �����ݒ� */
			if (FALSE == getClockPrivilege()) {
				SYSLOG((LOG_ERR, "syncTime(): No clock privilege."));
				logSync(IDS_NTP_SYNC_NG);
				rtn = FALSE;
			}else{
				if (FALSE == SetSystemTime(&st_set)) {
					SYSLOG((LOG_ERR, "syncTime(): SetSystemTime() failed."));
					logSync(IDS_NTP_SYNC_NG);
					rtn = FALSE;
				}else{
					SYSLOG((LOG_DEBUG, "syncTime(): Time syncing success."));
					memcpy(&s_LastSync, ts_got->getLocalTime(), sizeof(struct tm));
					logSync(IDS_NTP_SYNC_OK);
				}
			}
		}else{
			logSync(IDS_NTP_CHECK_OK);
			SYSLOG((LOG_DEBUG, "syncTime(): Sync test success."));
		}
	}

	delete ts_delay;
	delete ts_shift;
	delete ts_org;
	delete ts_xmt;
	delete ts_rec;
	delete ts_got;
	delete ts_error;

	return rtn;
}

/* �����p�P�b�g��M���� */
static BOOL
recvNtpPacket(struct pkt *pkt)
{
	SOCKADDR_STORAGE	saFrom;
	int nFromLen;
	int nLen;
	BOOL bRet = TRUE;

	nFromLen = sizeof(SOCKADDR_STORAGE);
	if (SOCKET_ERROR == (nLen = recvfrom(s_sockSync, (char *)pkt, sizeof(struct pkt), 0, (SOCKADDR *)&saFrom, &nFromLen))) {
		if (WSAECONNRESET == WSAGetLastError()) {
			SYSLOG((LOG_ERR, "recvNtpPacket(): recvfrom() failed. reseted by peer."));
			logSync(IDS_NTP_RECV_RESET, TRUE);
		}else{
			SYSLOG((LOG_ERR, "recvNtpPacket(): recvfrom() failed."));
			logSync(IDS_NTP_RECV_NG, TRUE, WSAGetLastError());
		}
		bRet = FALSE;
	}else if (nLen != 48) {
		SYSLOG((LOG_ERR, "recvNtpPacket(): recv length error. len=%d", nLen));
		logSync(IDS_NTP_RECV_NG, TRUE);
		bRet = FALSE;
	}else{
		SYSLOG((LOG_DEBUG, "recvNtpPacket(): recvfrom() success. length = %d", nLen));
	}
	return bRet;
}

/* SNTP���N�G�X�g�p�P�b�g�𑗐M���� */
static BOOL
sendNtpPacket(ADDRINFO *addr)
{
	struct pkt pkt;
	BOOL rtn = TRUE;
	int i;
	Timestamp now;

	/* ���N�G�X�g�p�P�b�g�̐ݒ� */
	memset(&pkt, 0, sizeof(struct pkt));
	pkt.li_vn_mode = 0x1B;		/* VN=3, Mode=3(Client) */
	//pkt.li_vn_mode = 0x23;	/* VN=4, Mode=3(Client) */
	now.getPacketData((char *)&pkt.xmt);	/* ���ݎ��� T1 */

	for (i = 0; i < SYNC_MAX_RETRY; i++) {
		if (SOCKET_ERROR != sendto(s_sockSync, (char*)&pkt, sizeof(struct pkt), 0, (SOCKADDR *)addr->ai_addr, addr->ai_addrlen)) {
			break;
		}
	}
	if (i == SYNC_MAX_RETRY) {
		/* Retry over */
		SYSLOG((LOG_ERR, "sendNtpPacket(): sendto() failed."));
		logSync(IDS_NTP_SEND_NG);
		rtn = FALSE;
	}else{
		SYSLOG((LOG_DEBUG, "sendNtpPacket(): sendto() success."));
	}
	return rtn;
}

/* NTP�Z�b�V�������J�n���� */
static SOCKET
initNtpSession(ADDRINFO *addr)
{
	const BOOL on = TRUE;

	/* �\�P�b�g���� */
	if (INVALID_SOCKET == (s_sockSync = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol))) {
		logSync(IDS_NTP_INIT_NG);
	}else{

		/* �u���[�h�L���X�g�L�� */
		setsockopt(s_sockSync, SOL_SOCKET, SO_BROADCAST, (const char *)&on, sizeof(on));

		SYSLOG((LOG_DEBUG, "initNtpSession(): created socket=%d", s_sockSync));
		if (SOCKET_ERROR == WSAAsyncSelect(s_sockSync, s_hwndSyncDlg, WM_SYNC_SELECT, FD_READ)) {
			SYSLOG((LOG_ERR, "initNtpSession(): WSAAsyncSelect() failed."));
			logSync(IDS_NTP_INIT_NG);
			closesocket(s_sockSync);
			s_sockSync = INVALID_SOCKET;
		}else{
			SYSLOG((LOG_DEBUG, "initNtpSession(): WSAAsyncSelect() success."));
		}
	}

	return s_sockSync;
}

/* �Z�b�V�����Ɋւ����n�� */
static void
endNtpSession()
{
	KillTimer(s_hwndSyncDlg, ID_TIMER_SYNC);

	/* �񓯊�GetAddrInfo()�n���h�����c���Ă���Β�~ */
	if (s_hGetHost != 0) {
		//WSACancelAsyncRequest(s_hGetHost);
		CloseHandle(s_hGetHost);
		SYSLOG((LOG_DEBUG, "endNtpSession(): Canceled async request handle=%d", s_hGetHost));
		s_hGetHost = 0;
	}

	/* �\�P�b�g�̊J�� */
	if (s_sockSync != INVALID_SOCKET) {
		closesocket(s_sockSync);
		SYSLOG((LOG_DEBUG, "endNtpSession(): Closed socket=%d", s_sockSync));
		s_sockSync = INVALID_SOCKET;
	}

	return;
}

/* �_�C�A���O�̏����I�� (�_�C�A���O�\�����́u����v�{�^���A��\�����͏I��) */
static void
endSyncDialog()
{
	if (FALSE == IsWindowVisible(s_hwndSyncDlg)) {
		DestroyWindow(s_hwndSyncDlg);
		s_hwndSyncDlg = 0;
	}else{
		SetDlgItemText(s_hwndSyncDlg, IDCANCEL, "����");
	}

	return;
}

BOOL CALLBACK
SyncDlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	static char szHostName[INTERNET_MAX_HOST_NAME_LENGTH];
	static ADDRINFO aiHints;
	static ADDRINFO *pHost;
	static int cntRetry;	/* ���g���C�J�E���g */
	static sync_param args;

	WSADATA	wsaData;
	struct pkt pkt;

//	SYSLOG((LOG_DEBUG, "SyncDlgProc(): Received Window Message %04x", uMsg));

	/* �I�������_�C�A���O�ւ̎c�����b�Z�[�W��Windows�ɔC���� */
	if (s_hwndSyncDlg != 0 && hwndDlg != s_hwndSyncDlg) {
		return FALSE;
	}

	switch (uMsg) {
	case WM_INITDIALOG:
		SYSLOG((LOG_DEBUG, "SyncDlgProc(): Received WM_INITDIALOG"));
		s_hwndSyncDlg = hwndDlg;
		s_sockSync = INVALID_SOCKET;
		s_hGetHost = 0;
		SendMessage(hwndDlg, WM_SYNC_INIT, 0, 0L);
		/* ��\���̏ꍇ�̓t�H�[�J�X��D��Ȃ� */
		if (FALSE == IsWindowVisible(hwndDlg)) {
			return FALSE;
		}
		break;

	/* ������ */
	case WM_SYNC_INIT:
		SYSLOG((LOG_DEBUG, "SyncDlgProc(): Received WM_SYNC_INIT"));
		if (0 != WSAStartup(MAKEWORD(1, 1), &wsaData)) {	/* �o�[�W���� 1.1 ��v�� */
			SYSLOG((LOG_ERR, "SyncDlgProc(): WSAStartup() failed."));
			logSync(IDS_NTP_INIT_NG);
			endSyncDialog();
		}else{
			SYSLOG((LOG_ERR, "SyncDlgProc(): WSAStartup() success."));
		}
		break;

	/* �������������J�n */
	case WM_SYNC_START:
		SYSLOG((LOG_DEBUG, "SyncDlgProc(): Received WM_SYNC_START"));
		memcpy(&args, (void *)lParam, sizeof(struct sync_param));

		/* WinSock������ */
		SetDlgItemText(hwndDlg, IDCANCEL, "��ݾ�");
		endNtpSession();

		/* �񓯊�GetAddrInfo()���s */
		memset(&aiHints, 0, sizeof(ADDRINFO));
		aiHints.ai_family = PF_UNSPEC;		/* �v���g�R���t�@�~��: ���w��(IPv4/v6) */
		aiHints.ai_socktype = SOCK_DGRAM;	/* �\�P�b�g�^�C�v: UDP */
		strcpy_s(szHostName, sizeof(szHostName), args.szServer);
		if (0 == (s_hGetHost = WSAAsyncGetAddrInfo(hwndDlg, WM_SYNC_GETHOST, szHostName, "123", &aiHints, &pHost))) {
			SYSLOG((LOG_ERR, "SyncDlgProc(): WSAsyncGetAddrInfo() failed."));
			logSync(IDS_NTP_INIT_NG);
			endSyncDialog();
		}else{
			SYSLOG((LOG_DEBUG, "SyncDlgProc(): WSASyncGetAddrInfo() success. hGetHost=%d", s_hGetHost));
			logSync(IDS_NTP_RESOLVING, szHostName);
		}
		break;

	/* gethostbyname()���� */
	case WM_SYNC_GETHOST:
		SYSLOG((LOG_DEBUG, "SyncDlgProc(): Received WM_SYNC_GETHOST"));
		if ((HANDLE)wParam == s_hGetHost) {
			if (0 != WSAGETASYNCERROR(lParam)) {
				logSync(IDS_NTP_RESOLV_NG);
				endNtpSession();
				endSyncDialog();
			}else{
				char buf[INET6_ADDRSTRLEN];
				getnameinfo(pHost->ai_addr, pHost->ai_addrlen, buf, sizeof(buf), NULL, 0, NI_NUMERICHOST);
				logSync(IDS_NTP_WAITING, buf);
				if (NULL == (s_sockSync = initNtpSession(pHost))) {
					endNtpSession();
					endSyncDialog();
				}else{
					cntRetry = 0;
					SetTimer(s_hwndSyncDlg, ID_TIMER_SYNC, 1000, NULL);
					if (FALSE == sendNtpPacket(pHost)) {
						closesocket(s_sockSync);
						s_sockSync = INVALID_SOCKET;
					}
				}
			}
			CloseHandle(s_hGetHost);
			SYSLOG((LOG_DEBUG, "SyncDlgProc(): Closed async request handle=%d", s_hGetHost));
			s_hGetHost = 0;
		}
		break;

	/* select()���� */
	case WM_SYNC_SELECT:
		SYSLOG((LOG_DEBUG, "SyncDlgProc(): Received WM_SYNC_SELECT"));
		/* �������ׂ� socket �̔��� */
		if((SOCKET)wParam == s_sockSync){
			/* ��M�C�x���g */
			if (FD_READ == WSAGETSELECTEVENT(lParam)) {
				if (FALSE != recvNtpPacket(&pkt)) {
					if (FALSE == syncTime(&pkt, &args)) {
						/* ��M���p������ */
						break;
					}
				}
				endNtpSession();
				endSyncDialog();
			}
		}
		break;

	case WM_TIMER:
		SYSLOG((LOG_DEBUG, "SyncDlgProc(): Received WM_TIMER"));
		/* �^�C���A�E�g�^�C�} */
		switch (wParam) {
		case ID_TIMER_SYNC:
			cntRetry++;
			if (cntRetry < SYNC_MAX_RETRY) {
				logSync(IDS_NTP_RETRY, cntRetry);
				SetTimer(s_hwndSyncDlg, ID_TIMER_SYNC, 1000, NULL);
				if (FALSE == sendNtpPacket(pHost)) {
					closesocket(s_sockSync);
					s_sockSync = INVALID_SOCKET;
				}
			}else{
				/* ���g���C�I�[�o�[ */
				logSync(IDS_NTP_TIMEOUT);
				endNtpSession();
				endSyncDialog();
			}
			break;
		}
		break;

	case WM_COMMAND:
		SYSLOG((LOG_DEBUG, "SyncDlgProc(): Received WM_COMMAND"));
		switch (LOWORD(wParam)) {
		case IDOK:
		case IDCANCEL:
			DestroyWindow(hwndDlg);
			break;
	 	}
		break;

	case WM_CLOSE:
		SYSLOG((LOG_DEBUG, "SyncDlgProc(): Received WM_CLOSE"));
		DestroyWindow(hwndDlg);
		break;

	case WM_DESTROY:
		SYSLOG((LOG_DEBUG, "SyncDlgProc(): Received WM_DESTROY"));
		endNtpSession();
		WSACleanup();
		SYSLOG((LOG_DEBUG, "SyncDlgProc(): WSACleanup() called."));
		s_hwndSyncDlg = 0;
		break;

	default:
		return FALSE;
	}

	return TRUE;
}




/*
======================================================================
�������� (�G���g��)
----------------------------------------------------------------------
*/
BOOL
syncClock(
	HWND	hWnd,		/* �e�E�B���h�E�n���h�� */
	struct sync_param *sa,	/* ���������p�����^ */
	BOOL	bShow		/* �_�C�A���O��\�����邩�ǂ��� */
)
{
	HWND hwndDlg;

	if (!IsWindow(s_hwndSyncDlg)) {
		//EnableWindow(hWnd, FALSE);
		hwndDlg = CreateDialog(g_hInst, MAKEINTRESOURCE(IDD_SYNC), hWnd, SyncDlgProc);
	}else{
		hwndDlg = s_hwndSyncDlg;
		/* ���ɋN�����Ă��� */
		SYSLOG((LOG_DEBUG, "syncClock(): Sync dialog already running"));
		/* �E�B���h�E�\�����̏ꍇ�͉������Ȃ� */
		if (FALSE != IsWindowVisible(s_hwndSyncDlg)) {
			return FALSE;
		}
	}
	/* �_�C�A���O��\������ */
	if (FALSE != bShow) {
		ShowWindow(hwndDlg, SW_SHOW);
	}
	SendMessage(hwndDlg, WM_SYNC_START, (WPARAM)0, (LPARAM)sa);

	return TRUE;
}

char *
getLastSync()
{
	static char szPrevSync[32];

	if (s_LastSync.tm_year != 0) {
		//strftime(szPrevSync, sizeof(szPrevSync), "%x %X %z", &s_LastSync);
		strftime(szPrevSync, sizeof(szPrevSync), "%Y-%m-%d %H:%M:%S", &s_LastSync);
		return szPrevSync;
	}

	return NULL;
}

