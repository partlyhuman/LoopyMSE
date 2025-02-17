#include "log.h"

#include <cstdarg>
#include <cstdio>

#define PRINTF(x)          \
	if (level > x) return; \
	va_list args;          \
	va_start(args, fmt);   \
	log_level(x);          \
	vprintf(fmt, args);    \
	printf("\n");          \
	va_end(args);

namespace Log
{

static Level level = WARN;

void set_level(Level l)
{
	level = l;
}

void log_level(Level l)
{
	switch (l)
	{
	case TRACE:
		printf("[TRACE] ");
		break;
	case DEBUG:
		printf("[DEBUG] ");
		break;
	case INFO:
		printf("[INFO] ");
		break;
	case WARN:
		printf("[WARN] ");
		break;
	case ERROR:
		printf("[ERROR] ");
		break;
	default:
		break;
	}
}

void debug(const char *fmt, ...)
{
	PRINTF(DEBUG);
}

void info(const char *fmt, ...)
{
	PRINTF(INFO);
}

void warn(const char *fmt, ...)
{
	PRINTF(WARN);
}

void error(const char *fmt, ...)
{
	PRINTF(ERROR);
}

}  // namespace Log
