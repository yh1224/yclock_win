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

/* �^�X�N�g���C�č\�z���b�Z�[�W (Win98�ȍ~��������IE4 Desktop���C���X�g�[������Ă���ꍇ */
static const UINT WM_TASKBARCREATED = RegisterWindowMessage(TEXT("TaskbarCreated"));

/* �^�X�N�g���C�ʒm�p���b�Z�[�W */
#define WM_TASKTRAY (WM_APP + 100)

#if 0 /* �E�B���h�E�����������g�� */
typedef BOOL __stdcall API_SetLayeredWindowAttributes(HWND hwnd, COLORREF crKey, BYTE bAlpha, DWORD dwFlags);
static API_SetLayeredWindowAttributes *SetLayeredWindowAttributes;
#endif



HINSTANCE g_hInst;		/* �A�v���P�[�V�����̃C���X�^���X */

/* Preload Resources */
static HMENU	s_hMenu;		/* ���j���[ */
static HICON	s_hIconTray;	/* �^�X�N�g���C�p�A�C�R�� */
#ifdef DISPWND
static HBITMAP	s_hbmIllust;	/* �E�B���h�E�p�r�b�g�}�b�v */
static HBITMAP	s_hbmMonth;		/* �E�B���h�E�p�r�b�g�}�b�v */
#endif



/* �\�����Ȃ��G���[ */
void
EXCEPTION()
{
	MessageBox(NULL,
		"�\�����Ȃ��v���I�ȗ�O�ɂ��I�����܂��B\n\n"
		"�������s���������̓V�X�e���ُ킪�l�����܂��B\n"
		"�Č����̂���ꍇ�͍�҂܂ł��₢���킹���������B", IDENT_APLTITLE, MB_OK | MB_ICONSTOP);
	exit(EXIT_FAILURE);
}



/*
======================================================================
�o�[�W�������_�C�A���O
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
			wsprintf(szMsg, "%s �Ɏ����������܂����B", getLastSync());
			SetDlgItemText(hwndDlg, IDC_ABOUT_PREVSYNC, szMsg);
		}
		return 1;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDC_ABOUT_HOMEPAGE:
			/* �z�[���y�[�W�ɔ�� */
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
���C�� �v���V�W��
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
	static int nTimer = -1;	/* �^�C�}�[�̃J�E���g */

	SYSTEMTIME time;	/* ���ݎ��� */
	struct sync_param syncParam;
	POINT point;
#ifdef DISPWND
	PAINTSTRUCT ps;
	HDC hdc, hdcMem;	/* �E�B���h�E��DC�ƈꎞDC */
	BITMAP bm;
	HBITMAP hbmOld;
	RECT rectClock;		/* �����\�����[�W���� */
	static HRGN hrgnWnd;	/* ���������E�B���h�E���[�W���� (�v���) */
	HRGN hRgn;
	HBRUSH hbWhite, hbOrange;
#endif

	/* ���ݎ����̎擾 */
	GetLocalTime(&time);

#ifdef DISPWND
	/* ���v�\���̈� */
	rectClock.left = 16;
	rectClock.top = 110;
	rectClock.right = rectClock.left + 58;
	rectClock.bottom = rectClock.top + 58;
#endif

	if (WM_TASKBARCREATED != 0 && message == WM_TASKBARCREATED) {
		/* �^�X�N�o�[���č\�z���ꂽ�ꍇ�A�^�X�N�g���C�ɃA�C�R�����ēo�^ */
		trayIcon(hWnd, NIM_ADD);
	}

	switch (message) {
	case WM_CREATE:
#ifdef DISPWND
		/* �C���[�W�ǂݍ��� */
		if (0 == GetObject(s_hbmIllust, sizeof(bm), &bm)) {
			SYSLOG((LOG_ERR, "GetObject() failed."));
			return -1;
		}
		SetWindowPos(hWnd, NULL, 0, 0, bm.bmWidth, bm.bmHeight, SWP_NOMOVE | SWP_NOZORDER);

#if 1 /* �r�b�g�}�b�v���[�W���� */
 		if (NULL != (hrgnWnd = createRgnFromBmp(s_hbmIllust))) {
#else /* �~�`���[�W���� */
		RECT rect;
		GetClientRect(hWnd, &rect);
		if (NULL != (hrgnWnd = CreateEllipticRgn(0, 0, rect.right, rect.bottom))) {
#endif
			/* �쐬�������[�W�����ɁA�����\���̈��ǉ� */
			CombineRgn(hrgnWnd, hrgnWnd, CreateEllipticRgnIndirect(&rectClock), RGN_OR);
			SetWindowRgn(hWnd, hrgnWnd, TRUE);
		}/* ���s�����ꍇ�́A�܂����̂܂܂ł������c */

		/* ��ʓ��Ɉړ� */
		fixWndPos(hWnd);
#endif

		/* �^�X�N�g���C�ɒǉ� */
		trayIcon(hWnd, NIM_ADD);

		/* �^�C�}�𐶐� */
		SetTimer(hWnd, ID_TIMER_VOICE, 60000 - (time.wSecond * 1000 + time.wMilliseconds), NULL);
		SetTimer(hWnd, ID_TIMER_SYNC, 1000, NULL);	// 1�b��ɏ��񓯊�

		break;

#ifdef DISPWND
	case WM_PAINT:
		if (NULL == (hdc = BeginPaint(hWnd, &ps))) {
			SYSLOG((LOG_ERR, "BeginPaint() failed."));
		}else{
			/* �C���X�g�̕`�� */
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

				/* �����̕`�� */
				if (NULL == (hbWhite = CreateSolidBrush(RGB(255, 255, 255)))) {		/* �� */
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

				if (NULL == (hbOrange = CreateSolidBrush(RGB(255, 153, 0)))) {	/* �I�����W */
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

				/* �� */
				hbmOld = (HBITMAP)SelectObject(hdcMem, s_hbmMonth);
				BitBlt(hdc, (rectClock.right + rectClock.left) / 2 - 16, (rectClock.bottom + rectClock.top) / 2 - 16
					, 32, 32, hdcMem, (time.wMonth - 1) % 12 * 32, 0, SRCCOPY);
				SelectObject(hdcMem, hbmOld);

				/* �Z�j(m) */
				drawTime(hdc, (double)time.wHour / 12.0 + (double)time.wMinute / 720.0
					, (rectClock.right + rectClock.left) / 2, (rectClock.bottom + rectClock.top) / 2
					, (rectClock.right - rectClock.left) / 2, 8, 11);
#if 0
				/* ���j(m) */
				drawTime(hdc, (double)time.wMinute / 60.0
					, (rectClock.right + rectClock.left) / 2, (rectClock.bottom + rectClock.top) / 2
					, (rectClock.right - rectClock.left) / 2, 8, 11);
#endif
				DeleteDC(hdcMem);
			}
			EndPaint(hWnd, &ps);
		}
		break;

#if 1 /* �_�u���N���b�N�Ŏ����ǂݏグ */
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
				/* �����\���X�V */
				InvalidateRect(hWnd, &rectClock, TRUE);
				UpdateWindow(hWnd);
#endif
				/* �^�X�N�g���C���b�Z�[�W�ύX */
				trayIcon(hWnd, NIM_MODIFY);

				/* �����ǂݏグ */
				if (0 != g_Conf.nVoiceInterval) {
					if (time.wMinute % g_Conf.nVoiceInterval == 0) {
						playCurrentTime(g_Conf.nVoiceVolume);
					}
				}
			}

			/* �^�C�}�[�̍Đݒ� */
			GetLocalTime(&time);	/* �Ď擾 */
			SetTimer(hWnd, ID_TIMER_VOICE, 60000 - (time.wSecond * 1000 + time.wMilliseconds), NULL);

		}else if (wParam == ID_TIMER_SYNC) {
			//KillTimer(hWnd, ID_TIMER_SYNC);
			if (FALSE != g_Conf.bSync) {
				/* �C���^�[�l�b�g�ڑ��̌��o */
				DWORD dwFlags;
				if (FALSE == InternetGetConnectedState(&dwFlags, 0)) {
					nTimer = -1;
					SYSLOG((LOG_DEBUG, "InternetGetConnectedState() failed."));
				}else{
					SYSLOG((LOG_DEBUG, "InternetGetConnectedState(): %d", dwFlags));
					/* �������� */
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

			/* �^�C�}�[�̍Đݒ� */
			SetTimer(hWnd, ID_TIMER_SYNC, 60000, NULL);
		}
		break;

	case WM_TASKTRAY:		/* �g���C�C�x���g */
		switch (lParam) {
		case WM_LBUTTONDOWN:	/* ���{�^������ */
			/* ���v�E�B���h�E����ɍőO�ʕ\���ɂ��Ă��Ȃ���΍őO�ʂɕ\�� */
			if (0 == (WS_EX_TOPMOST & GetWindowLong(hWnd, GWL_EXSTYLE))) {
				SetForegroundWindow(hWnd);
			}
			break;
		case WM_RBUTTONDOWN:	/* �E�{�^������ */
			SetForegroundWindow(hWnd);
			/* ���j���[�\�� */
			GetCursorPos(&point);
			TrackPopupMenu(s_hMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN, point.x, point.y, 0, hWnd, NULL );
			PostMessage(hWnd, WM_NULL, 0, 0);/* MSDN:Q135788 */
			break;
		case WM_LBUTTONDBLCLK:	/* ���{�^���_�u���N���b�N */
#if 1 /* �E�B���h�E�\���؂�ւ� */
#ifdef DISPWND
			/* �\��/��\���̐؂�ւ� */
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
#else /* ���ݎ����ǂݏグ */
			hCursor = SetCursor(LoadCursor(NULL, IDC_WAIT));
			playCurrentTime(g_Conf.nVoiceVolume);
			SetCursor(hCursor);
#endif
			break;
		}
		break;

	case WM_CLOSE:			/* �E�B���h�E����� */
	case WM_ENDSESSION:		/* Winwdows�I�� */
#ifdef DISPWND
		conf_setWnd(hWnd);
#endif
		conf_save();	/* ENDSESSION�΍�: �Ƃ肠����������save�����Ⴄ */
		return DefWindowProc(hWnd, message, wParam, lParam);

	case WM_DESTROY:		/* �E�B���h�E�I�� */
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

	case WM_COMMAND:		/* ���j���[�I�� */
		switch (LOWORD(wParam)) {
#ifdef DISPWND
		case ID_MENU_VIEW:		/* ���v�\�� */
			/* �\��/��\���̐؂�ւ� */
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

		case ID_MENU_ONTOP:		/* �őO�ʂɕ\�� */
			LONG wsex;
			/* �ʏ�/�őO�ʕ\���̐؂�ւ� */
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
		case ID_MENU_SYNC:		/* �������킹 */
			/* �����������s */
			memset(&syncParam, 0, sizeof(syncParam));
			syncParam.bSync = TRUE;
			syncParam.szServer = g_Conf.szServer;
			syncParam.nMaxDelay = g_Conf.nMaxDelay;
			syncParam.nTolerance = g_Conf.nTolerance;
			syncParam.nTimeShift = g_Conf.nTimeShift;
			syncClock(hWnd, &syncParam, TRUE);
			break;
#endif

		case ID_MENU_CONFIG:	/* �ݒ� */
			/* �ݒ�_�C�A���O�\�� */
			DialogBox(g_hInst, MAKEINTRESOURCE(IDD_CONFIG), hWnd, ConfigDlgProc);
			break;

		case ID_MENU_ABOUT:		/* ��� */
			/* ���_�C�A���O�\�� */
			DialogBox(g_hInst, MAKEINTRESOURCE(IDD_ABOUT), hWnd, AboutDlgProc);
			break;

		case ID_MENU_EXIT:		/* �I�� */
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
���C���֐�
----------------------------------------------------------------------
*/
int WINAPI
WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pszArg, int nShowCmd)
{
	HANDLE hMutex;		/* �A�v���P�[�V������Mutex(�d���N���j�~�p) */
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

	/* �d���N���j�~ */
	if (NULL == (hMutex = CreateMutex(NULL, FALSE, IDENT_MUTEXOBJECTNAME))) {
		SYSLOG((LOG_ERR, "CreateMutex() failed."));
		EXCEPTION();
	}
	if (ERROR_ALREADY_EXISTS == GetLastError()) {
		CloseHandle(hMutex);
		/* �����̃E�B���h�E���A�N�e�B�u�� */
		if (NULL != (hWnd = FindWindow(IDENT_APLTITLE, IDENT_APLTITLE))) {
			SetForegroundWindow(hWnd);
		}
		return 0;
	}


	/* ���\�[�X�̃��[�h */
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
	/* ���C�u�����ɒ�`����Ă��Ȃ�API�ւ̃|�C���^���擾 */
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
		/* WS_EX_APPWINDOW | WS_EX_TOPMOST,*/	/* �L���v�`������Ƃ� */
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
