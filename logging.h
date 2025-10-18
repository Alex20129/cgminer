#ifndef __LOGGING_H__
#define __LOGGING_H__

#include "config.h"
#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>

#ifdef HAVE_SYSLOG_H
#include <syslog.h>
#else
enum {
	LOG_ERR,
	LOG_WARNING,
	LOG_NOTICE,
	LOG_INFO,
	LOG_DEBUG,
};
#endif

/* debug flags */
extern bool opt_debug;
extern bool opt_decode;
extern bool opt_log_verbose;
extern bool opt_use_syslog;
extern bool want_per_device_stats;

/* global log_level, messages with lower or equal prio are logged */
extern unsigned char opt_log_level;

#define LOGBUFSIZ 512

void _applog(int prio, const char *str);

#define IN_FMT_FFL " in %s %s():%d"

#define applog(prio, fmt, ...) do { \
	if(opt_debug || prio != LOG_DEBUG) { \
		if(opt_use_syslog || opt_log_verbose || prio <= opt_log_level) { \
			char tmp42[LOGBUFSIZ]; \
			snprintf(tmp42, sizeof(tmp42), fmt, ##__VA_ARGS__); \
			_applog(prio, tmp42); \
		} \
	} \
} while(0)

#define quit(status, fmt, ...) do { \
	if(fmt) { \
		char tmp42[LOGBUFSIZ]; \
		snprintf(tmp42, sizeof(tmp42), fmt, ##__VA_ARGS__); \
		_applog(LOG_ERR, tmp42); \
	} \
	__quit(status, true); \
} while(0)

#define early_quit(status, fmt, ...) do { \
	if(fmt) { \
		char tmp42[LOGBUFSIZ]; \
		snprintf(tmp42, sizeof(tmp42), fmt, ##__VA_ARGS__); \
		_applog(LOG_ERR, tmp42); \
	} \
	__quit(status, false); \
} while(0)

#define quithere(status, fmt, ...) do { \
	if(fmt) { \
		char tmp42[LOGBUFSIZ]; \
		snprintf(tmp42, sizeof(tmp42), fmt IN_FMT_FFL, \
				##__VA_ARGS__, __FILE__, __func__, __LINE__); \
		_applog(LOG_ERR, tmp42); \
	} \
	__quit(status, true); \
} while(0)

#endif /* __LOGGING_H__ */
