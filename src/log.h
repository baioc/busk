#ifndef INCLUDE_LOG_H
#define INCLUDE_LOG_H

#include <stddef.h> // NULL
#include <stdio.h> // FILE

typedef enum LogLevel {
	LOG_LEVEL_TRACE = -2,
	LOG_LEVEL_DEBUG = -1,
	LOG_LEVEL_INFO = 0,
	LOG_LEVEL_WARN = 1,
	LOG_LEVEL_ERROR = 2,
	LOG_LEVEL_FATAL = 3,
} LogLevel;

typedef struct LogConfig {
	LogLevel level;
	FILE *file;
	unsigned indent;
} LogConfig;

// Per-thread logging configuration.
extern _Thread_local LogConfig logger;

#ifdef __GNUC__
__attribute__((format(printf, 5, 6)))
#endif
// Logging procedure. Use macros instead of calling this directly.
extern void log_impl(
	const char *logname, LogLevel level,
	const char *srcfile, int srcline,
	const char *format, ...
);

#ifndef LOG_NAME
#	define LOG_NAME NULL
#endif

#define LOG_TRACE(format, ...) \
	log_impl( \
		LOG_NAME, LOG_LEVEL_TRACE, \
		__FILE__, __LINE__, \
		format __VA_OPT__(,) __VA_ARGS__ \
	)

#define LOG_DEBUG(format, ...) \
	log_impl( \
		LOG_NAME, LOG_LEVEL_DEBUG, \
		__FILE__, __LINE__, \
		format __VA_OPT__(,) __VA_ARGS__ \
	)

#define LOG_INFO(format, ...) \
	log_impl( \
		LOG_NAME, LOG_LEVEL_INFO, \
		__FILE__, __LINE__, \
		format __VA_OPT__(,) __VA_ARGS__ \
	)

#define LOG_WARN(format, ...) \
	log_impl( \
		LOG_NAME, LOG_LEVEL_WARN, \
		__FILE__, __LINE__, \
		format __VA_OPT__(,) __VA_ARGS__ \
	)

#define LOG_ERROR(format, ...) \
	log_impl( \
		LOG_NAME, LOG_LEVEL_ERROR, \
		__FILE__, __LINE__, \
		format __VA_OPT__(,) __VA_ARGS__ \
	)

#define LOG_FATAL(format, ...) \
	log_impl( \
		LOG_NAME, LOG_LEVEL_FATAL, \
		__FILE__, __LINE__, \
		format __VA_OPT__(,) __VA_ARGS__ \
	)

#endif // INCLUDE_LOG_H
