#include "log.h"

#include <SDL.h>

#include <cstdarg>
#include <cstdio>

namespace Log
{

void debug(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	SDL_LogMessageV(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_DEBUG, fmt, args);
	va_end(args);
}

void info(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	SDL_LogMessageV(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO, fmt, args);
	va_end(args);
}

void warn(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	SDL_LogMessageV(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_WARN, fmt, args);
	va_end(args);
}

void error(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	SDL_LogMessageV(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_ERROR, fmt, args);
	va_end(args);
}

}  // namespace Log
