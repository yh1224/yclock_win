#ifndef NTP_TIMESTAMP_H
#define NTP_TIMESTAMP_H

#include <windows.h>

/*
 * NTP Timestamp
 */
class Timestamp
{
private:
	unsigned long	integer;	/* ������ */
	unsigned long	fraction;	/* ������ */
	int		overflow;	/* �I�[�o�[�t���[ */

public:
	Timestamp();
	Timestamp(SYSTEMTIME*);
	Timestamp(Timestamp*);
	Timestamp(char *);
	Timestamp(unsigned long, unsigned long);
	~Timestamp();

	/* getter */
	unsigned long getSec();
	unsigned long getFrac();
	unsigned long getUSec();
	int getOverflow();
	void getPacketData(char *);
	SYSTEMTIME* getSystemTime(SYSTEMTIME *);

	/* setter */
	void set(unsigned long, unsigned long);

	/* ���Z */
	void add(Timestamp*);
	void sub(Timestamp*);
	void abs();
	void half();
};

#endif /* NTP_TIMESTAMP_H */
