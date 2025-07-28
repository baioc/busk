#include "log.h"

#include <stdarg.h> // va_list
#include <stdio.h> // stderr
#include <stdlib.h> // exit
#include <time.h>


#if !defined(LOG_TIME_SUBSEC)
#	define LOG_TIME_SUBSEC 3
#elif LOG_TIME_SUBSEC > 9
#	error "LOG_TIME_SUBSEC can't be greater than 9"
#endif

#if !defined(LOG_INDENT_SIZE)
#	define LOG_INDENT_SIZE 4
#elif LOG_INDENT_SIZE < 1
#	error "LOG_INDENT_SIZE must be strictly positive"
#endif

#define STRINGIFY_(X) #X
#define STRINGIFY(X) STRINGIFY_(X)


_Thread_local LogConfig logger = {0};


static void log_va(
	const char *logname, LogLevel level,
	const char *srcfile, int srcline,
	const char *format, va_list vargs
) {
	LogConfig log = logger;
	if (level < log.level) return;
	if (!log.file) log.file = stderr;

	time_t now = time(NULL);
#if LOG_TIME_SUBSEC > 0
	struct timespec instant = {0};
	clock_gettime(CLOCK_REALTIME, &instant);
#endif
	struct tm tm = {0};
	gmtime_r(&now, &tm);

	const char *level_str;
	switch (level) {
		case LOG_LEVEL_TRACE:
			level_str = "TRACE";
			break;
		case LOG_LEVEL_DEBUG:
			level_str = "DEBUG";
			break;
		case LOG_LEVEL_INFO:
			level_str = "INFO";
			break;
		case LOG_LEVEL_WARN:
			level_str = "WARN";
			break;
		case LOG_LEVEL_ERROR:
			level_str = "ERROR";
			break;
		case LOG_LEVEL_FATAL:
			level_str = "FATAL";
			break;
	}

	// LOGGER_NAME [TIMESTAMP] LOG_LEVEL (SRC_FILE:SRC_LINE) - FORMATTED_MESSAGE
	// ^ both loggername and source location are optional

	if (logname) fprintf(log.file, "%s ", logname);

	fprintf(
		log.file, "[%04d-%02d-%02dT%02d:%02d:%02d",
		tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
		tm.tm_hour, tm.tm_min, tm.tm_sec
	);
#if LOG_TIME_SUBSEC > 0
	time_t nanos = instant.tv_nsec;
	for (int i = 0; i < 9 - LOG_TIME_SUBSEC; ++i) nanos /= 10;
	fprintf(log.file, ".%0" STRINGIFY(LOG_TIME_SUBSEC) "dZ]", (int)nanos);
#else
	fprintf(log.file, "Z]");
#endif

	fprintf(log.file, " %-5s ", level_str);

	if (srcfile) {
		fprintf(log.file, "(%s:%d) - ", srcfile, srcline);
	} else {
		fprintf(log.file, "- ");
	}

	for (unsigned long i = 0; i < log.indent * LOG_INDENT_SIZE; ++i) {
		fprintf(log.file, " ");
	}

	vfprintf(log.file, format, vargs);

	fprintf(log.file, "\n");
	fflush(log.file);

	if (level >= LOG_LEVEL_FATAL) exit(level);
}

void log_impl(
	const char *logname, LogLevel level,
	const char *srcfile, int srcline,
	const char *format, ...
) {
	va_list va;
	va_start(va, format);
	log_va(logname, level, srcfile, srcline, format, va);
	va_end(va);
}
