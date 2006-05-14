#include <winsock2.h>
#include <ws2tcpip.h>
#include <wininet.h>
#include <time.h>
#include "WSAASyncGetAddrInfo.h"
#include "config.h"
#include "debug.h"
#include "resource.h"
#include "ntp.h"
#include "sntp.h"

extern HINSTANCE g_hInst;
extern struct conf g_Conf;

/* 前回時刻同期日時 */
static struct timestamp s_tsPrevSync;

/* Window Message */
#define WM_SYNC_INIT	WM_USER + 1		/* 初期化           */
#define WM_SYNC_START	WM_USER + 2		/* 時刻同期開始     */
#define WM_SYNC_GETHOST	WM_USER + 3		/* GetHostbyName()  */
#define WM_SYNC_SELECT	WM_USER + 4		/* ソケットイベント */

/* Timer ID */
#define ID_TIMER_SYNC		1

/*
めも
	F = 2^32
	x = NTPタイムスタンプの分数部分
	n = ミリ秒
	とすると

	x / F = n / 1000
	x = nF / 1000
	∴ n = 1000x / F
*/

/*
======================================================================
NTPタイムスタンプ操作用
----------------------------------------------------------------------
*/

/* NTPタイムスタンプ → SYSTEMTIME */
static inline SYSTEMTIME*
ts2st(struct timestamp ts, SYSTEMTIME *st)
{
	time_t t;
	struct tm *tm;

	if (ntohl(ts.integer) < 0x80000000) {
		/* FSB == 0: 2036-02-07 06:28:16 UTC ～ ノーサポート */
		return NULL;
	}else if (ntohl(ts.integer) >= 0x83aa7e80) {
		/* FSB == 1: 1900-01-01 00:00:00 UTC ～ */
		t = ntohl(ts.integer) - 0x83aa7e80;
		if (NULL != (tm = gmtime(&t))) {
			st->wYear = tm->tm_year + 1900;
			st->wMonth = tm->tm_mon + 1;
			st->wDay = tm->tm_mday;
			st->wHour = tm->tm_hour;
			st->wMinute = tm->tm_min;
			st->wSecond = tm->tm_sec;
			st->wMilliseconds = (WORD)((double)(ntohl(ts.fraction)) / 4294967296. * 1000.);
			return st;
		}
	}
	return NULL;
}

/* 現在時刻を NTPタイムスタンプで取得 */
static inline struct timestamp
getnowts()
{
	time_t t;
	struct timestamp ts;
	SYSTEMTIME st;

	time(&t);
	GetSystemTime(&st);

	ts.integer = htonl(t + 0x83aa7e80);
	ts.fraction = htonl((u_long)((double)st.wMilliseconds * 4294967296. / 1000.));
	return ts;
}

/* NTPタイムスタンプの差分 (ミリ秒) */
static inline struct timestamp
tssub(struct timestamp a, struct timestamp b)
{
	struct timestamp r;

	if (ntohl(a.integer) < ntohl(b.integer) ||
			(ntohl(a.integer) == ntohl(b.integer) && ntohl(a.fraction) < ntohl(b.fraction))) {
		struct timestamp t;
		t = a;
		a = b;	/* swap */
		b = t;
	}

	r.integer = htonl(ntohl(a.integer) - ntohl(b.integer));
	r.fraction = htonl(ntohl(a.fraction) - ntohl(b.fraction));
	if (ntohl(a.fraction) < ntohl(b.fraction)) {
		r.integer = htonl(ntohl(r.integer) - 1);
	}

	SYSLOG((LOG_DEBUG, "tssub(): %08lx.%08lx - %08lx.%08lx = %08lx.%08lx",
		ntohl(a.integer), ntohl(a.fraction), ntohl(b.integer), ntohl(b.fraction),
		ntohl(r.integer), ntohl(r.fraction)));
	return r;
}

static inline struct timestamp
tsadd(struct timestamp a, struct timestamp b)
{
	struct timestamp r;

	r.integer = htonl(ntohl(a.integer) + ntohl(b.integer));
	r.fraction = htonl(ntohl(a.fraction) + ntohl(b.fraction));
	if (ntohl(r.fraction) < ntohl(a.fraction) && ntohl(r.fraction) < ntohl(b.fraction)) {
		r.integer = htonl(ntohl(r.integer) + 1);
	}

	SYSLOG((LOG_DEBUG, "tsadd(): %08lx.%08lx + %08lx.%08lx = %08lx.%08lx",
		ntohl(a.integer), ntohl(a.fraction), ntohl(b.integer), ntohl(b.fraction),
		ntohl(r.integer), ntohl(r.fraction)));
	return r;
}

static inline struct timestamp
tsdiv2(struct timestamp a)
{
	struct timestamp r;

	r.fraction = htonl(ntohl(a.fraction) / 2);
	r.integer = htonl(ntohl(a.integer) / 2);
	if (ntohl(a.integer) % 2 == 1) {	/* 切り捨てられたぶんを足す */
		r.fraction = htonl(ntohl(r.fraction) + 2147483648);
	}

	SYSLOG((LOG_DEBUG, "tsdiv2(): %08lx.%08lx / 2 = %08lx.%08lx",
		ntohl(a.integer), ntohl(a.fraction), ntohl(r.integer), ntohl(r.fraction)));
	return r;
}

/* 大小比較  a > b のとき TRUE */
static inline BOOL
tslt(struct timestamp a, struct timestamp b)
{
	if (ntohl(a.integer) > ntohl(b.integer)) {
		return TRUE;
	}else if (ntohl(a.fraction) > ntohl(b.fraction)) {
		return TRUE;
	}
	return FALSE;
}



/*
======================================================================
時刻同期ダイアログ
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
	struct timestamp ts_delay;	/* 往復遅延時間 */
	struct timestamp ts_now;	/* 取得した時刻 */
	struct timestamp ts_set;	/* 設定する時刻 */
	SYSTEMTIME st_now;
	BOOL rtn = TRUE;

	logSync(IDS_NTP_RECVPKTINFO,
		(pkt->li_vn_mode >> 6) & 0x03,	/* Location Information */
		(pkt->li_vn_mode >> 3) & 0x07,	/* Version Number */
		pkt->li_vn_mode & 0x07,		/* Mode */
		pkt->stratum,
		pkt->ppoll,
		pkt->precision);

	if ((pkt->li_vn_mode & 0xc0) == 0xc0) {
		logSync(IDS_NTP_NOTREADY);
		return FALSE;
	}

	/* 往復遅延 d = (T4 - T1) - (T2 - T3) */
	ts_delay = tssub(tssub(getnowts(), pkt->org), tssub(pkt->xmt, pkt->rec));
	logSync(IDS_NTP_DELAY, ntohl(ts_delay.integer), (int)(((double)ntohl(ts_delay.fraction) / 4294967296.) * 1000000));
	if (ts_delay.integer > 0 || (int)(((double)ntohl(ts_delay.fraction) / 4294967296.) * 10000) > syncParam->nMaxDelay * 10) {
		/* 許容遅延時間超過 */
		return FALSE;
	}
	/* 現在時刻 = T3 + d / 2 */
	ts_now = tsadd(pkt->xmt, tsdiv2(ts_delay));

	/* 故意に時間をずらす */
	ts_set.integer = htonl(ntohl(ts_now.integer) + syncParam->nTimeShift);
	ts_set.fraction = ts_now.fraction;

	/* NTPタイムスタンプから SYSTEMTIME に変換 */
	if (NULL == ts2st(ts_set, &st_now)) {
		/* 変換不能。たぶん変なデータ */
		SYSLOG((LOG_ERR, "syncTime(): Couldn't convert timestamp."));
		logSync(IDS_NTP_SYNC_NG);
		rtn = FALSE;
	}else{
		SYSLOG((LOG_DEBUG, "syncTime(): Result Time = %4d-%02d-%02d %d:%02d:%02d.%03d",
			st_now.wYear, st_now.wMonth, st_now.wDay,
			st_now.wHour, st_now.wMinute, st_now.wSecond, st_now.wMilliseconds));
		logSync(IDS_NTP_RECVTIME, st_now.wYear, st_now.wMonth, st_now.wDay,
			st_now.wHour, st_now.wMinute, st_now.wSecond, st_now.wMilliseconds);
		if (syncParam->bSync != FALSE) {
			/* 時刻設定 */
			if (FALSE == SetSystemTime(&st_now)) {
				SYSLOG((LOG_ERR, "syncTime(): SetSystemTime() failed."));
				logSync(IDS_NTP_SYNC_NG);
				rtn = FALSE;
			}else{
				SYSLOG((LOG_DEBUG, "syncTime(): Time syncing success."));
				s_tsPrevSync = ts_now;
				logSync(IDS_NTP_SYNC_OK);
			}
		}else{
			logSync(IDS_NTP_CHECK_OK);
			SYSLOG((LOG_DEBUG, "syncTime(): Sync test success."));
		}
	}
	return rtn;
}

/* 応答パケット受信処理 */
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

/* SNTPリクエストパケットを送信する */
static BOOL
sendNtpPacket(ADDRINFO *addr)
{
	struct pkt pkt;
	BOOL rtn = TRUE;
	int i;

	/* リクエストパケットの設定 */
	memset(&pkt, 0, sizeof(struct pkt));
	pkt.li_vn_mode = 0x1B;		/* VN=3, Mode=3(Client) */
	//pkt.li_vn_mode = 0x23;	/* VN=4, Mode=3(Client) */
	pkt.xmt = getnowts();		/* 現在時刻 T1 */

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

/* NTPセッションを開始する */
static SOCKET
initNtpSession(ADDRINFO *addr)
{
	const BOOL on = TRUE;

	/* ソケット生成 */
	if (INVALID_SOCKET == (s_sockSync = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol))) {
		logSync(IDS_NTP_INIT_NG);
	}else{

		/* ブロードキャスト有効 */
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

/* セッションに関する後始末 */
static void
endNtpSession()
{
	KillTimer(s_hwndSyncDlg, ID_TIMER_SYNC);

	/* 非同期GetAddrInfo()ハンドルが残っていれば停止 */
	if (s_hGetHost != 0) {
		//WSACancelAsyncRequest(s_hGetHost);
		CloseHandle(s_hGetHost);
		SYSLOG((LOG_DEBUG, "endNtpSession(): Canceled async request handle=%d", s_hGetHost));
		s_hGetHost = 0;
	}

	/* ソケットの開放 */
	if (s_sockSync != INVALID_SOCKET) {
		closesocket(s_sockSync);
		SYSLOG((LOG_DEBUG, "endNtpSession(): Closed socket=%d", s_sockSync));
		s_sockSync = INVALID_SOCKET;
	}

	return;
}

/* ダイアログの処理終了 (ダイアログ表示時は「閉じる」ボタン、非表示時は終了) */
static void
endSyncDialog()
{
	if (FALSE == IsWindowVisible(s_hwndSyncDlg)) {
		DestroyWindow(s_hwndSyncDlg);
		s_hwndSyncDlg = 0;
	}else{
		SetDlgItemText(s_hwndSyncDlg, IDCANCEL, "閉じる");
	}

	return;
}

BOOL CALLBACK
SyncDlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	static char szHostName[INTERNET_MAX_HOST_NAME_LENGTH];
	static ADDRINFO aiHints;
	static ADDRINFO *pHost;
	static cntRetry;	/* リトライカウント */
	static sync_param args;

	WSADATA	wsaData;
	struct pkt pkt;

//	SYSLOG((LOG_DEBUG, "SyncDlgProc(): Received Window Message %04x", uMsg));

	/* 終了したダイアログへの残存メッセージはWindowsに任せる */
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
		/* 非表示の場合はフォーカスを奪わない */
		if (FALSE == IsWindowVisible(hwndDlg)) {
			return FALSE;
		}
		break;

	/* 初期化 */
	case WM_SYNC_INIT:
		SYSLOG((LOG_DEBUG, "SyncDlgProc(): Received WM_SYNC_INIT"));
		if (0 != WSAStartup(MAKEWORD(1, 1), &wsaData)) {	/* バージョン 1.1 を要求 */
			SYSLOG((LOG_ERR, "SyncDlgProc(): WSAStartup() failed."));
			logSync(IDS_NTP_INIT_NG);
			endSyncDialog();
		}else{
			SYSLOG((LOG_ERR, "SyncDlgProc(): WSAStartup() success."));
		}
		break;

	/* 時刻同期処理開始 */
	case WM_SYNC_START:
		SYSLOG((LOG_DEBUG, "SyncDlgProc(): Received WM_SYNC_START"));
		memcpy(&args, (void *)lParam, sizeof(struct sync_param));

		/* WinSock初期化 */
		SetDlgItemText(hwndDlg, IDCANCEL, "ｷｬﾝｾﾙ");
		endNtpSession();

		/* 非同期GetAddrInfo()発行 */
		memset(&aiHints, 0, sizeof(ADDRINFO));
		aiHints.ai_family = PF_UNSPEC;		/* プロトコルファミリ: 未指定(IPv4/v6) */
		aiHints.ai_socktype = SOCK_DGRAM;	/* ソケットタイプ: UDP */
		strcpy(szHostName, args.szServer);
		if (0 == (s_hGetHost = WSAAsyncGetAddrInfo(hwndDlg, WM_SYNC_GETHOST, szHostName, "123", &aiHints, &pHost))) {
			SYSLOG((LOG_ERR, "SyncDlgProc(): WSAsyncGetAddrInfo() failed."));
			logSync(IDS_NTP_INIT_NG);
			endSyncDialog();
		}else{
			SYSLOG((LOG_DEBUG, "SyncDlgProc(): WSASyncGetAddrInfo() success. hGetHost=%d", s_hGetHost));
			logSync(IDS_NTP_RESOLVING, szHostName);
		}
		break;

	/* gethostbyname()応答 */
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

	/* select()応答 */
	case WM_SYNC_SELECT:
		SYSLOG((LOG_DEBUG, "SyncDlgProc(): Received WM_SYNC_SELECT"));
		/* 処理すべき socket の判定 */
		if((SOCKET)wParam == s_sockSync){
			/* 受信イベント */
			if (FD_READ == WSAGETSELECTEVENT(lParam)) {
				if (FALSE != recvNtpPacket(&pkt)) {
					if (FALSE == syncTime(&pkt, &args)) {
						/* 受信を継続する */
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
		/* タイムアウトタイマ */
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
				/* リトライオーバー */
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
時刻同期 (エントリ)
----------------------------------------------------------------------
*/
BOOL
syncClock(
	HWND	hWnd,		/* 親ウィンドウハンドル */
	struct sync_param *sa,	/* 時刻同期パラメタ */
	BOOL	bShow		/* ダイアログを表示するかどうか */
)
{
	HWND hwndDlg;

	if (!IsWindow(s_hwndSyncDlg)) {
		//EnableWindow(hWnd, FALSE);
		hwndDlg = CreateDialog(g_hInst, MAKEINTRESOURCE(IDD_SYNC), hWnd, SyncDlgProc);
	}else{
		hwndDlg = s_hwndSyncDlg;
		/* 既に起動している */
		SYSLOG((LOG_DEBUG, "syncClock(): Sync dialog already running"));
		/* ウィンドウ表示中の場合は何もしない */
		if (FALSE != IsWindowVisible(s_hwndSyncDlg)) {
			return FALSE;
		}
	}
	/* ダイアログを表示する */
	if (FALSE != bShow) {
		ShowWindow(hwndDlg, SW_SHOW);
	}
	SendMessage(hwndDlg, WM_SYNC_START, (WPARAM)0, (LPARAM)sa);

	return TRUE;
}

char *
getLastSync()
{
	static char szPrevSync[18];
	SYSTEMTIME st;

	if (s_tsPrevSync.integer != 0 && s_tsPrevSync.fraction != 0) {
		if (NULL != ts2st(s_tsPrevSync, &st)) {
			wsprintf(szPrevSync, "%4d-%02d-%02d %2d:%02d:%02d",
				st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
			return szPrevSync;
		}
	}

	return NULL;
}

