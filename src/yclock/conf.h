#ifndef CONFIG_H
#define CONFIG_H

struct conf {
	char	szServer[INTERNET_MAX_HOST_NAME_LENGTH + 1];
					/* SNTP�T�[�o�z�X�g�� */
	BOOL	bSync;			/* ���������@�\ */
	int	nSyncInterval;		/* ���������Ԋu(�b) */
	int	nVoiceInterval;		/* ����ǂݏグ�Ԋu(��) */
	int	nVoiceVolume;		/* �����ǂݏグ�̉��� */
	int	nPosX;			/* �E�B���h�E�ʒu */
	int	nPosY;
	int	bAutoRun;		/* �����N�� */

	int	nMaxDelay;		/* ���e�x������(�~���b) */
	int	nTolerance;		/* ���e�덷(�~���b) */
	int	nTimeShift;		/* �����V�t�g����(�b) */
#ifdef DISPWND
	BOOL	bShow;			/* ���v�\�� */
	BOOL	bOnTop;			/* ��Ɏ�O�ɕ\�� */
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
