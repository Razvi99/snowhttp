#include <time.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>

#include "log.h"

static int log_threshold = LOG_DEBUG;
static bool log_initialized;
static const char *ident;

void (*log_write)(int priority, const char *fmt, va_list ap);

static const char *log_ident()
{
    FILE *self;
    static char line[64];
    char *p = NULL;
    char *sbuf;

    if ((self = fopen("/proc/self/status", "r")) != NULL) {
        while (fgets(line, sizeof(line), self)) {
            if (!strncmp(line, "Name:", 5)) {
                strtok_r(line, "\t\n", &sbuf);
                p = strtok_r(NULL, "\t\n", &sbuf);
                break;
            }
        }
        fclose(self);
    }

    return p;
}

static inline void log_write_stdout(int priority, const char *fmt, va_list ap)
{
    time_t now;
    struct tm tm;
    char buf[32];

    now = time(NULL);
    localtime_r(&now, &tm);
    strftime(buf, sizeof(buf), "%Y/%m/%d %H:%M:%S", &tm);

    fprintf(stderr, "%s ", buf);
    vfprintf(stderr, fmt, ap);
}

static inline void log_write_syslog(int priority, const char *fmt, va_list ap)
{
    vsyslog(priority, fmt, ap);
}

static inline void log_init()
{
    if (log_initialized)
        return;

    ident = log_ident();

    if (isatty(STDOUT_FILENO)) {
        log_write = log_write_stdout;
    } else {
        log_write = log_write_syslog;

        openlog(ident, 0, LOG_DAEMON);
    }

    log_initialized = true;
}


void wsc_log_threshold(int threshold)
{
    log_threshold = threshold;
}

void wsc_log_close()
{
    if (!log_initialized)
        return;

    closelog();

    log_initialized = 0;
}

void __wsc_log(const char *filename, int line, int priority, const char *fmt, ...)
{
    static char new_fmt[256];
    va_list ap;

    if (priority > log_threshold)
        return;

    log_init();

    snprintf(new_fmt, sizeof(new_fmt), "(%s:%d) %s", filename, line, fmt);

    va_start(ap, fmt);
    log_write(priority, new_fmt, ap);
    va_end(ap);
}
