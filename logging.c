/*
 * Copyright 2011-2012 Con Kolivas
 * Copyright 2013 Andrew Smith
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 3 of the License, or (at your option)
 * any later version.  See COPYING for more details.
 */

#include <unistd.h>

#include "logging.h"
#include "cgminer.h"

bool opt_use_syslog=false;
bool opt_log_verbose=false;
bool opt_debug=false;

unsigned char opt_log_level=LOG_INFO;

/*
 * log function
 */
void _applog(int prio, const char *str)
{
#ifdef HAVE_SYSLOG_H
    if(opt_use_syslog)
    {
		syslog(LOG_USER | prio, "%s", str);
	}
	else
#endif
	{
		char datetime[64];
        struct timeval tv={0, 0};
		struct tm *tm;
		cgtime(&tv);
        const time_t tmp_time=tv.tv_sec;
        int ms=(int)(tv.tv_usec / 1000);
        tm=localtime(&tmp_time);

		snprintf(datetime, sizeof(datetime), " [%d-%02d-%02d %02d:%02d:%02d.%03d] ",
			tm->tm_year + 1900,
			tm->tm_mon + 1,
			tm->tm_mday,
			tm->tm_hour,
			tm->tm_min,
			tm->tm_sec, ms);

		/* Only output to stderr if it's not going to the screen as well */
//		if(!isatty(fileno((FILE *)stderr)))
//		{
//			fprintf(stderr, "%s%s\n", datetime, str);	/* atomic write to stderr */
//			fflush(stderr);
//		}

		fprintf(stdout, "%s%s\n", datetime, str);	/* atomic write to stderr */
		fflush(stdout);
	}
}
