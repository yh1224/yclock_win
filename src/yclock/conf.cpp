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

/* �O���[�o���ݒ� */
struct conf g_Conf;

/* ���W�X�g���L�[ */
static const char *pszRegistryKey_Run = "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run";
static const char *pszRegistryKey_Shortcut = "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders";

/* �f�t�H���g�l */
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

/* �f�t�H���g�̐ݒ� */
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

/* ���W�X�g���ɐݒ���������� */
BOOL
conf_save() {
	/* �ݒ�̕ۑ� */
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

/* ���W�X�g������ݒ��ǂݍ��� */
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
NTP�T�[�o�ݒ�_�C�A���O
----------------------------------------------------------------------
*/
static void
enableNtpDlg(
	HWND hWnd
)
{
	if (BST_CHECKED == IsDlgButtonChecked(hWnd, IDC_NTP_SHIFT)) {
		/* �����V�t�gON�̂Ƃ� */
		EnableWindow(GetDlgItem(hWnd, IDC_NTP_SHIFT_SEC), TRUE);	/* �V�t�g���Ԑݒ�L�� */
	}else{
		/* �����V�t�gOFF�̂Ƃ� */
		EnableWindow(GetDlgItem(hWnd, IDC_NTP_SHIFT_SEC), FALSE);	/* �V�t�g�Ԑݒ薳�� */
	}

	return;
}

BOOL CALLBACK
NtpDlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	BOOL bTrans;
	struct sync_param syncParam;

	/* �ݒ�̈ꎞ�ۑ��p */
	char szServer[INTERNET_MAX_HOST_NAME_LENGTH + 1];
	int nMaxDelay;
	int nTolerance;
	int nTimeShift;

	switch (uMsg) {
	case WM_INITDIALOG:
		/* �z�X�g�� */
		SetDlgItemText(hwndDlg, IDC_NTP_SERVER, g_Conf.szServer);

		/* ���e�x������ */
		SetDlgItemInt(hwndDlg, IDC_NTP_MAXDELAY, g_Conf.nMaxDelay, FALSE);
		SendMessage(GetDlgItem(hwndDlg, IDC_NTP_MAXDELAY), EM_SETLIMITTEXT, 4, 0);

		/* ���e�덷 */
		SetDlgItemInt(hwndDlg, IDC_NTP_TOLERANCE, g_Conf.nTolerance, FALSE);
		SendMessage(GetDlgItem(hwndDlg, IDC_NTP_TOLERANCE), EM_SETLIMITTEXT, 4, 0);

		/* �����V�t�g */
		if (g_Conf.nTimeShift != 0) {
			/* �����V�t�gOn�̂Ƃ� */
			CheckDlgButton(hwndDlg, IDC_NTP_SHIFT, BST_CHECKED);
		}
		SetDlgItemInt(hwndDlg, IDC_NTP_SHIFT_SEC, g_Conf.nTimeShift, TRUE);
		SendMessage(GetDlgItem(hwndDlg, IDC_NTP_SHIFT_SEC), EM_SETLIMITTEXT, 5, 0);

	 	enableNtpDlg(hwndDlg);
		return 1;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDC_NTP_TEST:
			/* ���݂̐ݒ���擾 */
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

			/* ���������e�X�g���s */
			memset(&syncParam, 0, sizeof(syncParam));
			syncParam.bSync = FALSE;
			syncParam.szServer = szServer;
			syncParam.nMaxDelay = nMaxDelay;
			syncParam.nTolerance = nTolerance;
			syncParam.nTimeShift = nTimeShift;
			syncClock(hwndDlg, &syncParam, TRUE);
			break;

		case IDC_NTP_DEFAULT:
			/* �����ݒ�ɖ߂� */
			SetDlgItemText(hwndDlg, IDC_NTP_SERVER, pszDefaultServer);
			SetDlgItemInt(hwndDlg, IDC_NTP_MAXDELAY, nDefaultMaxDelay, FALSE);
			SetDlgItemInt(hwndDlg, IDC_NTP_TOLERANCE, nDefaultTolerance, FALSE);
			CheckDlgButton(hwndDlg, IDC_NTP_SHIFT, BST_UNCHECKED);
			SetDlgItemInt(hwndDlg, IDC_NTP_SHIFT_SEC, nDefaultTimeShift, TRUE);
			break;

		case IDOK:
			/* �z�X�g�� */
			GetDlgItemText(hwndDlg, IDC_NTP_SERVER, szServer, INTERNET_MAX_HOST_NAME_LENGTH);

			/* ���e�x������ */
			nMaxDelay = GetDlgItemInt(hwndDlg, IDC_NTP_MAXDELAY, &bTrans, FALSE);
			if (FALSE == bTrans) {
				showMessage(g_hInst, hwndDlg, IDS_CONF_INVALIDDIGIT, MB_OK | MB_ICONSTOP);
				break;
			}
			if (nMaxDelay < nMinMaxDelay || nMaxDelay > nMaxMaxDelay) {
				showMessage(g_hInst, hwndDlg, IDS_CONF_OUTOFRANGE, MB_OK | MB_ICONSTOP);
				break;
			}

			/* ���e�덷 */
			nTolerance = GetDlgItemInt(hwndDlg, IDC_NTP_TOLERANCE, &bTrans, FALSE);
			if (FALSE == bTrans) {
				showMessage(g_hInst, hwndDlg, IDS_CONF_INVALIDDIGIT, MB_OK | MB_ICONSTOP);
				break;
			}
			if (nTolerance < nMinTolerance || nTolerance > nMaxTolerance) {
				showMessage(g_hInst, hwndDlg, IDS_CONF_OUTOFRANGE, MB_OK | MB_ICONSTOP);
				break;
			}

			/* �����V�t�g */
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
�ݒ�_�C�A���O
----------------------------------------------------------------------
*/
static void
enableConfigDlg(
	HWND	hWnd
)
{
	if (FALSE != IsWindowEnabled(GetDlgItem(hWnd,IDC_CONFIG_SYNC))
			&& BST_CHECKED == IsDlgButtonChecked(hWnd, IDC_CONFIG_SYNC)) {
		/* ����������������A����������On�̂Ƃ� */
		EnableWindow(GetDlgItem(hWnd, IDC_CONFIG_SYNCINTERVAL), TRUE);	/* ���������Ԋu�ݒ�L�� */
		//EnableWindow(GetDlgItem(hWnd, IDC_CONFIG_SYNCINTERVALSPIN), TRUE);
		EnableWindow(GetDlgItem(hWnd, IDC_CONFIG_NTP), TRUE);		/* ���������ڍאݒ薳�� */
	}else{
		/* ��������Off�̂Ƃ� */
		EnableWindow(GetDlgItem(hWnd, IDC_CONFIG_SYNCINTERVAL), FALSE);	/* ���������Ԋu�ݒ薳�� */
		//EnableWindow(GetDlgItem(hWnd, IDC_CONFIG_SYNCINTERVALSPIN), FALSE);
		EnableWindow(GetDlgItem(hWnd, IDC_CONFIG_NTP), FALSE);		/* ���������ڍאݒ薳�� */
	}

	if (BST_CHECKED == IsDlgButtonChecked(hWnd, IDC_CONFIG_VOICE0)) {
		/* �ǂݏグ�Ȃ��̏ꍇ */
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

/* �X�^�[�g�A�b�v�V���[�g�J�b�g�̃p�X���擾���� */
static void
getShortcutPath(
	char	*pszShortcutPath,	/* OUTPUT: �X�^�[�g�A�b�v�p�X���i�[����̈� */
	DWORD	cb			/* �̈�̃T�C�Y */
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

	/* �ݒ�̈ꎞ�ۑ��p */
	BOOL bSync;
	int nSyncInterval;
	int nVoiceInterval;
	int nVoiceVolume;

	TCHAR szModulePath[_MAX_PATH];
	TCHAR szShortcutPath[_MAX_PATH];

	//SHGetSpecialFolderPath(hWnd, szShortcutPath, CSIDL_STARTUP, TRUE);	/* required IE4 Desktop */

	switch (uMsg) {
	case WM_INITDIALOG:
		/* ���������ݒ� */
		if (FALSE == getClockPrivilege()) {
			/* �����C�������Ȃ��̂Ƃ� */
			EnableWindow(GetDlgItem(hWnd, IDC_CONFIG_SYNC), FALSE);		/* ���������ݒ薳�� */
		}
		if (g_Conf.bSync != FALSE) {
			/* ��������On�̂Ƃ� */
			CheckDlgButton(hWnd, IDC_CONFIG_SYNC, BST_CHECKED);
		}

		/* ���������Ԋu */
		SetDlgItemInt(hWnd, IDC_CONFIG_SYNCINTERVAL, g_Conf.nSyncInterval, FALSE);
		SendMessage(GetDlgItem(hWnd, IDC_CONFIG_SYNCINTERVAL), EM_SETLIMITTEXT, 4, 0);

		/* ����ǂݏグ�Ԋu */
		if (FALSE == existVoice()) {
			/* �����Ȃ��̂Ƃ� */
			EnableWindow(GetDlgItem(hWnd, IDC_CONFIG_VOICE60), FALSE);	/* ����ǂݏグ�ݒ薳�� */
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

		/* �����ǂݏグ���� */
		if (g_Conf.nVoiceVolume == nVoiceVolume_Low) {
			CheckDlgButton(hWnd, IDC_CONFIG_VOL_LOW, BST_CHECKED);
		}else if (g_Conf.nVoiceVolume == nVoiceVolume_Mid) {
			CheckDlgButton(hWnd, IDC_CONFIG_VOL_MID, BST_CHECKED);
		}else{
			CheckDlgButton(hWnd, IDC_CONFIG_VOL_HIGH, BST_CHECKED);
		}

		/* �����N���ݒ� */
		getShortcutPath(szShortcutPath, sizeof(szShortcutPath));
		if (NULL != (fp = fopen(szShortcutPath, "r"))) {
			/* �V���[�g�J�b�g�������On */
			CheckDlgButton(hWnd, IDC_CONFIG_STARTUP, BST_CHECKED);
			fclose(fp);
		}

	 	enableConfigDlg(hWnd);
		adjustWndPos(hWnd);
		return 1;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDC_CONFIG_NTP:
			/* NTP�ݒ�_�C�A���O���N�� */
			DialogBox(g_hInst, MAKEINTRESOURCE(IDD_SNTP), hWnd, NtpDlgProc);
			break;

		case IDOK:
			/* ���������ݒ� */
			if (BST_CHECKED == IsDlgButtonChecked(hWnd, IDC_CONFIG_SYNC)) {
				bSync = TRUE;
			}else{
				bSync = FALSE;
			}

			/* ���������Ԋu�`�F�b�N */
			nSyncInterval = GetDlgItemInt(hWnd, IDC_CONFIG_SYNCINTERVAL, &bTrans, FALSE);
			if (FALSE == bTrans) {
				showMessage(g_hInst, hWnd, IDS_CONF_INVALIDDIGIT, MB_OK | MB_ICONSTOP);
				break;
			}
			if (nSyncInterval <= 0 || nSyncInterval > 1440) {
				showMessage(g_hInst, hWnd, IDS_CONF_OUTOFRANGE, MB_OK | MB_ICONSTOP);
				break;
			}

			/* �����ǂݏグ�Ԋu�ݒ� */
			if (BST_CHECKED == IsDlgButtonChecked(hWnd, IDC_CONFIG_VOICE60)) {
				nVoiceInterval = 60;
			}else if (BST_CHECKED == IsDlgButtonChecked(hWnd, IDC_CONFIG_VOICE30)) {
				nVoiceInterval = 30;
			}else{
				nVoiceInterval = 0;
			}

			/* �����ǂݏグ���ʐݒ� */
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

			/* �����N���ݒ� */
			getShortcutPath(szShortcutPath, sizeof(szShortcutPath));
			if (BST_CHECKED == IsDlgButtonChecked(hWnd, IDC_CONFIG_STARTUP)) {
				/* �����N��On�̂Ƃ� */
				if (SUCCEEDED(CoInitialize(NULL))) {
					/* �X�^�[�g�A�b�v�ɃV���[�g�J�b�g�𐶐� */
					GetModuleFileName(NULL, szModulePath, sizeof(szModulePath));
					CreateShortcut(szShortcutPath, szModulePath, NULL, NULL, NULL, NULL, 0, SW_SHOWNORMAL);
					CoUninitialize();
				}
			}else{
				/* �����N��Off�̂Ƃ� */
				remove(szShortcutPath);		/* �V���[�g�J�b�g�폜 */
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
