/*
 * Copyright (c) 2018 Yubico AB. All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fido.h"

#ifndef FIDO_NO_DIAGNOSTIC

#define XXDLEN	32
#define XXDROW	128
#define LINELEN	256

#ifndef TLS
#define TLS
#endif

static TLS int logging;
static TLS fido_log_handler_t *log_handler;

static void
log_on_stderr(const char *str)
{
	fprintf(stderr, "%s", str);
}

void
fido_log_init(void)
{
	logging = 1;
	log_handler = log_on_stderr;
}

void
fido_log_debug(const char *fmt, ...)
{
	char line[LINELEN];
	va_list ap;
	int r;

	if (!logging || log_handler == NULL)
		return;

	va_start(ap, fmt);
	r = vsnprintf(line, sizeof(line) - 1, fmt, ap);
	va_end(ap);
	if (r < 0 || (size_t)r >= sizeof(line) - 1)
		return;
	strlcat(line, "\n", sizeof(line));
	log_handler(line);
}

void
fido_log_xxd(const void *buf, size_t count)
{
	const uint8_t *ptr = buf;
	char row[XXDROW];
	char xxd[XXDLEN];

	if (!logging || log_handler == NULL || count == 0)
		return;

	*row = '\0';

	for (size_t i = 0; i < count; i++) {
		*xxd = '\0';
		if (i % 16 == 0)
			snprintf(xxd, sizeof(xxd), "%04zu: %02x", i, *ptr++);
		else
			snprintf(xxd, sizeof(xxd), " %02x", *ptr++);
		strlcat(row, xxd, sizeof(row));
		if (i % 16 == 15 || i == count - 1) {
			fido_log_debug("%s", row);
			*row = '\0';
		}
	}
}

void
fido_set_log_handler(fido_log_handler_t *handler)
{
	if (handler != NULL)
		log_handler = handler;
}

#endif /* !FIDO_NO_DIAGNOSTIC */
