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
#define SYNC_MAX_RETRY		5	/* ���g���C�� */

/* arguments for sync */
struct sync_param {
	BOOL	bSync;		/* ���ۂɎ�����ݒ肷�邩�ǂ��� */
	char	*szServer;	/* �T�[�o�z�X�g�� */
	int	nMaxDelay;	/* ���e�x������(�~���b) */
	int	nTimeShift;	/* �����V�t�g(�b) */
};

char *getLastSync();
BOOL syncClock(HWND, struct sync_param *, BOOL);

#endif /* SNTP_H */
