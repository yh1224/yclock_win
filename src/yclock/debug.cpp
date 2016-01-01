#ifdef _DEBUG
#include <windows.h>
#include <stdio.h>
#include <time.h>
#include "misc.h"
#include "debug.h"

static const char pszLogFile[] = "yclock.log";
static FILE *fp = NULL;

void
syslog(int pri, ...)	/* priは今のところ無視 */
{
	va_list args;
	char *fmt;
	char path[MAX_PATH + 1];
	char timstr[26];
	char *p;
	time_t tim;

	if (fp == NULL) {	/* 初回 */
		getFullPath(pszLogFile, path, sizeof(path));
		fopen_s(&fp, path, "a");
	}

	if (NULL != fp) {
		time(&tim);
		ctime_s(timstr, sizeof(timstr), &tim);
		p = timstr;
		*(p + strlen(p) - 1) = '\0';
		fprintf(fp, "[%s] ", p);

		va_start(args, pri);
		fmt = va_arg(args, char *);
		vfprintf(fp, fmt, args);
		va_end(args);

		fprintf(fp, "\n");
	}
	fflush(fp);
	return;
}

void
syslog_close()
{
	if (fp != NULL) {
		fclose(fp);
	}
}

void
syslog_dump(int pri, void *addr, int size)
{
	/* 受信生データ出力 */
	int i = 0;
	char *p;
	char msg[100000];
	wsprintf(msg, "Dump Address: %x", addr);
	for (p = (char *)addr; i < size; i++, p++) {
		if (i % 8 == 0) {
			wsprintf(msg + strlen(msg), "\n\t+%04X:", i);
		}
		wsprintf(msg + strlen(msg), " %02X", (unsigned char)*p);
	}
	syslog(pri, msg);
}

#endif /* _DEBUG */
