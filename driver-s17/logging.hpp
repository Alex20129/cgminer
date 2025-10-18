#ifndef LOGGING_HPP
#define LOGGING_HPP

extern char log_file[32];
extern unsigned int log_level;

enum
{
    LOG_ERR,
    LOG_WARNING,
    LOG_NOTICE,
    LOG_INFO,
    LOG_DEBUG
};

#endif
