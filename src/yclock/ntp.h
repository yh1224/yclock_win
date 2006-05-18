#ifndef NTP_H
#define NTP_H

/*
 * NTP Timestamp
 */
struct timestamp {
	unsigned long	integer;
	unsigned long	fraction;
};

/* NTP packet */
struct pkt {
	unsigned char		li_vn_mode;
	unsigned char		stratum;
	unsigned char		ppoll;
	char			precision;
	unsigned long		rootdelay;
	unsigned long		rootdispersion;
	unsigned long		refid;
	struct timestamp	reftime;
	struct timestamp	org;
	struct timestamp	rec;
	struct timestamp	xmt;
};

#endif /* NTP_H */
