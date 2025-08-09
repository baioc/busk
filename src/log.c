#include "log.h"

#include <assert.h>
#include <stdarg.h>
#include <stddef.h> // NULL
#include <stdio.h>
#include <stdlib.h> // exit
#include <time.h>


#ifndef LOG_TIME_SUBSEC
#define LOG_TIME_SUBSEC 3
#elif LOG_TIME_SUBSEC > 9
#error "LOG_TIME_SUBSEC can't be greater than 9"
#endif

#ifndef LOG_INDENT_SIZE
#define LOG_INDENT_SIZE 4
#elif LOG_INDENT_SIZE < 1
#error "LOG_INDENT_SIZE must be strictly positive"
#endif

#ifndef LOG_BUFFER_SIZE
#define LOG_BUFFER_SIZE 30000
#elif LOG_BUFFER_SIZE < 81
#error "LOG_BUFFER_SIZE must be at least 81"
#endif

#define STRINGIFY_(X) #X
#define STRINGIFY(X) STRINGIFY_(X)


_Thread_local struct LogConfig logger = {0};


static void log_va(
	const char *logname, enum LogLevel level,
	const char *srcfile, int srcline,
	const char *format, va_list vargs
) {
	struct LogConfig log = logger;
	if (level < log.level) return;
	if (!log.file) log.file = stderr;

	time_t now = time(NULL);
#if LOG_TIME_SUBSEC > 0
	struct timespec instant = {0};
	clock_gettime(CLOCK_REALTIME, &instant);
#endif
	struct tm tm = {0};
	gmtime_r(&now, &tm);

	const char *level_str = NULL;
	switch (level) {
		case LOG_LEVEL_TRACE: level_str = "TRACE"; break;
		case LOG_LEVEL_DEBUG: level_str = "DEBUG"; break;
		case LOG_LEVEL_INFO: level_str = "INFO"; break;
		case LOG_LEVEL_WARN: level_str = "WARN"; break;
		case LOG_LEVEL_ERROR: level_str = "ERROR"; break;
		case LOG_LEVEL_FATAL: level_str = "FATAL"; break;
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

	static _Thread_local char buffer[LOG_BUFFER_SIZE];
	const int msglen = vsnprintf(buffer, sizeof(buffer), format, vargs);
	if (msglen < 0 || (size_t)msglen >= sizeof(buffer)) {
		fprintf(log.file, "[ERRFMT]");
	} else {
		// filter possibly dangerous chars before printing
		// NOTE: this means that we can only log ASCII
		for (int i = 0; i < msglen; ++i) {
			const char c = buffer[i];
			if ((c >= ' ' && c <= '~') || c == '\t') continue;
			buffer[i] = '?';
		}
		assert(buffer[msglen] == '\0');
		fprintf(log.file, "%s", buffer);
	}

	fprintf(log.file, "\n");
	fflush(log.file);

	if (level >= LOG_LEVEL_FATAL) exit(level);
}

void log_impl(
	const char *logname, enum LogLevel level,
	const char *srcfile, int srcline,
	const char *format, ...
) {
	const struct LogConfig log = logger;
	va_list va;
	va_start(va, format);
	if (!log.function) {
		log_va(logname, level, srcfile, srcline, format, va);
	} else {
		log.function(log.arg, logname, level, srcfile, srcline, format, va);
	}
	va_end(va);
}
