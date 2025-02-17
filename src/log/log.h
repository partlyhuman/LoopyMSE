#pragma once

namespace Log
{

enum Level
{
	VERBOSE,
	TRACE,
	DEBUG,
	INFO,
	WARN,
	ERROR,
};

void set_level(Level level);
void log(Level level, const char *fmt, ...);
void trace(const char *fmt, ...);
void debug(const char *fmt, ...);
void info(const char *fmt, ...);
void warn(const char *fmt, ...);
void error(const char *fmt, ...);

}  // namespace Log