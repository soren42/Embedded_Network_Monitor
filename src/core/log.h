#ifndef LOG_H
#define LOG_H

#define LOG_LEVEL_DEBUG  0
#define LOG_LEVEL_INFO   1
#define LOG_LEVEL_WARN   2
#define LOG_LEVEL_ERROR  3

void log_init(int level);
void log_msg(int level, const char *fmt, ...);

#define LOG_DEBUG(...)  log_msg(LOG_LEVEL_DEBUG, __VA_ARGS__)
#define LOG_INFO(...)   log_msg(LOG_LEVEL_INFO,  __VA_ARGS__)
#define LOG_WARN(...)   log_msg(LOG_LEVEL_WARN,  __VA_ARGS__)
#define LOG_ERROR(...)  log_msg(LOG_LEVEL_ERROR, __VA_ARGS__)

#endif /* LOG_H */
