#ifndef SNTP_H
#define SNTP_H

/* Sync Retry */
#define SYNC_MAX_RETRY		5	/* リトライ回数 */

/* arguments for sync */
struct sync_param {
	BOOL	bSync;		/* 実際に時刻を設定するかどうか */
	char	*szServer;	/* サーバホスト名 */
	int	nMaxDelay;	/* 許容遅延時間(ミリ秒) */
	int	nTimeShift;	/* 時刻シフト(秒) */
};

char *getLastSync();
BOOL syncClock(HWND, struct sync_param *, BOOL);

#endif /* SNTP_H */
