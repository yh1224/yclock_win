#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <shlobj.h>
#include "misc.h"
#include "debug.h"

extern HINSTANCE g_hInst;


/*
======================================================================
レジストリ操作用
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
プラットフォームがWin32NTかどうかを取得する
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
時刻修正の権限を得る (NT only, その他のOSは常にTRUEを返す)
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
		/* Win9x系は常に TRUE */
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
ウインドウを画面内に収める。
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
ウインドウをマウスカーソル付近に表示する
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
ウインドウを画面中央へ
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
ウインドウを画面逆サイドに移動
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
BITMAPからリージョンを作成
----------------------------------------------------------------------
*/
HRGN
createRgnFromBmp(HBITMAP hBitmap)
{
	BITMAP bm;
	HBITMAP hbmDIB = NULL;
	COLORREF* pBits = NULL;
	HRGN hRgn = CreateRectRgn(0, 0, 0, 0);	/* 生成するリージョン */

	HDC hdcMem;
	HDC hdcCopy;
	HBITMAP hbmDIBOld;
	HBITMAP hbmDDBOld;
	BITMAPINFOHEADER bmih;

	if (NULL == hBitmap) {
		return NULL;
	}

	/* ビットマップのサイズを取得 */
	if (NULL == GetObject(hBitmap, sizeof(bm), &bm)) {
		return NULL;
	}

	/* DIBの作成
		成功すると hbmDIB にビットマップハンドル、pBits にビットへの
		ポインタが設定される。(DeleteObject(hbmDIB); を忘れてはいけない)
		失敗すると hbmDIB 及び pBits は NULL のまま。
	*/

	if (NULL != (hdcMem = CreateCompatibleDC(NULL))) {

		/* DIBヘッダ情報 */
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

		/* DIBの作成 */
		if (NULL != (hbmDIB = CreateDIBSection(
				hdcMem, (CONST BITMAPINFO*)&bmih, DIB_RGB_COLORS, (void**)&pBits, NULL, 0))) {

			/* 作成したDIBをDCに選択 */
			hbmDIBOld = (HBITMAP)SelectObject(hdcMem, hbmDIB);

			/* 画像の転送元となるメモリデバイスコンテキストを作成 */
			if (NULL != (hdcCopy = CreateCompatibleDC(hdcMem))) {

				/* hBitmap で指定されたビットマップをデバイスコンテキストに選択 */
				hbmDDBOld = (HBITMAP)SelectObject(hdcCopy, hBitmap);

				/* ビットマップをhdcMemに転送 */
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
	/* DIBの作成に失敗していたらリージョンは作れない */
	if (hbmDIB == NULL) {
		return NULL;
	}

	/* リージョンの作成 */
	{
		COLORREF cTransparentColor;
		HRGN hDotRgn;       /* 一時的なドットのリージョン */
		int xx;             /* x の値を保存 */

		/* 左下隅のドットの色を透明色とする */
		cTransparentColor = *pBits;

		/* 縦横にビットが透明色かを調べ、透明色で無い部分をリージョンに追加 */
		for (int y = 0; y < bm.bmHeight; y++) {
			for (int x = 0; x < bm.bmWidth; x++) {
				if (*pBits != cTransparentColor) {   /* 透明色ではない */
					xx = x;		/* x を保存(透明色ではない点の始まりを保存) */
					/* 連続した透明色ではない領域を求める */
					for (x = x + 1; x < bm.bmWidth; x++) {
						/* ポインタをインクリメント */
						pBits++;

						/* 透明色になったら break */
						if (*pBits == cTransparentColor) {
							break;
						}
					}

					/* 一時的な透明色でない領域のリージョンを作成 */
					hDotRgn = CreateRectRgn(xx, bm.bmHeight-y, x, bm.bmHeight - y - 1);
					/* hRgn と合成(ビットマップ全体のリージョンを作成) */
					CombineRgn(hRgn, hRgn, hDotRgn, RGN_OR);
					/* 一時的なリージョンを削除 */
					DeleteObject(hDotRgn);
				}
				/* ポインタをインクリメント */
				pBits++;
			}
		}
	}

	DeleteObject(hbmDIB);
	return hRgn;
}



/*
======================================================================
モジュールのディレクトリにあるファイルのフルパスを取得
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
カレントをモジュールのディレクトリから相対指定で変更する
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
ショートカットを作成する
----------------------------------------------------------------------
*/
HRESULT
CreateShortcut(
	LPSTR pszLink,				// ショートカットの絶対パス
	LPSTR pszFile,				// ターゲットファイル
	LPSTR pszDescription = NULL,		// 説明
	LPSTR pszArgs	     = NULL,		// 引数
	LPSTR pszWorkingDir  = NULL,		// 作業ディレクトリ
	LPSTR pszIconPath    = NULL,		// アイコンの場所
	int iIcon	     = 0,		// アイコンのインデックス
	int iShowCmd	     = SW_SHOWNORMAL)	// ウィンドウスタイル
{
	HRESULT hres;
	IShellLink *psl;

	// IShellLink オブジェクトを作成しポインタを取得する
	hres = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER,
						IID_IShellLink, (void **)&psl);
	if (SUCCEEDED(hres)) {
		IPersistFile *ppf;

		// ショートカットを二次記憶装置に保存するため IPersistFile
		// インターフェイスの問い合わせをおこなう
		hres = psl->QueryInterface(IID_IPersistFile, (void **)&ppf);
		if (SUCCEEDED(hres)) {
			WORD wsz[MAX_PATH];   // Unicode 文字列へのバッファ

			psl->SetPath(pszFile);			// ターゲットファイル
			psl->SetDescription(pszDescription);	// 説明
			psl->SetArguments(pszArgs); 		// 引数
			psl->SetWorkingDirectory(pszWorkingDir);	// 作業ディレクトリ
			psl->SetIconLocation(pszIconPath, iIcon);	// アイコン
			psl->SetShowCmd(iShowCmd);			// ウィンドウスタイル

			// 文字列がANSI文字で構成されるようにする
			MultiByteToWideChar(CP_ACP, 0, pszLink, -1, wsz, MAX_PATH);

			// ショートカットを保存する
			hres = ppf->Save(wsz, TRUE);

			// IPersistFile へのポインタを開放する
			ppf->Release();
		}
		// IShellLinkへのポインタを開放する
		psl->Release();
	}
	return hres;
}



/*
======================================================================
メッセージを表示する (256文字まで)
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
