#ifndef NTP_TIMESTAMP_H
#define NTP_TIMESTAMP_H

#include <windows.h>

/*
 * NTP Timestamp
 */
class Timestamp
{
private:
	unsigned long	integer;	/* 整数部 */
	unsigned long	fraction;	/* 小数部 */
	int		overflow;	/* オーバーフロー */

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

	/* 演算 */
	void add(Timestamp*);
	void sub(Timestamp*);
	void abs();
	void half();
};

#endif /* NTP_TIMESTAMP_H */
