#ifndef _WSC_LOG_H
#define _WSC_LOG_H

#include <syslog.h>
#include <string.h>

void wsc_log_threshold(int threshold);
void wsc_log_close();

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

#define wsc_log(priority, fmt...) __wsc_log(__FILENAME__, __LINE__, priority, fmt)

#define wsc_log_debug(fmt...)     wsc_log(LOG_DEBUG, fmt)
#define wsc_log_info(fmt...)      wsc_log(LOG_INFO, fmt)
#define wsc_log_err(fmt...)       wsc_log(LOG_ERR, fmt)

void  __wsc_log(const char *filename, int line, int priority, const char *fmt, ...);

#endif
