#ifndef INCLUDE_LOG_H
#define INCLUDE_LOG_H

#include <stdarg.h>
#include <stdio.h> // FILE

enum LogLevel {
	LOG_LEVEL_TRACE = -2,
	LOG_LEVEL_DEBUG = -1,
	LOG_LEVEL_INFO = 0,
	LOG_LEVEL_WARN = 1,
	LOG_LEVEL_ERROR = 2,
	LOG_LEVEL_FATAL = 3, // Causes program to exit after logging.
};

typedef void (*LogFunction)(
	void *arg,
	const char *logname, enum LogLevel level,
	const char *srcfile, int srcline,
	const char *format, va_list varargs
);

// Logging configuration. Initialize with `{0}` for sane defaults.
struct LogConfig {
	enum LogLevel level; // Minimum level where logs will actually be written.
	FILE *file; // Log file ready for appending. NULL means write to stderr.
	unsigned indent; // nice for recursion
	LogFunction function; // Set to override default logging behaviour.
	void *arg; // Always forwarded to custom logging function.
};

// Per-thread logging configuration.
extern _Thread_local struct LogConfig logger;

#ifdef __GNUC__
__attribute__((format(printf, 5, 6)))
#endif
// Logging procedure. Use macros instead of calling this directly.
extern void log_impl(
	const char *logname, enum LogLevel level,
	const char *srcfile, int srcline,
	const char *format, ...
);

#ifndef LOG_NAME
#include <stddef.h>
#define LOG_NAME NULL
#endif

#define LOG_TRACEF(format, ...) \
	log_impl( \
		LOG_NAME, LOG_LEVEL_TRACE, \
		__FILE__, __LINE__, \
		(format), __VA_ARGS__ \
	)

#define LOG_TRACE(msg) LOG_TRACEF("%s", (msg))

#define LOG_DEBUGF(format, ...) \
	log_impl( \
		LOG_NAME, LOG_LEVEL_DEBUG, \
		__FILE__, __LINE__, \
		(format), __VA_ARGS__ \
	)

#define LOG_DEBUG(msg) LOG_DEBUGF("%s", (msg))

#define LOG_INFOF(format, ...) \
	log_impl( \
		LOG_NAME, LOG_LEVEL_INFO, \
		__FILE__, __LINE__, \
		(format), __VA_ARGS__ \
	)

#define LOG_INFO(msg) LOG_INFOF("%s", (msg))

#define LOG_WARNF(format, ...) \
	log_impl( \
		LOG_NAME, LOG_LEVEL_WARN, \
		__FILE__, __LINE__, \
		(format), __VA_ARGS__ \
	)

#define LOG_WARN(msg) LOG_WARNF("%s", (msg))

#define LOG_ERRORF(format, ...) \
	log_impl( \
		LOG_NAME, LOG_LEVEL_ERROR, \
		__FILE__, __LINE__, \
		(format), __VA_ARGS__ \
	)

#define LOG_ERROR(msg) LOG_ERRORF("%s", (msg))

#define LOG_FATALF(format, ...) \
	log_impl( \
		LOG_NAME, LOG_LEVEL_FATAL, \
		__FILE__, __LINE__, \
		(format), __VA_ARGS__ \
	)

#define LOG_FATAL(msg) LOG_FATALF("%s", (msg))

#endif // INCLUDE_LOG_H
