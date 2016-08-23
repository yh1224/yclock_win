#include <windows.h>
#include <wininet.h>
#include <shlobj.h>
#include <stdio.h>
#include <math.h>
#include "ident.h"
#include "resource.h"
#include "sntp.h"
#include "debug.h"
#include "voice.h"
#include "conf.h"
#include "misc.h"

/* タスクトレイ再構築メッセージ (Win98以降もしくはIE4 Desktopがインストールされている場合 */
static const UINT WM_TASKBARCREATED = RegisterWindowMessage(TEXT("TaskbarCreated"));

/* タスクトレイ通知用メッセージ */
#define WM_TASKTRAY (WM_APP + 100)

#if 0 /* ウィンドウ半透明化を使う */
typedef BOOL __stdcall API_SetLayeredWindowAttributes(HWND hwnd, COLORREF crKey, BYTE bAlpha, DWORD dwFlags);
static API_SetLayeredWindowAttributes *SetLayeredWindowAttributes;
#endif



HINSTANCE g_hInst;		/* アプリケーションのインスタンス */

/* Preload Resources */
static HMENU	s_hMenu;		/* メニュー */
static HICON	s_hIconTray;	/* タスクトレイ用アイコン */
#ifdef DISPWND
static HBITMAP	s_hbmIllust;	/* ウィンドウ用ビットマップ */
static HBITMAP	s_hbmMonth;		/* ウィンドウ用ビットマップ */
#endif



/* 予期しないエラー */
void
EXCEPTION()
{
	MessageBox(NULL,
		"予期しない致命的な例外により終了します。\n\n"
		"メモリ不足もしくはシステム異常が考えられます。\n"
		"再現性のある場合は作者までお問い合わせください。", IDENT_APLTITLE, MB_OK | MB_ICONSTOP);
	exit(EXIT_FAILURE);
}



/*
======================================================================
バージョン情報ダイアログ
----------------------------------------------------------------------
*/
BOOL CALLBACK
AboutDlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	char szMsg[64];

	switch (uMsg) {
	case WM_INITDIALOG:
		centeringWndPos(hwndDlg);
		SetDlgItemText(hwndDlg, IDC_ABOUT_TITLE, IDENT_APLTITLE);
		SetDlgItemText(hwndDlg, IDC_ABOUT_VERSION, IDENT_APLVERSION);
		if (NULL != getLastSync()) {
			wsprintf(szMsg, "%s に時刻同期しました。", getLastSync());
			SetDlgItemText(hwndDlg, IDC_ABOUT_PREVSYNC, szMsg);
		}
		return 1;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDC_ABOUT_HOMEPAGE:
			/* ホームページに飛ぶ */
			ShellExecute(NULL, "open", IDENT_SUPPORTURL, NULL, NULL, SW_SHOWNORMAL);
			break;
		case IDOK:
		case IDCANCEL:
			EndDialog(hwndDlg, 0);
	 	}
		return 1;
	}
	return 0;
}

BOOL
trayIcon(HWND hWnd, DWORD dwMessage)
{
	static NOTIFYICONDATA nIcon;
	SYSTEMTIME st;
	char szTip[64];

	GetLocalTime(&st);
	wsprintf(szTip, "%d/%d/%d %d:%02d", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute);

	nIcon.cbSize = sizeof(NOTIFYICONDATA);
	nIcon.uID = 1;
	nIcon.hWnd = hWnd;
	nIcon.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
	nIcon.hIcon = s_hIconTray;
	nIcon.uCallbackMessage = WM_TASKTRAY;
	lstrcpy(nIcon.szTip, szTip);
	//lstrcpy(nIcon.szTip, IDENT_APLTITLE);

	return Shell_NotifyIcon(dwMessage, &nIcon);
}

/*
======================================================================
メイン プロシジャ
----------------------------------------------------------------------
*/

#ifdef DISPWND
BOOL
drawTime(HDC hdc, double p, LONG x, LONG y, int r, int d, int r2) {
	BOOL rtn = FALSE;
	RECT rect;
	HPEN hpOld;
	HBRUSH hb, hbOld;

	rect.left = x	+ (LONG)(sin(p * 2 * 3.14) * (r - d)) - r2 / 2;
	rect.top = y - (LONG)(cos(p * 2 * 3.14) * (r - d)) - r2 / 2;
	rect.right = rect.left + r2;
	rect.bottom = rect.top + r2;
	//SYSLOG((LOG_DEBUG, "Long hand (%d, %d) - (%d, %d)", rect.left, rect.top, rect.right, rect.bottom));
	if (NULL == (hpOld = (HPEN)SelectObject(hdc, (HPEN)GetStockObject(WHITE_PEN)))) {
		SYSLOG((LOG_ERR, "SelectObject() failed."));
	}else{
		if (NULL == (hb = CreateSolidBrush(RGB(0, 204, 51)))) {
			SYSLOG((LOG_ERR, "CreateSolidBrush() failed."));
		}else{
			if (NULL == (hbOld = (HBRUSH)SelectObject(hdc, hb))) {
				SYSLOG((LOG_ERR, "SelectObject() failed."));
			}else{
				Ellipse(hdc, rect.left, rect.top, rect.right, rect.bottom);
				SelectObject(hdc, hbOld);
				rtn = TRUE;
			}
			DeleteObject(hb);
		}
		SelectObject(hdc, hpOld);
	}
	return rtn;
}
#endif

/* Timer ID */
#define ID_TIMER_VOICE		1
#define ID_TIMER_SYNC		2

LRESULT CALLBACK
WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	static int nTimer = -1;	/* タイマーのカウント */

	SYSTEMTIME time;	/* 現在時刻 */
	struct sync_param syncParam;
	POINT point;
#ifdef DISPWND
	PAINTSTRUCT ps;
	HDC hdc, hdcMem;	/* ウィンドウのDCと一時DC */
	BITMAP bm;
	HBITMAP hbmOld;
	RECT rectClock;		/* 時刻表示リージョン */
	static HRGN hrgnWnd;	/* 生成したウィンドウリージョン (要解放) */
	HRGN hRgn;
	HBRUSH hbWhite, hbOrange;
#endif

	/* 現在時刻の取得 */
	GetLocalTime(&time);

#ifdef DISPWND
	/* 時計表示領域 */
	rectClock.left = 16;
	rectClock.top = 110;
	rectClock.right = rectClock.left + 58;
	rectClock.bottom = rectClock.top + 58;
#endif

	if (WM_TASKBARCREATED != 0 && message == WM_TASKBARCREATED) {
		/* タスクバーが再構築された場合、タスクトレイにアイコンを再登録 */
		trayIcon(hWnd, NIM_ADD);
	}

	switch (message) {
	case WM_CREATE:
#ifdef DISPWND
		/* イメージ読み込み */
		if (0 == GetObject(s_hbmIllust, sizeof(bm), &bm)) {
			SYSLOG((LOG_ERR, "GetObject() failed."));
			return -1;
		}
		SetWindowPos(hWnd, NULL, 0, 0, bm.bmWidth, bm.bmHeight, SWP_NOMOVE | SWP_NOZORDER);

#if 1 /* ビットマップリージョン */
 		if (NULL != (hrgnWnd = createRgnFromBmp(s_hbmIllust))) {
#else /* 円形リージョン */
		RECT rect;
		GetClientRect(hWnd, &rect);
		if (NULL != (hrgnWnd = CreateEllipticRgn(0, 0, rect.right, rect.bottom))) {
#endif
			/* 作成したリージョンに、時刻表示領域を追加 */
			CombineRgn(hrgnWnd, hrgnWnd, CreateEllipticRgnIndirect(&rectClock), RGN_OR);
			SetWindowRgn(hWnd, hrgnWnd, TRUE);
		}/* 失敗した場合は、まぁそのままでいいか… */

		/* 画面内に移動 */
		fixWndPos(hWnd);
#endif

		/* タスクトレイに追加 */
		trayIcon(hWnd, NIM_ADD);

		/* タイマを生成 */
		SetTimer(hWnd, ID_TIMER_VOICE, 60000 - (time.wSecond * 1000 + time.wMilliseconds), NULL);
		SetTimer(hWnd, ID_TIMER_SYNC, 1000, NULL);	// 1秒後に初回同期

		break;

#ifdef DISPWND
	case WM_PAINT:
		if (NULL == (hdc = BeginPaint(hWnd, &ps))) {
			SYSLOG((LOG_ERR, "BeginPaint() failed."));
		}else{
			/* イラストの描画 */
			if (NULL == (hdcMem = CreateCompatibleDC(hdc))) {
				SYSLOG((LOG_ERR, "CreateCompatibleDC() failed."));
			}else{
				if (0 == GetObject(s_hbmIllust, sizeof(bm), &bm)) {
					SYSLOG((LOG_ERR, "GetObject() failed."));
				}else{
					if (NULL == (hbmOld = (HBITMAP)SelectObject(hdcMem, s_hbmIllust))) {
						SYSLOG((LOG_ERR, "SelectObject() failed."));
					}else{
						/*if (0 == (RC_BITBLT & GetDeviceCaps(hdc, RASTERCAPS))) {
							SYSLOG((LOG_ERR, "DC not support BitBlt()."));
						}else*/
						if (FALSE == BitBlt(hdc, 0, 0, bm.bmWidth, bm.bmHeight, hdcMem, 0, 0, SRCCOPY)) {
#ifdef _DEBUG
							char *msg;
							FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER,
								NULL, GetLastError(), 0, (LPTSTR)&msg, 0, NULL);
							SYSLOG((LOG_ERR, msg));
							LocalFree(msg);
#endif
						}
						SelectObject(hdcMem, hbmOld);
					}
				}

				/* 時刻の描画 */
				if (NULL == (hbWhite = CreateSolidBrush(RGB(255, 255, 255)))) {		/* 白 */
					SYSLOG((LOG_ERR, "CreateSolidBrush() failed."));
				}else{
					if (NULL == (hRgn = CreateEllipticRgnIndirect(&rectClock))) {
						SYSLOG((LOG_ERR, "CreateEllipticRgnIndirect() failed."));
					}else{
						FillRgn(hdc, hRgn, hbWhite);
						DeleteObject(hRgn);
					}
					DeleteObject(hbWhite);
				}

				if (NULL == (hbOrange = CreateSolidBrush(RGB(255, 153, 0)))) {	/* オレンジ */
					SYSLOG((LOG_ERR, "CreateSolidBrush() failed."));
				}else{
					if (NULL == (hRgn = CreateEllipticRgn(rectClock.left + 2, rectClock.top + 2, rectClock.right - 2, rectClock.bottom - 2))) {
						SYSLOG((LOG_ERR, "CreateEllipticRgn() failed."));
					}else{
						FillRgn(hdc, hRgn, hbOrange);
						DeleteObject(hRgn);
					}
					DeleteObject(hbOrange);
				}

				/* 月 */
				hbmOld = (HBITMAP)SelectObject(hdcMem, s_hbmMonth);
				BitBlt(hdc, (rectClock.right + rectClock.left) / 2 - 16, (rectClock.bottom + rectClock.top) / 2 - 16
					, 32, 32, hdcMem, (time.wMonth - 1) % 12 * 32, 0, SRCCOPY);
				SelectObject(hdcMem, hbmOld);

				/* 短針(m) */
				drawTime(hdc, (double)time.wHour / 12.0 + (double)time.wMinute / 720.0
					, (rectClock.right + rectClock.left) / 2, (rectClock.bottom + rectClock.top) / 2
					, (rectClock.right - rectClock.left) / 2, 8, 11);
#if 0
				/* 長針(m) */
				drawTime(hdc, (double)time.wMinute / 60.0
					, (rectClock.right + rectClock.left) / 2, (rectClock.bottom + rectClock.top) / 2
					, (rectClock.right - rectClock.left) / 2, 8, 11);
#endif
				DeleteDC(hdcMem);
			}
			EndPaint(hWnd, &ps);
		}
		break;

#if 1 /* ダブルクリックで時刻読み上げ */
	case WM_LBUTTONDBLCLK:
	case WM_NCLBUTTONDBLCLK:
		{
		HCURSOR hCursor = SetCursor(LoadCursor(NULL, IDC_WAIT));
		playCurrentTime(g_Conf.nVoiceVolume);
		SetCursor(hCursor);
		}
		break;
#endif

	case WM_RBUTTONDOWN:
	case WM_NCRBUTTONDOWN:
		GetCursorPos(&point);
		TrackPopupMenu(s_hMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN | TPM_RIGHTBUTTON, point.x, point.y, 0, hWnd, NULL );
		break;

	case WM_NCHITTEST:
		wParam = DefWindowProc(hWnd, message, wParam, lParam);
		if (wParam == HTCLIENT) {
			return HTCAPTION;
		}else{
			return wParam;
		}
#endif

	case WM_TIMER:
		if (wParam == ID_TIMER_VOICE) {
			//KillTimer(hWnd, ID_TIMER_VOICE);
			if (time.wSecond < 10) {
#ifdef DISPWND
				/* 時刻表示更新 */
				InvalidateRect(hWnd, &rectClock, TRUE);
				UpdateWindow(hWnd);
#endif
				/* タスクトレイメッセージ変更 */
				trayIcon(hWnd, NIM_MODIFY);

				/* 時刻読み上げ */
				if (0 != g_Conf.nVoiceInterval) {
					if (time.wMinute % g_Conf.nVoiceInterval == 0) {
						playCurrentTime(g_Conf.nVoiceVolume);
					}
				}
			}

			/* タイマーの再設定 */
			GetLocalTime(&time);	/* 再取得 */
			SetTimer(hWnd, ID_TIMER_VOICE, 60000 - (time.wSecond * 1000 + time.wMilliseconds), NULL);

		}else if (wParam == ID_TIMER_SYNC) {
			//KillTimer(hWnd, ID_TIMER_SYNC);
			if (FALSE != g_Conf.bSync) {
				/* インターネット接続の検出 */
				DWORD dwFlags;
				if (FALSE == InternetGetConnectedState(&dwFlags, 0)) {
					nTimer = -1;
					SYSLOG((LOG_DEBUG, "InternetGetConnectedState() failed."));
				}else{
					SYSLOG((LOG_DEBUG, "InternetGetConnectedState(): %d", dwFlags));
					/* 時刻同期 */
					nTimer++;
					if (nTimer == 0 || nTimer >= (int)g_Conf.nSyncInterval) {
						memset(&syncParam, 0, sizeof(syncParam));
						syncParam.bSync = TRUE;
						syncParam.szServer = g_Conf.szServer;
						syncParam.nMaxDelay = g_Conf.nMaxDelay;
						syncParam.nTolerance = g_Conf.nTolerance;
						syncParam.nTimeShift = g_Conf.nTimeShift;
						syncClock(hWnd, &syncParam, FALSE);
						nTimer = 0;
					}
					SYSLOG((LOG_DEBUG, "Timer counts: %d", nTimer));
				}
			}

			/* タイマーの再設定 */
			SetTimer(hWnd, ID_TIMER_SYNC, 60000, NULL);
		}
		break;

	case WM_TASKTRAY:		/* トレイイベント */
		switch (lParam) {
		case WM_LBUTTONDOWN:	/* 左ボタン押下 */
			/* 時計ウィンドウが常に最前面表示にしていなければ最前面に表示 */
			if (0 == (WS_EX_TOPMOST & GetWindowLong(hWnd, GWL_EXSTYLE))) {
				SetForegroundWindow(hWnd);
			}
			break;
		case WM_RBUTTONDOWN:	/* 右ボタン押下 */
			SetForegroundWindow(hWnd);
			/* メニュー表示 */
			GetCursorPos(&point);
			TrackPopupMenu(s_hMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN, point.x, point.y, 0, hWnd, NULL );
			PostMessage(hWnd, WM_NULL, 0, 0);/* MSDN:Q135788 */
			break;
		case WM_LBUTTONDBLCLK:	/* 左ボタンダブルクリック */
#if 1 /* ウィンドウ表示切り替え */
#ifdef DISPWND
			/* 表示/非表示の切り替え */
			if (FALSE == IsWindowVisible(hWnd)) {
				ShowWindow(hWnd, SW_SHOW);
				CheckMenuItem(s_hMenu, ID_MENU_VIEW, MF_CHECKED);
				EnableMenuItem(s_hMenu, ID_MENU_ONTOP, MF_ENABLED);
			}else{
				ShowWindow(hWnd, SW_HIDE);
				CheckMenuItem(s_hMenu, ID_MENU_VIEW, MF_UNCHECKED);
				EnableMenuItem(s_hMenu, ID_MENU_ONTOP, MF_GRAYED);
			}
#endif
#else /* 現在時刻読み上げ */
			hCursor = SetCursor(LoadCursor(NULL, IDC_WAIT));
			playCurrentTime(g_Conf.nVoiceVolume);
			SetCursor(hCursor);
#endif
			break;
		}
		break;

	case WM_CLOSE:			/* ウィンドウを閉じる */
	case WM_ENDSESSION:		/* Winwdows終了 */
#ifdef DISPWND
		conf_setWnd(hWnd);
#endif
		conf_save();	/* ENDSESSION対策: とりあえずここでsaveしちゃう */
		return DefWindowProc(hWnd, message, wParam, lParam);

	case WM_DESTROY:		/* ウィンドウ終了 */
#ifdef DISPWND
		if (hrgnWnd != NULL) {
			DeleteObject(hrgnWnd);
		}
#endif
		KillTimer(hWnd, ID_TIMER_VOICE);
		KillTimer(hWnd, ID_TIMER_SYNC);
		trayIcon(hWnd, NIM_DELETE);
		PostQuitMessage(0);
		break;

	case WM_COMMAND:		/* メニュー選択 */
		switch (LOWORD(wParam)) {
#ifdef DISPWND
		case ID_MENU_VIEW:		/* 時計表示 */
			/* 表示/非表示の切り替え */
			if (FALSE == IsWindowVisible(hWnd)) {
				ShowWindow(hWnd, SW_SHOW);
				CheckMenuItem(s_hMenu, ID_MENU_VIEW, MF_CHECKED);
				EnableMenuItem(s_hMenu, ID_MENU_ONTOP, MF_ENABLED);
			}else{
				ShowWindow(hWnd, SW_HIDE);
				CheckMenuItem(s_hMenu, ID_MENU_VIEW, MF_UNCHECKED);
				EnableMenuItem(s_hMenu, ID_MENU_ONTOP, MF_GRAYED);
			}
			break;

		case ID_MENU_ONTOP:		/* 最前面に表示 */
			LONG wsex;
			/* 通常/最前面表示の切り替え */
			if (0 == (WS_EX_TOPMOST & (wsex = GetWindowLong(hWnd, GWL_EXSTYLE)))) {
				SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
				CheckMenuItem(s_hMenu, ID_MENU_ONTOP, MF_CHECKED);
			}else{
				SetWindowPos(hWnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
				CheckMenuItem(s_hMenu, ID_MENU_ONTOP, MF_UNCHECKED);
			}
			break;
#endif
#if 0
		case ID_MENU_SYNC:		/* 時刻合わせ */
			/* 時刻同期実行 */
			memset(&syncParam, 0, sizeof(syncParam));
			syncParam.bSync = TRUE;
			syncParam.szServer = g_Conf.szServer;
			syncParam.nMaxDelay = g_Conf.nMaxDelay;
			syncParam.nTolerance = g_Conf.nTolerance;
			syncParam.nTimeShift = g_Conf.nTimeShift;
			syncClock(hWnd, &syncParam, TRUE);
			break;
#endif

		case ID_MENU_CONFIG:	/* 設定 */
			/* 設定ダイアログ表示 */
			DialogBox(g_hInst, MAKEINTRESOURCE(IDD_CONFIG), hWnd, ConfigDlgProc);
			break;

		case ID_MENU_ABOUT:		/* 情報 */
			/* 情報ダイアログ表示 */
			DialogBox(g_hInst, MAKEINTRESOURCE(IDD_ABOUT), hWnd, AboutDlgProc);
			break;

		case ID_MENU_EXIT:		/* 終了 */
#ifdef DISPWND
			conf_setWnd(hWnd);
#endif
			DestroyWindow(hWnd);
			break;
	 	}
		break;

	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}



/*
======================================================================
メイン関数
----------------------------------------------------------------------
*/
int WINAPI
WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pszArg, int nShowCmd)
{
	HANDLE hMutex;		/* アプリケーションのMutex(重複起動阻止用) */
	HWND hWnd;
	HMENU hMenu;
	WNDCLASSEX wcl;
	MSG msg;
	HICON hIcon;
#if 0
	HINSTANCE hDllInst;
#endif

	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(pszArg);
	UNREFERENCED_PARAMETER(nShowCmd);

	g_hInst = hInstance;

	/* 重複起動阻止 */
	if (NULL == (hMutex = CreateMutex(NULL, FALSE, IDENT_MUTEXOBJECTNAME))) {
		SYSLOG((LOG_ERR, "CreateMutex() failed."));
		EXCEPTION();
	}
	if (ERROR_ALREADY_EXISTS == GetLastError()) {
		CloseHandle(hMutex);
		/* 既存のウィンドウをアクティブ化 */
		if (NULL != (hWnd = FindWindow(IDENT_APLTITLE, IDENT_APLTITLE))) {
			SetForegroundWindow(hWnd);
		}
		return 0;
	}


	/* リソースのロード */
	if (NULL == (hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_NORMAL)))) {
		SYSLOG((LOG_ERR, "Failed to load resource: IDI_NORMAL"));
		EXCEPTION();
	}
	if (NULL == (s_hIconTray = (HICON)LoadImage(hInstance, MAKEINTRESOURCE(IDI_NORMAL), IMAGE_ICON, 16, 16, 0))) {
		SYSLOG((LOG_ERR, "Failed to load resource: IDI_NORMAL"));
		EXCEPTION();
	}
#ifdef DISPWND
	if (NULL == (s_hbmIllust = LoadBitmap(hInstance, MAKEINTRESOURCE(IDB_ILLUST)))) {
		SYSLOG((LOG_ERR, "Failed to load resource: IDB_ILLUST"));
		EXCEPTION();
	}
	if (NULL == (s_hbmMonth = LoadBitmap(hInstance, MAKEINTRESOURCE(IDB_MONTH)))) {
		SYSLOG((LOG_ERR, "Failed to load resource: IDB_MONTH"));
		EXCEPTION();
	}
#endif
	if (NULL != (hMenu = LoadMenu(hInstance, MAKEINTRESOURCE(IDR_MENU1)))) {
		s_hMenu = GetSubMenu(hMenu, 0);
	}
	if (s_hMenu == NULL) {
		SYSLOG((LOG_ERR, "Failed to load menu."));
		EXCEPTION();
	}

#if 0
	/* ライブラリに定義されていないAPIへのポインタを取得 */
	if (NULL != (hDllInst = LoadLibrary("user32.dll"))) {
		SetLayeredWindowAttributes = (API_SetLayeredWindowAttributes *)GetProcAddress(hDllInst, "SetLayeredWindowAttributes");
		if (SetLayeredWindowAttributes == NULL) {
			SYSLOG((LOG_INFO, "SetLayeredWindowAttributes() not found in user32.dll"));
		}
	}else{
		SetLayeredWindowAttributes = NULL;
		SYSLOG((LOG_INFO, "Load library failed: user32.dll"));
	}
#endif

	ZeroMemory(&wcl, sizeof(wcl));
	wcl.hInstance = hInstance;
	wcl.lpszClassName = IDENT_APLTITLE;
	wcl.lpfnWndProc = WndProc;
	wcl.style = 0;
	wcl.hIcon = hIcon;
	wcl.hIconSm = s_hIconTray;
	wcl.hCursor = LoadCursor(NULL,IDC_ARROW);
	wcl.lpszMenuName = NULL;
	wcl.cbClsExtra = wcl.cbWndExtra = 0;
	wcl.hbrBackground = (HBRUSH)GetStockObject(LTGRAY_BRUSH);
	wcl.cbSize = sizeof(WNDCLASSEX);
	if (0 == RegisterClassEx(&wcl)) {
		SYSLOG((LOG_ERR, "Failed to register class."));
		EXCEPTION();
	}

	if (FALSE == conf_load()) {
		SYSLOG((LOG_ERR, "Failed to load configuration from registry."));
		EXCEPTION();
	}

	SYSLOG((LOG_DEBUG, "Startup."));

#ifdef DISPWND
	hWnd = CreateWindowEx(
#if 0
		WS_EX_TOOLWINDOW | 0x00080000L/*WS_EX_LAYERED*/,
#else
		WS_EX_TOOLWINDOW,
#endif
		/* WS_EX_APPWINDOW | WS_EX_TOPMOST,*/	/* キャプチャするとき */
		IDENT_APLTITLE,
		IDENT_APLTITLE,
		WS_POPUP,
		g_Conf.nPosX,
		g_Conf.nPosY,
		0,
		0,
		HWND_DESKTOP,
		NULL,
		hInstance,
		NULL
	);
	if (hWnd == NULL) {
		SYSLOG((LOG_ERR, "Can't create window."));
		EXCEPTION();
	}
#if 0
	if (SetLayeredWindowAttributes != NULL) {
		SetLayeredWindowAttributes(hWnd, 0, 255, 2);
	}
#endif
	if (FALSE != g_Conf.bOnTop) {
		SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
		CheckMenuItem(s_hMenu, ID_MENU_ONTOP, MF_CHECKED);
	}
	if (FALSE == g_Conf.bShow) {
		ShowWindow(hWnd, SW_HIDE);
		EnableMenuItem(s_hMenu, ID_MENU_ONTOP, MF_GRAYED);
	}else{
		ShowWindow(hWnd, SW_SHOW);
		CheckMenuItem(s_hMenu, ID_MENU_VIEW, MF_CHECKED);
	}
	UpdateWindow(hWnd);
#else /* DISPWND */
	hWnd = CreateWindowEx(
		WS_EX_TOOLWINDOW,
		IDENT_APLTITLE,
		IDENT_APLTITLE,
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		HWND_DESKTOP,
		NULL,
		hInstance,
		NULL
	);
	ShowWindow(hWnd, SW_HIDE);
	UpdateWindow(hWnd);
#endif /* DISPWND */

	while (FALSE != GetMessage(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

#ifdef DISPWND
	DeleteObject(s_hbmMonth);
	DeleteObject(s_hbmIllust);
#endif
	DestroyMenu(s_hMenu);

	conf_save();
#if 0
	FreeLibrary(hDllInst);
#endif

	ReleaseMutex(hMutex);

	SYSLOG((LOG_DEBUG, "Exit."));
	SYSLOG_CLOSE();

	return msg.wParam;
}
