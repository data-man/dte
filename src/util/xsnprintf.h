#ifndef UTIL_XSNPRINTF_H
#define UTIL_XSNPRINTF_H

#include <stdarg.h>
#include <stddef.h>
#include "macros.h"

PRINTF(3) NONNULL_ARGS
size_t xsnprintf(char *restrict buf, size_t len, const char *restrict fmt, ...);

VPRINTF(3) NONNULL_ARGS
size_t xvsnprintf(char *restrict buf, size_t len, const char *restrict fmt, va_list ap);

#endif
