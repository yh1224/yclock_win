#ifndef SNTP_H
#define SNTP_H

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
