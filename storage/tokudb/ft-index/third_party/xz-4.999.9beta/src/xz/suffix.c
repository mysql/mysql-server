/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
///////////////////////////////////////////////////////////////////////////////
//
/// \file       suffix.c
/// \brief      Checks filename suffix and creates the destination filename
//
//  Author:     Lasse Collin
//
//  This file has been put into the public domain.
//  You can do whatever you want with this file.
//
///////////////////////////////////////////////////////////////////////////////

#include "private.h"

// For case-insensitive filename suffix on case-insensitive systems
#ifdef DOSLIKE
#	define strcmp strcasecmp
#endif


static char *custom_suffix = NULL;


struct suffix_pair {
	const char *compressed;
	const char *uncompressed;
};


/// \brief      Checks if src_name has given compressed_suffix
///
/// \param      suffix      Filename suffix to look for
/// \param      src_name    Input filename
/// \param      src_len     strlen(src_name)
///
/// \return     If src_name has the suffix, src_len - strlen(suffix) is
///             returned. It's always a positive integer. Otherwise zero
///             is returned.
static size_t
test_suffix(const char *suffix, const char *src_name, size_t src_len)
{
	const size_t suffix_len = strlen(suffix);

	// The filename must have at least one character in addition to
	// the suffix. src_name may contain path to the filename, so we
	// need to check for directory separator too.
	if (src_len <= suffix_len || src_name[src_len - suffix_len - 1] == '/')
		return 0;

	if (strcmp(suffix, src_name + src_len - suffix_len) == 0)
		return src_len - suffix_len;

	return 0;
}


/// \brief      Removes the filename suffix of the compressed file
///
/// \return     Name of the uncompressed file, or NULL if file has unknown
///             suffix.
static char *
uncompressed_name(const char *src_name, const size_t src_len)
{
	static const struct suffix_pair suffixes[] = {
		{ ".xz",    "" },
		{ ".txz",   ".tar" }, // .txz abbreviation for .txt.gz is rare.
		{ ".lzma",  "" },
		{ ".tlz",   ".tar" },
		// { ".gz",    "" },
		// { ".tgz",   ".tar" },
	};

	const char *new_suffix = "";
	size_t new_len = 0;

	if (opt_format == FORMAT_RAW) {
		// Don't check for known suffixes when --format=raw was used.
		if (custom_suffix == NULL) {
			message_error(_("%s: With --format=raw, "
					"--suffix=.SUF is required unless "
					"writing to stdout"), src_name);
			return NULL;
		}
	} else {
		for (size_t i = 0; i < ARRAY_SIZE(suffixes); ++i) {
			new_len = test_suffix(suffixes[i].compressed,
					src_name, src_len);
			if (new_len != 0) {
				new_suffix = suffixes[i].uncompressed;
				break;
			}
		}
	}

	if (new_len == 0 && custom_suffix != NULL)
		new_len = test_suffix(custom_suffix, src_name, src_len);

	if (new_len == 0) {
		message_warning(_("%s: Filename has an unknown suffix, "
				"skipping"), src_name);
		return NULL;
	}

	const size_t new_suffix_len = strlen(new_suffix);
	char *dest_name = xmalloc(new_len + new_suffix_len + 1);

	memcpy(dest_name, src_name, new_len);
	memcpy(dest_name + new_len, new_suffix, new_suffix_len);
	dest_name[new_len + new_suffix_len] = '\0';

	return dest_name;
}


/// \brief      Appends suffix to src_name
///
/// In contrast to uncompressed_name(), we check only suffixes that are valid
/// for the specified file format.
static char *
compressed_name(const char *src_name, const size_t src_len)
{
	// The order of these must match the order in args.h.
	static const struct suffix_pair all_suffixes[][3] = {
		{
			{ ".xz",    "" },
			{ ".txz",   ".tar" },
			{ NULL, NULL }
		}, {
			{ ".lzma",  "" },
			{ ".tlz",   ".tar" },
			{ NULL,     NULL }
/*
		}, {
			{ ".gz",    "" },
			{ ".tgz",   ".tar" },
			{ NULL,     NULL }
*/
		}, {
			// --format=raw requires specifying the suffix
			// manually or using stdout.
			{ NULL,     NULL }
		}
	};

	// args.c ensures this.
	assert(opt_format != FORMAT_AUTO);

	const size_t format = opt_format - 1;
	const struct suffix_pair *const suffixes = all_suffixes[format];

	for (size_t i = 0; suffixes[i].compressed != NULL; ++i) {
		if (test_suffix(suffixes[i].compressed, src_name, src_len)
				!= 0) {
			message_warning(_("%s: File already has `%s' "
					"suffix, skipping"), src_name,
					suffixes[i].compressed);
			return NULL;
		}
	}

	// TODO: Hmm, maybe it would be better to validate this in args.c,
	// since the suffix handling when decoding is weird now.
	if (opt_format == FORMAT_RAW && custom_suffix == NULL) {
		message_error(_("%s: With --format=raw, "
				"--suffix=.SUF is required unless "
				"writing to stdout"), src_name);
		return NULL;
	}

	const char *suffix = custom_suffix != NULL
			? custom_suffix : suffixes[0].compressed;
	const size_t suffix_len = strlen(suffix);

	char *dest_name = xmalloc(src_len + suffix_len + 1);

	memcpy(dest_name, src_name, src_len);
	memcpy(dest_name + src_len, suffix, suffix_len);
	dest_name[src_len + suffix_len] = '\0';

	return dest_name;
}


extern char *
suffix_get_dest_name(const char *src_name)
{
	assert(src_name != NULL);

	// Length of the name is needed in all cases to locate the end of
	// the string to compare the suffix, so calculate the length here.
	const size_t src_len = strlen(src_name);

	return opt_mode == MODE_COMPRESS
			? compressed_name(src_name, src_len)
			: uncompressed_name(src_name, src_len);
}


extern void
suffix_set(const char *suffix)
{
	// Empty suffix and suffixes having a slash are rejected. Such
	// suffixes would break things later.
	if (suffix[0] == '\0' || strchr(suffix, '/') != NULL)
		message_fatal(_("%s: Invalid filename suffix"), optarg);

	// Replace the old custom_suffix (if any) with the new suffix.
	free(custom_suffix);
	custom_suffix = xstrdup(suffix);
	return;
}
