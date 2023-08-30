/*
 * Public domain
 * err.h compatibility shim
 */

#ifndef _COMPAT_ERR_H
#define _COMPAT_ERR_H

#if !defined(HAVE_ERR_H)

#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#if defined(_MSC_VER)
__declspec(noreturn)
#else
__attribute__((noreturn))
#endif
static inline void
err(int eval, const char *fmt, ...)
{
	int sverrno = errno;
	va_list ap;

	va_start(ap, fmt);
	if (fmt != NULL) {
		vfprintf(stderr, fmt, ap);
		fprintf(stderr, ": ");
	}
	va_end(ap);
	fprintf(stderr, "%s\n", strerror(sverrno));
	exit(eval);
}

#if defined(_MSC_VER)
__declspec(noreturn)
#else
__attribute__((noreturn))
#endif
static inline void
errx(int eval, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	if (fmt != NULL)
		vfprintf(stderr, fmt, ap);
	va_end(ap);
	fprintf(stderr, "\n");
	exit(eval);
}

static inline void
warn(const char *fmt, ...)
{
	int sverrno = errno;
	va_list ap;

	va_start(ap, fmt);
	if (fmt != NULL) {
		vfprintf(stderr, fmt, ap);
		fprintf(stderr, ": ");
	}
	va_end(ap);
	fprintf(stderr, "%s\n", strerror(sverrno));
}

static inline void
warnx(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	if (fmt != NULL)
		vfprintf(stderr, fmt, ap);
	va_end(ap);
	fprintf(stderr, "\n");
}

#endif /* !defined(HAVE_ERR_H) */

#endif /* _COMPAT_ERR_H */
