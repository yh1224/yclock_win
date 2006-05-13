#ifdef _DEBUG
#include <assert.h>

#define LOG_ERR   1
#define LOG_WARN  2
#define LOG_INFO  3
#define LOG_DEBUG 4

void syslog(int, ...);
void syslog_close();
void syslog_dump(int, void *, int);

#define SYSLOG(x) syslog x
#define SYSLOG_CLOSE() syslog_close()
#define SYSLOG_DUMP(x) syslog_dump x
#define ASSERT(x) assert(x)

#else

#define SYSLOG(x)
#define SYSLOG_CLOSE()
#define SYSLOG_DUMP(x) 
#define ASSERT(x)

#endif
