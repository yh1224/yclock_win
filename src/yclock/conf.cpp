#include <windows.h>
#include <wininet.h>
#include <shlobj.h>
#include <stdio.h>
#include "ident.h"
#include "resource.h"
#include "voice.h"
#include "sntp.h"
#include "misc.h"
#include "conf.h"

/* グローバル設定 */
struct conf g_Conf;

/* レジストリキー */
static const char *pszRegistryKey_Run = "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run";
static const char *pszRegistryKey_Shortcut = "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders";

/* デフォルト値 */
static const char *pszDefaultServer = "assemble.net";
static const BOOL bDefaultSync = TRUE;
static const int nDefaultSyncInterval = 30;
static const int nDefaultVoiceInterval = 60;
static const int nVoiceVolume_Low = 33;
static const int nVoiceVolume_Mid = 66;
static const int nVoiceVolume_High = 100;
static const int nDefaultVoiceVolume = nVoiceVolume_High;
static const int nDefaultMaxDelay = 500;
static const int nMinMaxDelay = 1;
static const int nMaxMaxDelay = 1000;
static const int nDefaultTolerance = 200;
static const int nMinTolerance = 0;
static const int nMaxTolerance = 1000;
static const int nDefaultTimeShift = 0;
static const int nMaxTimeShift = 300;
static const int nMinTimeShift = -300;
#ifdef DISPWND
static const BOOL bDefaultShow = TRUE;
#endif

extern HINSTANCE g_hInst;

/* デフォルトの設定 */
void
conf_init() {
	memset(&g_Conf, 0, sizeof(struct conf));
	strcpy(g_Conf.szServer, pszDefaultServer);
	g_Conf.bSync =bDefaultSync;
	g_Conf.nSyncInterval = nDefaultSyncInterval;
	g_Conf.nVoiceInterval = nDefaultVoiceInterval;
	g_Conf.nVoiceVolume = nDefaultVoiceVolume;
	g_Conf.nMaxDelay = nDefaultMaxDelay;
	g_Conf.nTolerance = nDefaultTolerance;
	g_Conf.nTimeShift = nDefaultTimeShift;
#ifdef DISPWND
	g_Conf.bShow = bDefaultShow;
#endif
	return;
}

/* レジストリに設定を書き込む */
BOOL
conf_save() {
	/* 設定の保存 */
	HKEY hkReg;
	DWORD dp;

	RegCreateKeyEx(HKEY_CURRENT_USER, IDENT_REGISTRYKEY, 0, "REG_SZ",
		REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hkReg, &dp);
	RegSetValueEx(hkReg, "Server", 0, REG_SZ, (BYTE*)g_Conf.szServer, strlen(g_Conf.szServer) + 1);
	RegSetValueEx(hkReg, "SyncInterval", 0, REG_DWORD, (BYTE*)&g_Conf.nSyncInterval, 4);
	RegSetBool(hkReg, "Sync", g_Conf.bSync);
	RegSetValueEx(hkReg, "VoiceInterval", 0, REG_DWORD, (BYTE*)&g_Conf.nVoiceInterval, 4);
	RegSetValueEx(hkReg, "VoiceVolume", 0, REG_DWORD, (BYTE*)&g_Conf.nVoiceVolume, 4);
	RegSetValueEx(hkReg, "PosX", 0, REG_DWORD, (BYTE*)&g_Conf.nPosX, 4);
	RegSetValueEx(hkReg, "PosY", 0, REG_DWORD, (BYTE*)&g_Conf.nPosY, 4);
	RegSetValueEx(hkReg, "MaxDelay", 0, REG_DWORD, (BYTE*)&g_Conf.nMaxDelay, 4);
	RegSetValueEx(hkReg, "Tolerance", 0, REG_DWORD, (BYTE*)&g_Conf.nTolerance, 4);
	RegSetValueEx(hkReg, "TimeShift", 0, REG_DWORD, (BYTE*)&g_Conf.nTimeShift, 4);
#ifdef DISPWND
	RegSetBool(hkReg, "Show", g_Conf.bShow);
	RegSetBool(hkReg, "OnTop", g_Conf.bOnTop);
#endif
	RegCloseKey(hkReg);

	return TRUE;
}

/* レジストリから設定を読み込む */
BOOL
conf_load() {
	HKEY hkReg;

	// Load default value
	conf_init();

	if (ERROR_SUCCESS == RegOpenKeyEx(HKEY_CURRENT_USER, IDENT_REGISTRYKEY, 0, KEY_ALL_ACCESS, &hkReg)) {
		DWORD siz, typ;

		siz = INTERNET_MAX_HOST_NAME_LENGTH;
		RegQueryValueEx(hkReg, "Server", 0, NULL, (BYTE*)&g_Conf.szServer, (LPDWORD)&siz);

		RegGetDW(hkReg, "SyncInterval", (DWORD *)&g_Conf.nSyncInterval);
		RegGetDW(hkReg, "VoiceInterval", (DWORD *)&g_Conf.nVoiceInterval);
		RegGetDW(hkReg, "VoiceVolume", (DWORD *)&g_Conf.nVoiceVolume);
		if (g_Conf.nVoiceVolume <= 0 || g_Conf.nVoiceVolume > 100) {
			g_Conf.nVoiceVolume = nDefaultVoiceVolume;
		}
		RegGetDW(hkReg, "PosX", (DWORD *)&g_Conf.nPosX);
		RegGetDW(hkReg, "PosY", (DWORD *)&g_Conf.nPosY);
		g_Conf.bSync = RegGetBool(hkReg, "Sync");
		RegGetDW(hkReg, "MaxDelay", (DWORD *)&g_Conf.nMaxDelay);
		if (g_Conf.nMaxDelay < nMinMaxDelay || g_Conf.nMaxDelay > nMaxMaxDelay) {
			g_Conf.nMaxDelay = nDefaultMaxDelay;
		}
		RegGetDW(hkReg, "Tolerance", (DWORD *)&g_Conf.nTolerance);
		if (g_Conf.nTolerance < nMinTolerance || g_Conf.nTolerance > nMaxTolerance) {
			g_Conf.nTolerance = nDefaultTolerance;
		}
		RegGetDW(hkReg, "TimeShift", (DWORD *)&g_Conf.nTimeShift);
		if (g_Conf.nTimeShift < nMinTimeShift || g_Conf.nTimeShift > nMaxTimeShift) {
			g_Conf.nTimeShift = nDefaultTimeShift;
		}
#ifdef DISPWND
		g_Conf.bShow = RegGetBool(hkReg, "Show");
		g_Conf.bOnTop = RegGetBool(hkReg, "OnTop");
#endif
		RegQueryValueEx(hkReg, "Away", 0, &typ, NULL, (LPDWORD)&siz);
		RegCloseKey(hkReg);
	}

	return TRUE;
}

#ifdef DISPWND
BOOL
conf_setPosWnd(HWND hWnd) {
	RECT rect;

	if (FALSE == GetWindowRect(hWnd, &rect)) {
		return FALSE;
	}
	g_Conf.nPosX = rect.left;
	g_Conf.nPosY = rect.top;
	return TRUE;
}
BOOL
conf_setWnd(HWND hWnd) {
	LONG wsex;
	if (FALSE == conf_setPosWnd(hWnd)) {
		return FALSE;
	}
	g_Conf.bShow = IsWindowVisible(hWnd);
	if (0 == (WS_EX_TOPMOST & (wsex = GetWindowLong(hWnd, GWL_EXSTYLE)))) {
		g_Conf.bOnTop = FALSE;
	}else{
		g_Conf.bOnTop = TRUE;
	}
	return TRUE;
}
#endif



/*
======================================================================
NTPサーバ設定ダイアログ
----------------------------------------------------------------------
*/
static void
enableNtpDlg(
	HWND hWnd
)
{
	if (BST_CHECKED == IsDlgButtonChecked(hWnd, IDC_NTP_SHIFT)) {
		/* 時刻シフトONのとき */
		EnableWindow(GetDlgItem(hWnd, IDC_NTP_SHIFT_SEC), TRUE);	/* シフト時間設定有効 */
	}else{
		/* 時刻シフトOFFのとき */
		EnableWindow(GetDlgItem(hWnd, IDC_NTP_SHIFT_SEC), FALSE);	/* シフト間設定無効 */
	}

	return;
}

BOOL CALLBACK
NtpDlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	BOOL bTrans;
	struct sync_param syncParam;

	/* 設定の一時保存用 */
	char szServer[INTERNET_MAX_HOST_NAME_LENGTH + 1];
	int nMaxDelay;
	int nTolerance;
	int nTimeShift;

	switch (uMsg) {
	case WM_INITDIALOG:
		/* ホスト名 */
		SetDlgItemText(hwndDlg, IDC_NTP_SERVER, g_Conf.szServer);

		/* 許容遅延時間 */
		SetDlgItemInt(hwndDlg, IDC_NTP_MAXDELAY, g_Conf.nMaxDelay, FALSE);
		SendMessage(GetDlgItem(hwndDlg, IDC_NTP_MAXDELAY), EM_SETLIMITTEXT, 4, 0);

		/* 許容誤差 */
		SetDlgItemInt(hwndDlg, IDC_NTP_TOLERANCE, g_Conf.nTolerance, FALSE);
		SendMessage(GetDlgItem(hwndDlg, IDC_NTP_TOLERANCE), EM_SETLIMITTEXT, 4, 0);

		/* 時刻シフト */
		if (g_Conf.nTimeShift != 0) {
			/* 時刻シフトOnのとき */
			CheckDlgButton(hwndDlg, IDC_NTP_SHIFT, BST_CHECKED);
		}
		SetDlgItemInt(hwndDlg, IDC_NTP_SHIFT_SEC, g_Conf.nTimeShift, TRUE);
		SendMessage(GetDlgItem(hwndDlg, IDC_NTP_SHIFT_SEC), EM_SETLIMITTEXT, 5, 0);

	 	enableNtpDlg(hwndDlg);
		return 1;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDC_NTP_TEST:
			/* 現在の設定を取得 */
			GetDlgItemText(hwndDlg, IDC_NTP_SERVER, szServer, INTERNET_MAX_HOST_NAME_LENGTH);
			nMaxDelay = GetDlgItemInt(hwndDlg, IDC_NTP_MAXDELAY, &bTrans, FALSE);
			if (FALSE == bTrans) {
				showMessage(g_hInst, hwndDlg, IDS_CONF_INVALIDDIGIT, MB_OK | MB_ICONSTOP);
				break;
			}
			if (nMaxDelay < nMinMaxDelay || nMaxDelay > nMaxMaxDelay) {
				showMessage(g_hInst, hwndDlg, IDS_CONF_OUTOFRANGE, MB_OK | MB_ICONSTOP);
				break;
			}
			nTolerance = GetDlgItemInt(hwndDlg, IDC_NTP_TOLERANCE, &bTrans, FALSE);
			if (FALSE == bTrans) {
				showMessage(g_hInst, hwndDlg, IDS_CONF_INVALIDDIGIT, MB_OK | MB_ICONSTOP);
				break;
			}
			if (nTolerance < nMinTolerance || nTolerance > nMaxTolerance) {
				showMessage(g_hInst, hwndDlg, IDS_CONF_OUTOFRANGE, MB_OK | MB_ICONSTOP);
				break;
			}
			nTimeShift = GetDlgItemInt(hwndDlg, IDC_NTP_SHIFT_SEC, &bTrans, TRUE);
			if (FALSE == bTrans) {
				showMessage(g_hInst, hwndDlg, IDS_CONF_INVALIDDIGIT, MB_OK | MB_ICONSTOP);
				break;
			}
			if (nTimeShift < nMinTimeShift || nTimeShift > nMaxTimeShift) {
				showMessage(g_hInst, hwndDlg, IDS_CONF_OUTOFRANGE, MB_OK | MB_ICONSTOP);
				break;
			}

			/* 時刻同期テスト実行 */
			memset(&syncParam, 0, sizeof(syncParam));
			syncParam.bSync = FALSE;
			syncParam.szServer = szServer;
			syncParam.nMaxDelay = nMaxDelay;
			syncParam.nTolerance = nTolerance;
			syncParam.nTimeShift = nTimeShift;
			syncClock(hwndDlg, &syncParam, TRUE);
			break;

		case IDC_NTP_DEFAULT:
			/* 初期設定に戻す */
			SetDlgItemText(hwndDlg, IDC_NTP_SERVER, pszDefaultServer);
			SetDlgItemInt(hwndDlg, IDC_NTP_MAXDELAY, nDefaultMaxDelay, FALSE);
			SetDlgItemInt(hwndDlg, IDC_NTP_TOLERANCE, nDefaultTolerance, FALSE);
			CheckDlgButton(hwndDlg, IDC_NTP_SHIFT, BST_UNCHECKED);
			SetDlgItemInt(hwndDlg, IDC_NTP_SHIFT_SEC, nDefaultTimeShift, TRUE);
			break;

		case IDOK:
			/* ホスト名 */
			GetDlgItemText(hwndDlg, IDC_NTP_SERVER, szServer, INTERNET_MAX_HOST_NAME_LENGTH);

			/* 許容遅延時間 */
			nMaxDelay = GetDlgItemInt(hwndDlg, IDC_NTP_MAXDELAY, &bTrans, FALSE);
			if (FALSE == bTrans) {
				showMessage(g_hInst, hwndDlg, IDS_CONF_INVALIDDIGIT, MB_OK | MB_ICONSTOP);
				break;
			}
			if (nMaxDelay < nMinMaxDelay || nMaxDelay > nMaxMaxDelay) {
				showMessage(g_hInst, hwndDlg, IDS_CONF_OUTOFRANGE, MB_OK | MB_ICONSTOP);
				break;
			}

			/* 許容誤差 */
			nTolerance = GetDlgItemInt(hwndDlg, IDC_NTP_TOLERANCE, &bTrans, FALSE);
			if (FALSE == bTrans) {
				showMessage(g_hInst, hwndDlg, IDS_CONF_INVALIDDIGIT, MB_OK | MB_ICONSTOP);
				break;
			}
			if (nTolerance < nMinTolerance || nTolerance > nMaxTolerance) {
				showMessage(g_hInst, hwndDlg, IDS_CONF_OUTOFRANGE, MB_OK | MB_ICONSTOP);
				break;
			}

			/* 時刻シフト */
			if (BST_CHECKED == IsDlgButtonChecked(hwndDlg, IDC_NTP_SHIFT)) {
				nTimeShift = GetDlgItemInt(hwndDlg, IDC_NTP_SHIFT_SEC, &bTrans, TRUE);
				if (FALSE == bTrans) {
					showMessage(g_hInst, hwndDlg, IDS_CONF_INVALIDDIGIT, MB_OK | MB_ICONSTOP);
					break;
				}
				if (nTimeShift < nMinTimeShift || nTimeShift > nMaxTimeShift) {
					showMessage(g_hInst, hwndDlg, IDS_CONF_OUTOFRANGE, MB_OK | MB_ICONSTOP);
					break;
				}
			}else{
				nTimeShift = 0;
			}

			strcpy(g_Conf.szServer, szServer);
			g_Conf.nMaxDelay = nMaxDelay;
			g_Conf.nTolerance = nTolerance;
			g_Conf.nTimeShift = nTimeShift;

			/* no break */
		case IDCANCEL:
			EndDialog(hwndDlg, 0);
	 	}

	 	enableNtpDlg(hwndDlg);
		return 1;
	}
	return 0;
}



/*
======================================================================
設定ダイアログ
----------------------------------------------------------------------
*/
static void
enableConfigDlg(
	HWND	hWnd
)
{
	if (FALSE != IsWindowEnabled(GetDlgItem(hWnd,IDC_CONFIG_SYNC))
			&& BST_CHECKED == IsDlgButtonChecked(hWnd, IDC_CONFIG_SYNC)) {
		/* 時刻同期権限あり、かつ時刻同期Onのとき */
		EnableWindow(GetDlgItem(hWnd, IDC_CONFIG_SYNCINTERVAL), TRUE);	/* 時刻同期間隔設定有効 */
		//EnableWindow(GetDlgItem(hWnd, IDC_CONFIG_SYNCINTERVALSPIN), TRUE);
		EnableWindow(GetDlgItem(hWnd, IDC_CONFIG_NTP), TRUE);		/* 時刻同期詳細設定無効 */
	}else{
		/* 時刻同期Offのとき */
		EnableWindow(GetDlgItem(hWnd, IDC_CONFIG_SYNCINTERVAL), FALSE);	/* 時刻同期間隔設定無効 */
		//EnableWindow(GetDlgItem(hWnd, IDC_CONFIG_SYNCINTERVALSPIN), FALSE);
		EnableWindow(GetDlgItem(hWnd, IDC_CONFIG_NTP), FALSE);		/* 時刻同期詳細設定無効 */
	}

	if (BST_CHECKED == IsDlgButtonChecked(hWnd, IDC_CONFIG_VOICE0)) {
		/* 読み上げなしの場合 */
		EnableWindow(GetDlgItem(hWnd, IDC_CONFIG_VOL_LOW), FALSE);
		EnableWindow(GetDlgItem(hWnd, IDC_CONFIG_VOL_MID), FALSE);
		EnableWindow(GetDlgItem(hWnd, IDC_CONFIG_VOL_HIGH), FALSE);
	}else{
		EnableWindow(GetDlgItem(hWnd, IDC_CONFIG_VOL_LOW), TRUE);
		EnableWindow(GetDlgItem(hWnd, IDC_CONFIG_VOL_MID), TRUE);
		EnableWindow(GetDlgItem(hWnd, IDC_CONFIG_VOL_HIGH), TRUE);
	}

	return;
}

/* スタートアップショートカットのパスを取得する */
static void
getShortcutPath(
	char	*pszShortcutPath,	/* OUTPUT: スタートアップパスを格納する領域 */
	DWORD	cb			/* 領域のサイズ */
)
{
	HKEY	hkey;
	DWORD	dwType;

	RegOpenKeyEx(HKEY_CURRENT_USER, pszRegistryKey_Shortcut, 0, KEY_READ, &hkey);
	RegQueryValueEx(hkey, "Startup", 0, &dwType, (BYTE *)pszShortcutPath, &cb);
	RegCloseKey(hkey);

	sprintf(pszShortcutPath + strlen(pszShortcutPath), "\\%s.lnk", IDENT_APLTITLE);
	return;
}

BOOL CALLBACK
ConfigDlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	FILE *fp;
	BOOL bTrans;

	/* 設定の一時保存用 */
	BOOL bSync;
	int nSyncInterval;
	int nVoiceInterval;
	int nVoiceVolume;

	TCHAR szModulePath[_MAX_PATH];
	TCHAR szShortcutPath[_MAX_PATH];

	//SHGetSpecialFolderPath(hWnd, szShortcutPath, CSIDL_STARTUP, TRUE);	/* required IE4 Desktop */

	switch (uMsg) {
	case WM_INITDIALOG:
		/* 時刻同期設定 */
		if (FALSE == getClockPrivilege()) {
			/* 時刻修正権限なしのとき */
			EnableWindow(GetDlgItem(hWnd, IDC_CONFIG_SYNC), FALSE);		/* 時刻同期設定無効 */
		}
		if (g_Conf.bSync != FALSE) {
			/* 時刻同期Onのとき */
			CheckDlgButton(hWnd, IDC_CONFIG_SYNC, BST_CHECKED);
		}

		/* 時刻同期間隔 */
		SetDlgItemInt(hWnd, IDC_CONFIG_SYNCINTERVAL, g_Conf.nSyncInterval, FALSE);
		SendMessage(GetDlgItem(hWnd, IDC_CONFIG_SYNCINTERVAL), EM_SETLIMITTEXT, 4, 0);

		/* 時報読み上げ間隔 */
		if (FALSE == existVoice()) {
			/* 音声なしのとき */
			EnableWindow(GetDlgItem(hWnd, IDC_CONFIG_VOICE60), FALSE);	/* 時報読み上げ設定無効 */
			EnableWindow(GetDlgItem(hWnd, IDC_CONFIG_VOICE30), FALSE);
			EnableWindow(GetDlgItem(hWnd, IDC_CONFIG_VOICE0), FALSE);
		}
		if (g_Conf.nVoiceInterval == 60) {
			CheckDlgButton(hWnd, IDC_CONFIG_VOICE60, BST_CHECKED);
		}else if (g_Conf.nVoiceInterval == 30) {
			CheckDlgButton(hWnd, IDC_CONFIG_VOICE30, BST_CHECKED);
		}else{
			CheckDlgButton(hWnd, IDC_CONFIG_VOICE0, BST_CHECKED);
		}

		/* 時刻読み上げ音量 */
		if (g_Conf.nVoiceVolume == nVoiceVolume_Low) {
			CheckDlgButton(hWnd, IDC_CONFIG_VOL_LOW, BST_CHECKED);
		}else if (g_Conf.nVoiceVolume == nVoiceVolume_Mid) {
			CheckDlgButton(hWnd, IDC_CONFIG_VOL_MID, BST_CHECKED);
		}else{
			CheckDlgButton(hWnd, IDC_CONFIG_VOL_HIGH, BST_CHECKED);
		}

		/* 自動起動設定 */
		getShortcutPath(szShortcutPath, sizeof(szShortcutPath));
		if (NULL != (fp = fopen(szShortcutPath, "r"))) {
			/* ショートカットがあればOn */
			CheckDlgButton(hWnd, IDC_CONFIG_STARTUP, BST_CHECKED);
			fclose(fp);
		}

	 	enableConfigDlg(hWnd);
		adjustWndPos(hWnd);
		return 1;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDC_CONFIG_NTP:
			/* NTP設定ダイアログを起動 */
			DialogBox(g_hInst, MAKEINTRESOURCE(IDD_SNTP), hWnd, NtpDlgProc);
			break;

		case IDOK:
			/* 時刻同期設定 */
			if (BST_CHECKED == IsDlgButtonChecked(hWnd, IDC_CONFIG_SYNC)) {
				bSync = TRUE;
			}else{
				bSync = FALSE;
			}

			/* 時刻同期間隔チェック */
			nSyncInterval = GetDlgItemInt(hWnd, IDC_CONFIG_SYNCINTERVAL, &bTrans, FALSE);
			if (FALSE == bTrans) {
				showMessage(g_hInst, hWnd, IDS_CONF_INVALIDDIGIT, MB_OK | MB_ICONSTOP);
				break;
			}
			if (nSyncInterval <= 0 || nSyncInterval > 1440) {
				showMessage(g_hInst, hWnd, IDS_CONF_OUTOFRANGE, MB_OK | MB_ICONSTOP);
				break;
			}

			/* 時刻読み上げ間隔設定 */
			if (BST_CHECKED == IsDlgButtonChecked(hWnd, IDC_CONFIG_VOICE60)) {
				nVoiceInterval = 60;
			}else if (BST_CHECKED == IsDlgButtonChecked(hWnd, IDC_CONFIG_VOICE30)) {
				nVoiceInterval = 30;
			}else{
				nVoiceInterval = 0;
			}

			/* 時刻読み上げ音量設定 */
			if (BST_CHECKED == IsDlgButtonChecked(hWnd, IDC_CONFIG_VOL_LOW)) {
				nVoiceVolume = nVoiceVolume_Low;
			}else if (BST_CHECKED == IsDlgButtonChecked(hWnd, IDC_CONFIG_VOL_MID)) {
				nVoiceVolume = nVoiceVolume_Mid;
			}else{
				nVoiceVolume = nVoiceVolume_High;
			}

			g_Conf.bSync = bSync;
			g_Conf.nSyncInterval = nSyncInterval;
			g_Conf.nVoiceInterval = nVoiceInterval;
			g_Conf.nVoiceVolume = nVoiceVolume;

			/* 自動起動設定 */
			getShortcutPath(szShortcutPath, sizeof(szShortcutPath));
			if (BST_CHECKED == IsDlgButtonChecked(hWnd, IDC_CONFIG_STARTUP)) {
				/* 自動起動Onのとき */
				if (SUCCEEDED(CoInitialize(NULL))) {
					/* スタートアップにショートカットを生成 */
					GetModuleFileName(NULL, szModulePath, sizeof(szModulePath));
					CreateShortcut(szShortcutPath, szModulePath, NULL, NULL, NULL, NULL, 0, SW_SHOWNORMAL);
					CoUninitialize();
				}
			}else{
				/* 自動起動Offのとき */
				remove(szShortcutPath);		/* ショートカット削除 */
			}
			/* no break */
		case IDCANCEL:
			EndDialog(hWnd, 0);
	 	}

	 	enableConfigDlg(hWnd);
		return 1;
	}
	return 0;
}
