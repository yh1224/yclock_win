#ifndef MISC_H
#define MISC_H

BOOL isNum(char *, BOOL);

BOOL RegGetBool(HKEY, char *);
LONG RegSetBool(HKEY, char *, BOOL);
DWORD RegGetDW(HKEY, char *);

BOOL isPlatformWin32NT();
BOOL getClockPrivilege();
BOOL fixWndPos(HWND);
BOOL adjustWndPos(HWND);
BOOL centeringWndPos(HWND);
BOOL awayWndPos(HWND);

HRGN createRgnFromBmp(HBITMAP);

BOOL getFullPath(const char *, char *, unsigned int);

HRESULT CreateShortcut(LPSTR, LPSTR, LPSTR, LPSTR, LPSTR, LPSTR, int, int);
void showMessage(HINSTANCE, HWND, UINT, UINT, ...);

#endif
