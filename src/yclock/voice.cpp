#include <windows.h>
#include <stdio.h>
#include "debug.h"
#include "misc.h"
#include "voice.h"

static char *pszVoiceFilename = "voice.dat";

struct tbl {
	char name[12];
	DWORD start;
	DWORD size;
};

struct riff_tag {
	char tag[4];
	DWORD len;
};

struct riff_hdr {
	char tag[4];
	DWORD len;
	char type[4];
};

struct riff_fact {
	struct riff_tag riff;
	DWORD fact;
};

struct fmt_chunk {
	short		chunkID;
	long		chunkSize; 
	short		wFormatTag;
	unsigned short	wChannels;
	unsigned long	dwSamplesPerSec;
	unsigned long	dwAvgBytesPerSec;
	unsigned short	wBlockAlign;
	unsigned short	wBitsPerSample;
};


/* 指定したタグ部へのポインタを取得 */
static char *
riffSearchTag(char *pRiff, char *pszTag)
{
	struct riff_tag *rt;
	DWORD len;

	rt = (struct riff_tag*)pRiff;
	if (0 != strncmp(rt->tag, "RIFF", 4)) {
		/* フォーマットエラー */
		SYSLOG((LOG_ERR, "RIFF format error."));
		return NULL;
	}
	len = rt->len;	/* 領域のサイズ。もはやこれが狂ってると落ちるかも */
	rt = (struct riff_tag*)((char *)pRiff + sizeof(struct riff_hdr));

	while (0 != strncmp(rt->tag, pszTag, 4)) {
		if (len < rt->len + sizeof(struct riff_tag)) {
			return NULL;	/* Out of bound */
		}
		len -= (sizeof(struct riff_tag) + rt->len);
		rt = (struct riff_tag*)((char*)rt + sizeof(struct riff_tag) + rt->len);
	}

	return (char *)rt;
}

/*
2つのwavをマージする。data以外のタグは前者のまま。
RIFFヘッダとdataのサイズを調整する。
malloc された領域であること。成功すると p2 は解放される
*/
static char *
wavMerge(char *p1, char *p2)
{
	char *p_new, *p1_data, *p2_data;
	DWORD len1, len2;

	SYSLOG_DUMP((LOG_DEBUG, p1, 48));
	SYSLOG_DUMP((LOG_DEBUG, p2, 48));

	/* RIFFフォーマットチェック */
	if (0 != strncmp(((struct riff_tag*)p1)->tag, "RIFF", 4)
			|| 0 != strncmp(((struct riff_tag*)p2)->tag, "RIFF", 4)) {
		SYSLOG((LOG_ERR, "RIFF format error."));
		return NULL;
	}

	if (NULL == (p2_data = riffSearchTag(p2, "data"))) {
		return NULL;
	}

	/* len1はRIFFヘッダのlen */
	len1 = ((struct riff_hdr*)p1)->len;
	/* len2はdata部のみの長さ(RIFFタグ含まない) */
	len2 = ((struct riff_hdr*)p2)->len - (p2_data - p2);

	/* 真奈美くろっくはちょいゴミ有りなので対処 */
	if (*(p1 + len1 + 7) == 0 && *(p1 + len1 + 6) == (char)0x80) {
		*(p1 + len1 + 7) = (char)0x80;
	}

	/* p1の最後にp2のdataをマージ */
	if (NULL == (p_new = (char*)realloc(p1, sizeof(struct riff_tag) + len1 + len2))) {
		return NULL;
	}
	memcpy(p_new + 8 + len1, p2_data + sizeof(struct riff_tag), len2);
	((struct riff_hdr*)p_new)->len = len1 + len2;
	if (NULL == (p1_data = riffSearchTag(p_new, "data"))) {
		return NULL;
	}
	((struct riff_tag*)p1_data)->len += len2;

	SYSLOG_DUMP((LOG_DEBUG, p_new, 64));

	free(p2);
	return p_new;
}

/* ボリューム調整 */
static char *
reduceVolume(char *pRiff, int vol)
{
	char *pData;
	struct fmt_chunk *pFmt;
	DWORD i, len;
	short *sp;
	unsigned char *ucp;

	pFmt = (struct fmt_chunk *)riffSearchTag(pRiff, "fmt ");
	pData = riffSearchTag(pRiff, "data");
	if (pFmt == NULL || pData == NULL) {
		return NULL;
	}

	len = ((struct riff_tag *)pData)->len - sizeof(struct riff_tag);
	pData += sizeof(struct riff_tag);
	SYSLOG_DUMP((LOG_DEBUG, pData, 48));

	if (pFmt->wBitsPerSample == 16) {
		/* 16bit */
		sp = (short *)pData;
		for (i = 0; i < len; i += 2) {
			*sp = (short)(*sp * ((double)vol / 100));
			sp++;
		}
	}else if (pFmt->wBitsPerSample == 8) {
		/* 8bit */
		ucp = (unsigned char *)pData;
		for (i = 0; i < len; i++) {
			*ucp = (unsigned char)((*ucp - 128) * ((double)vol / 100) + 128);
			ucp++;
		}
	}

	SYSLOG_DUMP((LOG_DEBUG, pData, 48));

	return pRiff;
}



/* 時刻読み上げ */
BOOL
playVoice(char *voice, int count, int vol) {
	BOOL rtn = TRUE;
	FILE *fp;
	char filepath[MAX_PATH + 1];
	DWORD numTbl;
	struct tbl tbl;
	char *pSound = NULL;
	int i, j;

	getFullPath(pszVoiceFilename, filepath, sizeof(filepath));

	if (0 != fopen_s(&fp, filepath, "rb")) {
		SYSLOG((LOG_ERR, "Failed to open voice file."));
		return FALSE;
	}

	if (fread(&numTbl, 4, 1, fp) < 1) {
		SYSLOG((LOG_ERR, "Failed to read file."));
		rtn = FALSE;
	}else{
		for (i = 0; i < count; i++) {
			for (j = 0; j < (int)numTbl; j++) {
				if (fread(&tbl, 20, 1, fp) == 1) {
					if (0 == strcmp(voice, tbl.name)) {
						char *p;
						if (NULL != (p = (char*)malloc(tbl.size))) {
							if (0 == fseek(fp, tbl.start, SEEK_SET)) {
								if (1 == fread(p, tbl.size, 1, fp)) {
									if (pSound != NULL) {
										/* 2コめ以降 */
										char *p2;
										if (NULL == (p2 = wavMerge(pSound, p))) {
											SYSLOG((LOG_ERR, "Failed to merge voice."));
											free(p);
										}
										pSound = p2;
									}else{
										pSound = p;
									}
								}else{
									SYSLOG((LOG_ERR, "Failed to read voice."));
									rtn = FALSE;
								}
							}else{
								SYSLOG((LOG_ERR, "Failed to seek."));
								rtn = FALSE;
							}
						}else{
							SYSLOG((LOG_ERR, "Failed to alloc memory."));
							rtn = FALSE;
						}
						break;
					}
				}else{
					SYSLOG((LOG_ERR, "Failed to read table."));
					rtn = FALSE;
					break;
				}
			}
			if (j == (int)numTbl) {	/* 見つからない */
				SYSLOG((LOG_ERR, "Voice name %s not found.", voice));
				rtn = FALSE;
				break;
			}
			voice += strlen(voice) + 1;
			fseek(fp, 4, SEEK_SET);
		}
	}

	if (pSound != NULL) {
		/* 音量調節 */
		SYSLOG((LOG_DEBUG, "Volume = %d", vol));
		if (vol < 100) {
			if (NULL == reduceVolume(pSound, vol)) {
				rtn = FALSE;
			}
		}
		if (rtn != FALSE) {
			if (FALSE == PlaySound((LPCSTR)pSound, NULL, SND_MEMORY)) {
				SYSLOG((LOG_ERR, "Playing voice failed."));
			}
		}
		free(pSound);
	}

	fclose(fp);
	return rtn;
}

/* 現在時刻読み上げ */
BOOL
playCurrentTime(int vol) {
	SYSTEMTIME st;
	char voice[8];
	int hour;

	GetLocalTime(&st);

	hour = st.wHour;
	if (hour >= 12) {
		hour -= 12;
	}
	wsprintf(voice, "H%02d", hour);
	wsprintf(voice + 4, "M%02d", st.wMinute);

	return playVoice(voice, 2, vol);
}

BOOL
playVoiceTest(int vol) {
	return playVoice("H00\0M00", 2, vol);
}

/* 音声ファイルがあるかどうか */
BOOL
existVoice() {
	char path[MAX_PATH + 1];
	HANDLE hf;

	if (FALSE == getFullPath(pszVoiceFilename, path, sizeof(path))) {
		return FALSE;
	}else{
		if (INVALID_HANDLE_VALUE == (hf = CreateFile(path, 0, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL))) {
			SYSLOG((LOG_INFO, "%s not found.", path));
			return FALSE;
		}else{
			SYSLOG((LOG_INFO, "%s found.", path));
			CloseHandle(hf);
		}
	}
	return TRUE;
}
