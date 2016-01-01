#include <time.h>
#include "ntp.h"
#include "debug.h"
#include "ntp_timestamp.h"

/*
 * 現在時刻で初期化
 */
Timestamp::Timestamp() {
	time_t t;
	SYSTEMTIME st;

	time(&t);
	GetSystemTime(&st);

	set(t + 0x83aa7e80, (unsigned long)((double)st.wMilliseconds / 1000. * 4294967296.));
}

/*
 * Timestampを複製
 */
Timestamp::Timestamp(Timestamp* ts) {
	set(ts->integer, ts->fraction);
}

/*
 * integer,fractionを指定
 */
Timestamp::Timestamp(unsigned long i, unsigned long f) {
	set(i, f);
}

/*
 * パケットデータから設定
 */
Timestamp::Timestamp(char *p) {
	unsigned long *lp = (unsigned long *)p;
	set(ntohl(*lp), ntohl(*(lp + 1)));
}

Timestamp::~Timestamp() {
}

/*
 * 秒数を取得
 */
unsigned long
Timestamp::getSec() {
	return integer;
}

/*
 * 小数部を取得
 */
unsigned long
Timestamp::getFrac() {
	return fraction;
}

/*
 * マイクロ数を取得
 */
unsigned long
Timestamp::getUSec() {
	return (unsigned long)((double)fraction * 1000000. / 4294967296.);
}

/*
 * オーバーフロー情報を取得
 */
int
Timestamp::getOverflow() {
	return overflow;
}

/*
 * パケットデータを取得
 */
void
Timestamp::getPacketData(char *p) {
	unsigned long *lp = (unsigned long *)p;
	*lp = htonl(integer);
	*(lp + 1) = htonl(fraction);
	return;
}

/*
 * SYSTEMTIME型で取得
 */
SYSTEMTIME*
Timestamp::getSystemTime(SYSTEMTIME* st) {
	time_t t;
	struct tm tm;

	if (integer < 0x80000000) {
		/* FSB == 0: 2036-02-07 06:28:16 UTC ～ ノーサポート */
		return NULL;
	}else if (integer >= 0x83aa7e80) {
		/* FSB == 1: 1900-01-01 00:00:00 UTC ～ */
		t = integer - 0x83aa7e80;
		if (0 == gmtime_s(&tm, &t)) {
			st->wYear = tm.tm_year + 1900;
			st->wMonth = tm.tm_mon + 1;
			st->wDay = tm.tm_mday;
			st->wHour = tm.tm_hour;
			st->wMinute = tm.tm_min;
			st->wSecond = tm.tm_sec;
			st->wMilliseconds = (WORD)((double)(fraction) / 4294967296. * 1000.);
			return st;
		}
	}
	return NULL;
}

/*
 * struct tm 型で取得 (localtime)
 */
struct tm *
Timestamp::getLocalTime() {
	static struct tm tm;
	time_t t;

	if (integer < 0x80000000) {
		/* FSB == 0: 2036-02-07 06:28:16 UTC ～ ノーサポート */
		return NULL;
	}else if (integer >= 0x83aa7e80) {
		/* FSB == 1: 1900-01-01 00:00:00 UTC ～ */
		t = integer - 0x83aa7e80;
		localtime_s(&tm, &t);
		return &tm;
	}
	return NULL;
}

/*
 * Timestampを設定
 */
void
Timestamp::set(unsigned long i, unsigned long f) {
	integer = i;
	fraction = f;
	overflow = 0;
}

/*
 * 足し算
 */
void
Timestamp::add(Timestamp* t) {
	integer += t->integer;
	fraction += t->fraction;
	if (fraction < t->fraction) {
		integer++;
	}
	if (integer < t->integer) {
		/* overflow */
		overflow++;
	}
	return;
}

/*
 * 引き算
 */
void
Timestamp::sub(Timestamp* t) {
	unsigned long orig_integer;

	orig_integer = integer;
	integer -= t->integer;
	if (fraction < t->fraction) {
		integer--;
	}
	fraction -= t->fraction;
	if (integer > orig_integer) {
		/* underflow */
		overflow--;
	}
	return;
}

/*
 * 絶対値
 */
void
Timestamp::abs() {
	if (overflow == -1) {
		integer = ~integer;
		fraction = ~fraction + 1;
		if (fraction == 0) {
			integer++;
		}
		overflow = 0;
	}
	return;
}

/*
 * 半分
 */
void
Timestamp::half() {
	fraction >>= 1;
	if ((integer & 0x00000001) == 1) {
		fraction |= 0x80000000;
	}
	integer >>= 1;
	return;
}
