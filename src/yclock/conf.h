#ifndef CONFIG_H
#define CONFIG_H

struct conf {
	char	szServer[INTERNET_MAX_HOST_NAME_LENGTH + 1];
					/* SNTPサーバホスト名 */
	BOOL	bSync;			/* 時刻同期機能 */
	int	nSyncInterval;		/* 時刻同期間隔(秒) */
	int	nVoiceInterval;		/* 時報読み上げ間隔(分) */
	int	nVoiceVolume;		/* 時刻読み上げの音量 */
	int	nPosX;			/* ウィンドウ位置 */
	int	nPosY;
	int	bAutoRun;		/* 自動起動 */

	int	nMaxDelay;		/* 許容遅延時間(ミリ秒) */
	int	nTolerance;		/* 許容誤差(ミリ秒) */
	int	nTimeShift;		/* 時刻シフト時間(秒) */
#ifdef DISPWND
	BOOL	bShow;			/* 時計表示 */
	BOOL	bOnTop;			/* 常に手前に表示 */
#endif
};

extern struct conf g_Conf;

void conf_init();
BOOL conf_save();
BOOL conf_load();
BOOL conf_setPosWnd(HWND);
BOOL conf_setWnd(HWND);

BOOL CALLBACK NtpDlgProc(HWND, UINT, WPARAM, LPARAM);
BOOL CALLBACK ConfigDlgProc(HWND, UINT, WPARAM, LPARAM);

#endif /* CONFIG_H */
