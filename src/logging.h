// logging.h

#ifndef LOGGING_H
#define LOGGING_H

#ifdef DEBUG
extern void log_debug(const char *fmt, ...);
#else
static inline void log_debug(const char *fmt, ...) {}
#endif

extern void log_info(const char *fmt, ...);

extern void log_warn(const char *fmt, ...);

extern void log_error(const char *fmt, ...);

extern void log_fatal(const char *fmt, ...);

#endif
