#include <stdio.h>
#include <stdarg.h>
#include "logging.h"
#include "filesystem.h"

#define BUFFER_LENGTH 256*1024
#define BUFFER_THRESHOLD (BUFFER_LENGTH - 256)
static char log_buffer[BUFFER_LENGTH];
static int log_pointer = 0;

void log_save(char *filename) {
    file_save_bin(filename, log_buffer, log_pointer);
}

#ifdef DEBUG
void log_debug(const char *fmt, ...) {
   va_list ap;
   printf("DEBUG: ");
   log_pointer += sprintf(log_buffer + log_pointer, "DEBUG: ");
   va_start(ap, fmt);
   vprintf(fmt, ap);
   log_pointer += vsprintf(log_buffer + log_pointer, fmt, ap);
   log_pointer += sprintf(log_buffer + log_pointer, "\r\n");
   log_pointer = log_pointer > BUFFER_THRESHOLD ? 0 : log_pointer;
   va_end(ap);
   printf("\r\n");
}
#endif

void log_info(const char *fmt, ...) { //can print up to 6 chars very fast (8 char tx fifo buffer minus CR/LF) - assumes buffer is already empty
   va_list ap;
   va_start(ap, fmt);
   vprintf(fmt, ap);
   log_pointer += vsprintf(log_buffer + log_pointer, fmt, ap);
   log_pointer += sprintf(log_buffer + log_pointer, "\r\n");
   log_pointer = log_pointer > BUFFER_THRESHOLD ? 0 : log_pointer;
   va_end(ap);
   printf("\r\n");
}

void log_warn(const char *fmt, ...) {
   va_list ap;
   printf("WARN: ");
   log_pointer += sprintf(log_buffer + log_pointer, "WARN: ");
   va_start(ap, fmt);
   vprintf(fmt, ap);
   log_pointer += vsprintf(log_buffer + log_pointer, fmt, ap);
   log_pointer += sprintf(log_buffer + log_pointer, "\r\n");
   log_pointer = log_pointer > BUFFER_THRESHOLD ? 0 : log_pointer;
   va_end(ap);
   printf("\r\n");
}

void log_error(const char *fmt, ...) {
   va_list ap;
   printf("ERROR: ");
   log_pointer += sprintf(log_buffer + log_pointer, "ERROR: ");
   va_start(ap, fmt);
   vprintf(fmt, ap);
   log_pointer += vsprintf(log_buffer + log_pointer, fmt, ap);
   log_pointer += sprintf(log_buffer + log_pointer, "\r\n");
   log_pointer = log_pointer > BUFFER_THRESHOLD ? 0 : log_pointer;
   va_end(ap);
   printf("\r\n");
}

void log_fatal(const char *fmt, ...) {
   va_list ap;
   printf("FATAL: ");
   log_pointer += sprintf(log_buffer + log_pointer, "FATAL: ");
   va_start(ap, fmt);
   vprintf(fmt, ap);
   log_pointer += vsprintf(log_buffer + log_pointer, fmt, ap);
   log_pointer += sprintf(log_buffer + log_pointer, "\r\n");
   log_pointer = log_pointer > BUFFER_THRESHOLD ? 0 : log_pointer;
   va_end(ap);
   printf("\r\n");
}
