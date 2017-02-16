/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
///////////////////////////////////////////////////////////////////////////////
//
/// \file       util.c
/// \brief      Miscellaneous utility functions
//
//  Author:     Lasse Collin
//
//  This file has been put into the public domain.
//  You can do whatever you want with this file.
//
///////////////////////////////////////////////////////////////////////////////

#include "private.h"


// Thousand separator for format strings is not supported outside POSIX.
// This is used in uint64_to_str() and double_to_str().
#ifdef DOSLIKE
#	define THOUSAND ""
#else
#	define THOUSAND "'"
#endif


extern void *
xrealloc(void *ptr, size_t size)
{
	assert(size > 0);

	ptr = realloc(ptr, size);
	if (ptr == NULL)
		message_fatal("%s", strerror(errno));

	return ptr;
}


extern char *
xstrdup(const char *src)
{
	assert(src != NULL);
	const size_t size = strlen(src) + 1;
	char *dest = xmalloc(size);
	return memcpy(dest, src, size);
}


extern uint64_t
str_to_uint64(const char *name, const char *value, uint64_t min, uint64_t max)
{
	uint64_t result = 0;

	// Skip blanks.
	while (*value == ' ' || *value == '\t')
		++value;

	// Accept special value "max". Supporting "min" doesn't seem useful.
	if (strcmp(value, "max") == 0)
		return max;

	if (*value < '0' || *value > '9')
		message_fatal(_("%s: Value is not a non-negative "
				"decimal integer"), value);

	do {
		// Don't overflow.
		if (result > (UINT64_MAX - 9) / 10)
			goto error;

		result *= 10;
		result += *value - '0';
		++value;
	} while (*value >= '0' && *value <= '9');

	if (*value != '\0') {
		// Look for suffix.
		static const struct {
			const char name[4];
			uint64_t multiplier;
		} suffixes[] = {
			{ "k",   UINT64_C(1000) },
			{ "kB",  UINT64_C(1000) },
			{ "M",   UINT64_C(1000000) },
			{ "MB",  UINT64_C(1000000) },
			{ "G",   UINT64_C(1000000000) },
			{ "GB",  UINT64_C(1000000000) },
			{ "Ki",  UINT64_C(1024) },
			{ "KiB", UINT64_C(1024) },
			{ "Mi",  UINT64_C(1048576) },
			{ "MiB", UINT64_C(1048576) },
			{ "Gi",  UINT64_C(1073741824) },
			{ "GiB", UINT64_C(1073741824) }
		};

		uint64_t multiplier = 0;
		for (size_t i = 0; i < ARRAY_SIZE(suffixes); ++i) {
			if (strcmp(value, suffixes[i].name) == 0) {
				multiplier = suffixes[i].multiplier;
				break;
			}
		}

		if (multiplier == 0) {
			message(V_ERROR, _("%s: Invalid multiplier suffix. "
					"Valid suffixes:"), value);
			message_fatal("`k' (10^3), `M' (10^6), `G' (10^9) "
					"`Ki' (2^10), `Mi' (2^20), "
					"`Gi' (2^30)");
		}

		// Don't overflow here either.
		if (result > UINT64_MAX / multiplier)
			goto error;

		result *= multiplier;
	}

	if (result < min || result > max)
		goto error;

	return result;

error:
	message_fatal(_("Value of the option `%s' must be in the range "
				"[%" PRIu64 ", %" PRIu64 "]"),
				name, min, max);
}


extern const char *
uint64_to_str(uint64_t value, uint32_t slot)
{
	// 2^64 with thousand separators is 26 bytes plus trailing '\0'.
	static char bufs[4][32];

	assert(slot < ARRAY_SIZE(bufs));

	snprintf(bufs[slot], sizeof(bufs[slot]), "%" THOUSAND PRIu64, value);
	return bufs[slot];
}


extern const char *
double_to_str(double value)
{
	// 64 bytes is surely enough, since it won't fit in some other
	// fields anyway.
	static char buf[64];

	snprintf(buf, sizeof(buf), "%" THOUSAND ".1f", value);
	return buf;
}


/*
/// \brief      Simple quoting to get rid of ASCII control characters
///
/// This is not so cool and locale-dependent, but should be good enough
/// At least we don't print any control characters on the terminal.
///
extern char *
str_quote(const char *str)
{
	size_t dest_len = 0;
	bool has_ctrl = false;

	while (str[dest_len] != '\0')
		if (*(unsigned char *)(str + dest_len++) < 0x20)
			has_ctrl = true;

	char *dest = malloc(dest_len + 1);
	if (dest != NULL) {
		if (has_ctrl) {
			for (size_t i = 0; i < dest_len; ++i)
				if (*(unsigned char *)(str + i) < 0x20)
					dest[i] = '?';
				else
					dest[i] = str[i];

			dest[dest_len] = '\0';

		} else {
			// Usually there are no control characters,
			// so we can optimize.
			memcpy(dest, str, dest_len + 1);
		}
	}

	return dest;
}
*/


extern bool
is_empty_filename(const char *filename)
{
	if (filename[0] == '\0') {
		message_error(_("Empty filename, skipping"));
		return true;
	}

	return false;
}


extern bool
is_tty_stdin(void)
{
	const bool ret = isatty(STDIN_FILENO);

	if (ret)
		message_error(_("Compressed data not read from a terminal "
				"unless `--force' is used."));

	return ret;
}


extern bool
is_tty_stdout(void)
{
	const bool ret = isatty(STDOUT_FILENO);

	if (ret)
		message_error(_("Compressed data not written to a terminal "
				"unless `--force' is used."));

	return ret;
}
