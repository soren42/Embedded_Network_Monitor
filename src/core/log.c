#include "log.h"
#include <stdio.h>
#include <stdarg.h>
#include <time.h>

static int g_log_level = LOG_LEVEL_INFO;

static const char *level_str(int level)
{
    switch (level) {
    case LOG_LEVEL_DEBUG: return "DEBUG";
    case LOG_LEVEL_INFO:  return "INFO ";
    case LOG_LEVEL_WARN:  return "WARN ";
    case LOG_LEVEL_ERROR: return "ERROR";
    default:              return "?????";
    }
}

void log_init(int level)
{
    g_log_level = level;
}

void log_msg(int level, const char *fmt, ...)
{
    if (level < g_log_level) return;

    time_t now = time(NULL);
    struct tm tm_info;
    localtime_r(&now, &tm_info);

    fprintf(stderr, "[%02d:%02d:%02d] [%s] ",
            tm_info.tm_hour, tm_info.tm_min, tm_info.tm_sec,
            level_str(level));

    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);

    fputc('\n', stderr);
}
