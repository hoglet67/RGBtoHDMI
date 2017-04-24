#include <stdio.h>
#include <stdarg.h>
#include "logging.h"

#ifdef DEBUG
void log_debug(const char *fmt, ...) {
   va_list ap;
   printf("DEBUG: ");
   va_start(ap, fmt);
   vprintf(fmt, ap);
   va_end(ap);
   printf("\r\n");
}
#endif

void log_info(const char *fmt, ...) {
   va_list ap;
   printf("INFO: ");
   va_start(ap, fmt);
   vprintf(fmt, ap);
   va_end(ap);
   printf("\r\n");
}

void log_warn(const char *fmt, ...) {
   va_list ap;
   printf("WARN: ");
   va_start(ap, fmt);
   vprintf(fmt, ap);
   va_end(ap);
   printf("\r\n");
}

void log_error(const char *fmt, ...) {
   va_list ap;
   printf("ERROR: ");
   va_start(ap, fmt);
   vprintf(fmt, ap);
   va_end(ap);
   printf("\r\n");
}

void log_fatal(const char *fmt, ...) {
   va_list ap;
   printf("FATAL: ");
   va_start(ap, fmt);
   vprintf(fmt, ap);
   va_end(ap);
   printf("\r\n");
}
