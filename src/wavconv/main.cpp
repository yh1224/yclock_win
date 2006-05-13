#include <windows.h>
#include <stdio.h>
#include <stdlib.h>



void *
mymalloc(size_t siz) {
	void *p;

	if (NULL == (p = malloc(siz))) {
		fprintf(stderr, "Not enough memory.\n");
		exit(EXIT_FAILURE);
	}
	return p;
}

void *
myrealloc(void *p1, size_t siz) {
	void * p;

	if (NULL == (p = realloc(p1, siz))) {
		fprintf(stderr, "Not enough memory.\n");
		exit(EXIT_FAILURE);
	}
	return p;
}


BOOL
getFullPath(char *filename, char *out, unsigned int size) {
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

BOOL
setCurrentDirRel(char *file)
{
	char path[MAX_PATH + 1];

	getFullPath(file, path, sizeof(path));
	return SetCurrentDirectory(path);
}


struct tbl {
	char name[12];
	DWORD start;
	DWORD size;
};

/*
======================================================================
dat→wav
----------------------------------------------------------------------
*/
BOOL
dat2wav(char *file, char *dir)
{
	FILE *fpin = NULL, *fpout = NULL;
	int rtn = FALSE;
	DWORD numTbl;
	int i;
	struct tbl *tbl;
	char *p = NULL;
	char filename[MAX_PATH + 1];

	getFullPath(file, filename, sizeof(filename));

	if (NULL == (fpin = fopen(filename, "rb"))) {
		fprintf(stderr, "Failed to open: %s\n", file);
		goto fail;
	}
	if (1 > fread(&numTbl, 4, 1, fpin)) {
		fprintf(stderr, "Failed to read file.\n");
		goto fail;
	}
	printf("Number of tables: %d\n", (int)numTbl);
	tbl = (struct tbl *)mymalloc(sizeof(struct tbl) * numTbl);
	if (numTbl > fread(tbl, sizeof(struct tbl), numTbl, fpin)) {
		fprintf(stderr, "Failed to read file.\n");
		goto fail;
	}

	for (i = 0; i < (int)numTbl; i++, tbl++) {
		char file[32];
		char path[MAX_PATH + 1];

		sprintf(file, "%s\\%s.wav", dir, tbl->name);
		getFullPath(file, path, sizeof(path));
		if (NULL == (fpout = fopen(path, "w"))) {
			fprintf(stderr, "Failed to create file: %s\n", path);
			goto fail;
		}
		printf("%s\n", tbl->name);
		p = (char *)mymalloc(tbl->size);
		if (0 != fseek(fpin, tbl->start, SEEK_SET)) {
			fprintf(stderr, "Failed to seek.\n");
			goto fail;
		}
		if (1 != fread(p, tbl->size, 1, fpin)) {
			fprintf(stderr, "Failed to read.\n");
			goto fail;
		}
		if (1 != fwrite(p, tbl->size, 1, fpout)) {
			fprintf(stderr, "Failed to write.\n");
			goto fail;
		}
		fclose(fpout);
		fpout = NULL;
	}

	rtn = TRUE;
fail:
	if (p != NULL) {
		free(p);
	}
	if (fpin != NULL) {
		fclose(fpin);
	}
	if (fpout != NULL) {
		fclose(fpout);
	}
	return rtn;
}

int
strUpper(char *ptr) {
	int i;

	for (i = 0; *ptr != '\0'; ptr++, i++) {
		*ptr = toupper(*ptr);
	}
	return i;
}

/* LISTチャンク以降を省く */
struct riff_tag {
	char tag[4];
	DWORD len;
};
int
stripRiff(char *pRiff, int siz) {
	struct riff_tag *pTag;
	int len;

	printf("        ");
	for (len = 12; len <= siz - 2; ) {
		pTag = (struct riff_tag *)(pRiff + len);
		printf("[%04x]%c%c%c%c/%04x ", len,
			pTag->tag[0], pTag->tag[1], pTag->tag[2], pTag->tag[3], pTag->len);
		if (strncmp(pTag->tag, "LIST", 4) == 0) {
			printf("found ");
			break;
		}
		/* ?? */
		if (strncmp(pTag->tag+1, "LIS", 3) == 0) {
			len -= 1;
			printf("found ");
			break;
		}
		len += sizeof(struct riff_tag) + pTag->len;
	}
	((struct riff_tag *)pRiff)->len = len - 8;
	printf("\n");
	return len;
}

BOOL
wav2dat(char *file, char *dir)
{
	FILE *fpin = NULL, *fpout = NULL;
	int rtn = FALSE;
	DWORD num, off;
	int i;
	struct tbl *tbl;
	char *p = NULL;
	char *pHdr = NULL, *pData = NULL;
	char path[MAX_PATH + 1];
	WIN32_FIND_DATA ffData;
	HANDLE hFind;
	int siz;

	getFullPath(file, path, sizeof(path));
	if (NULL == (fpout = fopen(path, "wb"))) {
		fprintf(stderr, "Failed to create file: %s\n", path);
		goto fail2;
	}

	pHdr = (char *)mymalloc(4);
	pData = (char *)mymalloc(1);

	if (FALSE == setCurrentDirRel(dir)) {
		fprintf(stderr, "Failed to find directory: %s\n", dir);
		goto fail2;
	}

	/* まずは1コめを探す */
	num = off = 0;
	if (NULL == (hFind = FindFirstFile("*.wav", &ffData))) {
		fprintf(stderr, "No wav file found.");
		goto fail2;
	}
	do {
		printf("%s\n", ffData.cFileName);

		pHdr = (char *)myrealloc(pHdr, 4 + (num + 1) * sizeof(struct tbl));
		tbl = (struct tbl *)((char *)pHdr + 4 + num * sizeof(struct tbl));

		/* ヘッダ: ファイル名 (3文字固定) */
		strncpy(tbl->name, ffData.cFileName, 3);
		memset(&tbl->name[3], 0, 9);
		strUpper(tbl->name);

		/* ヘッダ: 開始オフセット～サイズ (とりあえずデータ部の開始位置を0とする) */
		tbl->start = off;
		if (ffData.nFileSizeHigh != 0) {	/* 2G超え^^; */
			fprintf(stderr, "File size too big.");
			goto fail2;
		}

		/* データ */
		if (NULL == (fpin = fopen(ffData.cFileName, "rb"))) {
			fprintf(stderr, "Failed to open file: %s\n", path);
			goto fail2;
		}
		pData = (char *)myrealloc(pData, off + ffData.nFileSizeLow);
		if (1 != fread(pData + off, ffData.nFileSizeLow, 1, fpin)) {
			fprintf(stderr, "Failed to read file.\n");
			goto fail2;
		}
		siz = stripRiff(pData + off, ffData.nFileSizeLow);
		printf("        size: %08x -> %08x\n", ffData.nFileSizeLow, siz);

		/* ヘッダ: サイズ */
		tbl->size = siz;

		off += siz;
		num++;

	}while (FALSE != FindNextFile(hFind, &ffData));
	FindClose(hFind);

	printf("Total number: %d\n", num);
	printf("Total header size: 0x%x\n", 4 + num * sizeof(struct tbl));
	printf("Total data size: 0x%x\n", off);

	/* 書き込み */
	*((DWORD *)pHdr) = num;
	for (i = 0; i < (int)num; i++) {
		tbl = (struct tbl *)((char *)pHdr + 4 + i * sizeof(struct tbl));
		tbl->start += 4 + num * sizeof(struct tbl);	/* ヘッダ部の長さを足してゆく */
	}
	if (1 != fwrite(pHdr, 4 + num * sizeof(struct tbl), 1, fpout)) {
		fprintf(stderr, "Failed to write file.\n");
		goto fail2;
	}
	if (1 != fwrite(pData, off, 1, fpout)) {
		fprintf(stderr, "Failed to write file.\n");
		goto fail2;
	}

	rtn = TRUE;
fail2:
	if (pHdr != NULL) {
		free(pHdr);
	}
	if (pData != NULL) {
		free(pData);
	}
	if (fpin != NULL) {
		fclose(fpin);
	}
	if (fpout != NULL) {
		fclose(fpout);
	}
	return rtn;
}



void
main(int argc, char *argv[])
{
	wav2dat("voice.dat", "voice");
	//dat2wav("voice.dat", "voice");
	exit(EXIT_SUCCESS);
}



