#ifndef SNTP_H
#define SNTP_H

/*
 * NTP Timestamp
 */
struct timestamp {
	unsigned long	integer;
	unsigned long	fraction;
};

/* NTP packet */
struct pkt {
	u_char			li_vn_mode;
	u_char			stratum;
	u_char			ppoll;
	char			precision;
	unsigned long		rootdelay;
	unsigned long		rootdispersion;
	unsigned long		refid;
	struct timestamp	reftime;
	struct timestamp	org;
	struct timestamp	rec;
	struct timestamp	xmt;
};

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
