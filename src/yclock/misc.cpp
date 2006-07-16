#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <shlobj.h>
#include "misc.h"
#include "debug.h"

extern HINSTANCE g_hInst;


/*
======================================================================
���W�X�g������p
----------------------------------------------------------------------
*/
BOOL
RegGetBool(HKEY hkReg, char *szKey)
{
	DWORD typ, siz;

	RegQueryValueEx(hkReg, szKey, 0, &typ, NULL, (LPDWORD)&siz);
	if (typ == REG_SZ) {
		return TRUE;
	}else{
		return FALSE;
	}
}
LONG
RegSetBool(HKEY hkReg, char *szKey, BOOL bVal)
{
	if (bVal != FALSE) {
		return RegSetValueEx(hkReg, szKey, 0, REG_SZ, (BYTE*)"", 0);
	}else{
		return RegDeleteValue(hkReg, szKey);
	}
}

BOOL
RegGetDW(HKEY hkReg, char *szKey, DWORD *dwVal)
{
	DWORD val;
	DWORD siz = sizeof(DWORD);

	if (ERROR_SUCCESS == RegQueryValueEx(hkReg, szKey, 0, NULL, (BYTE*)&val, (LPDWORD)&siz)) {
		*dwVal = val;
		return TRUE;
	}else{
		return FALSE;
	}
}



/*
======================================================================
�v���b�g�t�H�[����Win32NT���ǂ������擾����
----------------------------------------------------------------------
*/
BOOL
isPlatformWin32NT()
{
	OSVERSIONINFO osVer;

	osVer.dwOSVersionInfoSize = sizeof(osVer);
	GetVersionEx(&osVer);
	if (osVer.dwPlatformId == VER_PLATFORM_WIN32_NT) {
		return TRUE;
	}
	return FALSE;
}



/*
======================================================================
�����C���̌����𓾂� (NT only, ���̑���OS�͏��TRUE��Ԃ�)
----------------------------------------------------------------------
*/
BOOL
getClockPrivilege()
{
	BOOL result;
	HANDLE hToken;
	TOKEN_PRIVILEGES pr;

	result = FALSE;
	if (FALSE != isPlatformWin32NT()) {
		if (OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken) != 0) {
			if (LookupPrivilegeValue(NULL, SE_SYSTEMTIME_NAME, &pr.Privileges[0].Luid) != 0) {
				pr.PrivilegeCount = 1;
				pr.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
				if (AdjustTokenPrivileges(hToken, FALSE, &pr, 0, NULL, NULL) != FALSE &&
				    GetLastError() == ERROR_SUCCESS) {
					SYSLOG((LOG_INFO, "AdjustTokenPrivileges() suceeded."));
					result = TRUE;
				}
			}
			CloseHandle(hToken);
		}
	}else{
		/* Win9x�n�͏�� TRUE */
		result = TRUE;
	}
#ifdef _DEBUG
	if (result == FALSE) {
		SYSLOG((LOG_INFO, "No clock privilege."));
	}
#endif
	return result;
}



/*
======================================================================
�E�C���h�E����ʓ��Ɏ��߂�B
----------------------------------------------------------------------
*/
BOOL
fixWndPos(HWND hWnd)
{
	RECT rectWnd, rectDesk;

	if (FALSE == SystemParametersInfo(SPI_GETWORKAREA, 0, &rectDesk, 0)) {
		return FALSE;
	}
	if (FALSE == GetWindowRect(hWnd, &rectWnd)) {
		return FALSE;
	}
	if (rectWnd.left + rectWnd.right - rectWnd.left > rectDesk.right) {
		rectWnd.left = rectDesk.right - (rectWnd.right - rectWnd.left) - 2;
	}
	if (rectWnd.left < 0) {
		rectWnd.left = 2;
	}
	if (rectWnd.top + rectWnd.bottom - rectWnd.top > rectDesk.bottom) {
		rectWnd.top = rectDesk.bottom - (rectWnd.bottom - rectWnd.top) - 2;
	}
	if (rectWnd.top < 0) {
		rectWnd.top = 2;
	}
	if (FALSE == SetWindowPos(hWnd, 0, rectWnd.left, rectWnd.top, 0, 0, SWP_NOSIZE | SWP_NOZORDER)) {
		return FALSE;
	}
	return TRUE;
}



/*
======================================================================
�E�C���h�E���}�E�X�J�[�\���t�߂ɕ\������
----------------------------------------------------------------------
*/
BOOL
adjustWndPos(HWND hWnd)
{
	POINT point;
	RECT rectWnd, rectDesk;

	if (FALSE == SystemParametersInfo(SPI_GETWORKAREA, 0, &rectDesk, 0)) {
		return FALSE;
	}
	if (FALSE == GetWindowRect(hWnd, &rectWnd)) {
		return FALSE;
	}
	if (FALSE == GetCursorPos(&point)) {
		return FALSE;
	}
	if (point.x + rectWnd.right - rectWnd.left > rectDesk.right) {
		point.x = rectDesk.right - (rectWnd.right - rectWnd.left) - 8;
	}
	if (point.y + rectWnd.bottom - rectWnd.top > rectDesk.bottom) {
		point.y = rectDesk.bottom - (rectWnd.bottom - rectWnd.top) - 8;
	}
	if (FALSE == SetWindowPos(hWnd, 0, point.x, point.y, 0, 0, SWP_NOSIZE | SWP_NOZORDER)) {
		return FALSE;
	}
	return TRUE;
}



/*
======================================================================
�E�C���h�E����ʒ�����
----------------------------------------------------------------------
*/
BOOL
centeringWndPos(HWND hWnd)
{
	RECT rectWnd, rectDesk;

	if (FALSE == SystemParametersInfo(SPI_GETWORKAREA, 0, &rectDesk, 0)) {
		return FALSE;
	}
	if (FALSE == GetWindowRect(hWnd, &rectWnd)) {
		return FALSE;
	}
	SetWindowPos(hWnd, 0,
		rectDesk.right / 2 - (rectWnd.right - rectWnd.left) / 2,
		rectDesk.bottom / 2 - (rectWnd.bottom - rectWnd.top) / 2,
		0, 0, SWP_NOSIZE | SWP_NOZORDER);
	return TRUE;
}



/*
======================================================================
�E�C���h�E����ʋt�T�C�h�Ɉړ�
----------------------------------------------------------------------
*/
BOOL
awayWndPos(HWND hWnd)
{
	RECT rectWnd, rectDesk;

	if (FALSE == SystemParametersInfo(SPI_GETWORKAREA, 0, &rectDesk, 0)) {
		return FALSE;
	}
	if (FALSE == GetWindowRect(hWnd, &rectWnd)) {
		return FALSE;
	}
	SetWindowPos(hWnd, 0,
		rectDesk.right - rectWnd.right,
		rectDesk.bottom - rectWnd.bottom,
		0, 0, SWP_NOSIZE | SWP_NOZORDER);
	return TRUE;
}



/*
======================================================================
BITMAP���烊�[�W�������쐬
----------------------------------------------------------------------
*/
HRGN
createRgnFromBmp(HBITMAP hBitmap)
{
	BITMAP bm;
	HBITMAP hbmDIB = NULL;
	COLORREF* pBits = NULL;
	HRGN hRgn = CreateRectRgn(0, 0, 0, 0);	/* �������郊�[�W���� */

	HDC hdcMem;
	HDC hdcCopy;
	HBITMAP hbmDIBOld;
	HBITMAP hbmDDBOld;
	BITMAPINFOHEADER bmih;

	if (NULL == hBitmap) {
		return NULL;
	}

	/* �r�b�g�}�b�v�̃T�C�Y���擾 */
	if (NULL == GetObject(hBitmap, sizeof(bm), &bm)) {
		return NULL;
	}

	/* DIB�̍쐬
		��������� hbmDIB �Ƀr�b�g�}�b�v�n���h���ApBits �Ƀr�b�g�ւ�
		�|�C���^���ݒ肳���B(DeleteObject(hbmDIB); ��Y��Ă͂����Ȃ�)
		���s����� hbmDIB �y�� pBits �� NULL �̂܂܁B
	*/

	if (NULL != (hdcMem = CreateCompatibleDC(NULL))) {

		/* DIB�w�b�_��� */
		bmih.biSize = sizeof(BITMAPINFOHEADER),
		bmih.biWidth = bm.bmWidth;
		bmih.biHeight = bm.bmHeight;
		bmih.biPlanes = 1;
		bmih.biBitCount = 32;
		bmih.biCompression = BI_RGB;
		bmih.biSizeImage = 0;
		bmih.biXPelsPerMeter = 0;
		bmih.biYPelsPerMeter = 0;
		bmih.biClrUsed = 0;
		bmih.biClrImportant = 0;

		/* DIB�̍쐬 */
		if (NULL != (hbmDIB = CreateDIBSection(
				hdcMem, (CONST BITMAPINFO*)&bmih, DIB_RGB_COLORS, (void**)&pBits, NULL, 0))) {

			/* �쐬����DIB��DC�ɑI�� */
			hbmDIBOld = (HBITMAP)SelectObject(hdcMem, hbmDIB);

			/* �摜�̓]�����ƂȂ郁�����f�o�C�X�R���e�L�X�g���쐬 */
			if (NULL != (hdcCopy = CreateCompatibleDC(hdcMem))) {

				/* hBitmap �Ŏw�肳�ꂽ�r�b�g�}�b�v���f�o�C�X�R���e�L�X�g�ɑI�� */
				hbmDDBOld = (HBITMAP)SelectObject(hdcCopy, hBitmap);

				/* �r�b�g�}�b�v��hdcMem�ɓ]�� */
				if (NULL == BitBlt(hdcMem, 0, 0, bm.bmWidth, bm.bmHeight, hdcCopy, 0, 0, SRCCOPY)) {
					DeleteObject(hbmDIB);
					hbmDIB = NULL;
					pBits = NULL;
				}

				if (hbmDDBOld != NULL) {
					SelectObject(hdcCopy, hbmDDBOld);
				}
				DeleteDC(hdcCopy);
			}
			if (hbmDIBOld != NULL) {
				SelectObject(hdcMem, hbmDIBOld);
			}
		}
		DeleteDC(hdcMem);
	}
	/* DIB�̍쐬�Ɏ��s���Ă����烊�[�W�����͍��Ȃ� */
	if (hbmDIB == NULL) {
		return NULL;
	}

	/* ���[�W�����̍쐬 */
	{
		COLORREF cTransparentColor;
		HRGN hDotRgn;       /* �ꎞ�I�ȃh�b�g�̃��[�W���� */
		int xx;             /* x �̒l��ۑ� */

		/* �������̃h�b�g�̐F�𓧖��F�Ƃ��� */
		cTransparentColor = *pBits;

		/* �c���Ƀr�b�g�������F���𒲂ׁA�����F�Ŗ������������[�W�����ɒǉ� */
		for (int y = 0; y < bm.bmHeight; y++) {
			for (int x = 0; x < bm.bmWidth; x++) {
				if (*pBits != cTransparentColor) {   /* �����F�ł͂Ȃ� */
					xx = x;		/* x ��ۑ�(�����F�ł͂Ȃ��_�̎n�܂��ۑ�) */
					/* �A�����������F�ł͂Ȃ��̈�����߂� */
					for (x = x + 1; x < bm.bmWidth; x++) {
						/* �|�C���^���C���N�������g */
						pBits++;

						/* �����F�ɂȂ����� break */
						if (*pBits == cTransparentColor) {
							break;
						}
					}

					/* �ꎞ�I�ȓ����F�łȂ��̈�̃��[�W�������쐬 */
					hDotRgn = CreateRectRgn(xx, bm.bmHeight-y, x, bm.bmHeight - y - 1);
					/* hRgn �ƍ���(�r�b�g�}�b�v�S�̂̃��[�W�������쐬) */
					CombineRgn(hRgn, hRgn, hDotRgn, RGN_OR);
					/* �ꎞ�I�ȃ��[�W�������폜 */
					DeleteObject(hDotRgn);
				}
				/* �|�C���^���C���N�������g */
				pBits++;
			}
		}
	}

	DeleteObject(hbmDIB);
	return hRgn;
}



/*
======================================================================
���W���[���̃f�B���N�g���ɂ���t�@�C���̃t���p�X���擾
----------------------------------------------------------------------
*/
BOOL
getFullPath(const char *filename, char *out, unsigned int size)
{
	char path[MAX_PATH + 1];
	char *p;

	GetModuleFileName(NULL, path, sizeof(path));
	if (NULL != (p = strrchr(path, '\\'))) {
		*p = '\0';
	}
	strcat(path, "\\");
	strcat(path, filename);
	if (strlen(path) < size) {
		strcpy(out, path);
		return TRUE;
	}
	return FALSE;
}



/*
======================================================================
�J�����g�����W���[���̃f�B���N�g�����瑊�Ύw��ŕύX����
----------------------------------------------------------------------
*/
BOOL
setCurrentDirRel(char *file)
{
	char path[MAX_PATH + 1];

	getFullPath(file, path, sizeof(path));
	return SetCurrentDirectory(path);
}



/*
======================================================================
�V���[�g�J�b�g���쐬����
----------------------------------------------------------------------
*/
HRESULT
CreateShortcut(
	LPSTR pszLink,				// �V���[�g�J�b�g�̐�΃p�X
	LPSTR pszFile,				// �^�[�Q�b�g�t�@�C��
	LPSTR pszDescription = NULL,		// ����
	LPSTR pszArgs	     = NULL,		// ����
	LPSTR pszWorkingDir  = NULL,		// ��ƃf�B���N�g��
	LPSTR pszIconPath    = NULL,		// �A�C�R���̏ꏊ
	int iIcon	     = 0,		// �A�C�R���̃C���f�b�N�X
	int iShowCmd	     = SW_SHOWNORMAL)	// �E�B���h�E�X�^�C��
{
	HRESULT hres;
	IShellLink *psl;

	// IShellLink �I�u�W�F�N�g���쐬���|�C���^���擾����
	hres = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER,
						IID_IShellLink, (void **)&psl);
	if (SUCCEEDED(hres)) {
		IPersistFile *ppf;

		// �V���[�g�J�b�g��񎟋L�����u�ɕۑ����邽�� IPersistFile
		// �C���^�[�t�F�C�X�̖₢���킹�������Ȃ�
		hres = psl->QueryInterface(IID_IPersistFile, (void **)&ppf);
		if (SUCCEEDED(hres)) {
			WORD wsz[MAX_PATH];   // Unicode ������ւ̃o�b�t�@

			psl->SetPath(pszFile);			// �^�[�Q�b�g�t�@�C��
			psl->SetDescription(pszDescription);	// ����
			psl->SetArguments(pszArgs); 		// ����
			psl->SetWorkingDirectory(pszWorkingDir);	// ��ƃf�B���N�g��
			psl->SetIconLocation(pszIconPath, iIcon);	// �A�C�R��
			psl->SetShowCmd(iShowCmd);			// �E�B���h�E�X�^�C��

			// ������ANSI�����ō\�������悤�ɂ���
			MultiByteToWideChar(CP_ACP, 0, pszLink, -1, wsz, MAX_PATH);

			// �V���[�g�J�b�g��ۑ�����
			hres = ppf->Save(wsz, TRUE);

			// IPersistFile �ւ̃|�C���^���J������
			ppf->Release();
		}
		// IShellLink�ւ̃|�C���^���J������
		psl->Release();
	}
	return hres;
}



/*
======================================================================
���b�Z�[�W��\������ (256�����܂�)
----------------------------------------------------------------------
*/
void
showMessage(
	HINSTANCE	hInstance,
	HWND		hWnd,
	UINT		uID,
	UINT		nType,
	...
)
{
	va_list args;
	char msg[256];
	char fmt[256];

	va_start(args, nType);
	LoadString(hInstance, uID, fmt, sizeof(fmt));
	vsprintf(msg, fmt, args);
	MessageBox(hWnd, msg, NULL, nType);
	va_end(args);

	return;
}
