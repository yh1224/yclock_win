#include <time.h>
#include "ntp.h"
#include "debug.h"
#include "ntp_timestamp.h"

/*
 * ���ݎ����ŏ�����
 */
Timestamp::Timestamp() {
	time_t t;
	SYSTEMTIME st;

	time(&t);
	GetSystemTime(&st);

	integer = t + 0x83aa7e80;
	fraction = (unsigned long)((double)st.wMilliseconds / 1000. * 4294967296.);
}

/*
 * Timestamp�𕡐�
 */
Timestamp::Timestamp(Timestamp* ts) {
	integer = ts->integer;
	fraction = ts->fraction;
}

/*
 * integer,fraction���w��
 */
Timestamp::Timestamp(unsigned long i, unsigned long f) {
	integer = i;
	fraction = f;
}

/*
 * �p�P�b�g�f�[�^����ݒ�
 */
Timestamp::Timestamp(char *p) {
	unsigned long *lp = (unsigned long *)p;
	integer = ntohl(*lp);
	fraction = ntohl(*(lp + 1));
}

Timestamp::~Timestamp() {
}

/*
 * �b�����擾
 */
unsigned long
Timestamp::getSec() {
	return integer;
}

/*
 * ���������擾
 */
unsigned long
Timestamp::getFrac() {
	return fraction;
}

/*
 * �}�C�N�������擾
 */
unsigned long
Timestamp::getUSec() {
	return (unsigned long)((double)fraction * 1000000. / 4294967296.);
}

/*
 * �p�P�b�g�f�[�^���擾
 */
void
Timestamp::getPacketData(char *p) {
	unsigned long *lp = (unsigned long *)p;
	*lp = htonl(integer);
	*(lp + 1) = htonl(fraction);
	return;
}

/*
 * SYSTEMTIME�^�Ŏ擾
 */
SYSTEMTIME*
Timestamp::getSystemTime(SYSTEMTIME* st) {
	time_t t;
	struct tm *tm;

	if (integer < 0x80000000) {
		/* FSB == 0: 2036-02-07 06:28:16 UTC �` �m�[�T�|�[�g */
		return NULL;
	}else if (integer >= 0x83aa7e80) {
		/* FSB == 1: 1900-01-01 00:00:00 UTC �` */
		t = integer - 0x83aa7e80;
		if (NULL != (tm = gmtime(&t))) {
			st->wYear = tm->tm_year + 1900;
			st->wMonth = tm->tm_mon + 1;
			st->wDay = tm->tm_mday;
			st->wHour = tm->tm_hour;
			st->wMinute = tm->tm_min;
			st->wSecond = tm->tm_sec;
			st->wMilliseconds = (WORD)((double)(fraction) / 4294967296. * 1000.);
			return st;
		}
	}
	return NULL;
}

/*
 * �����Z
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
		integer = 0xffffffff;
		fraction = 0xffffffff;
	}
	return;
}

/*
 * �����Z
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
		integer = 0;
		fraction = 0;
	}
	return;
}

/*
 * ����
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
