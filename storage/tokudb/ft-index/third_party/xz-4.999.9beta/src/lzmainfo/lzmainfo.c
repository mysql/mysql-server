/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
///////////////////////////////////////////////////////////////////////////////
//
/// \file       lzmainfo.c
/// \brief      lzmainfo tool for compatibility with LZMA Utils
//
//  Author:     Lasse Collin
//
//  This file has been put into the public domain.
//  You can do whatever you want with this file.
//
///////////////////////////////////////////////////////////////////////////////

#include "sysdefs.h"
#include <stdio.h>
#include <errno.h>

#ifdef ENABLE_NLS
#	include <libintl.h>
#	define _(msgid) gettext(msgid)
#else
#	define _(msgid) msgid
#endif

#include "lzma.h"
#include "getopt.h"


/// Name of the program from argv[0]
static const char *argv0;


/// Close stdout unless we are already going to exit with EXIT_FAILURE.
/// If closing stdout fails, set exit status to EXIT_FAILURE and print
/// an error message to stderr. We don't care about closing stderr,
/// because we don't print anything to stderr unless we are going to
/// use EXIT_FAILURE anyway.
static void lzma_attribute((noreturn))
my_exit(int status)
{
	if (status != EXIT_FAILURE) {
		const int ferror_err = ferror(stdout);
		const int fclose_err = fclose(stdout);

		if (ferror_err || fclose_err) {
			// If it was fclose() that failed, we have the reason
			// in errno. If only ferror() indicated an error,
			// we have no idea what the reason was.
			fprintf(stderr, "%s: %s: %s\n", argv0,
					_("Writing to standard output "
						"failed"),
					fclose_err ? strerror(errno)
						: _("Unknown error"));
			status = EXIT_FAILURE;
		}
	}

	exit(status);
}


static void lzma_attribute((noreturn))
help(void)
{
	printf(
_("Usage: %s [--help] [--version] [FILE]...\n"
"Show information stored in the .lzma file header"), argv0);

	printf(_(
"\nWith no FILE, or when FILE is -, read standard input.\n"));
	printf("\n");

	printf(_("Report bugs to <%s> (in English or Finnish).\n"),
			PACKAGE_BUGREPORT);
	printf(_("%s home page: <%s>\n"), PACKAGE_NAME, PACKAGE_HOMEPAGE);

	my_exit(EXIT_SUCCESS);
}


static void lzma_attribute((noreturn))
version(void)
{
	puts("lzmainfo (" PACKAGE_NAME ") " PACKAGE_VERSION);
	my_exit(EXIT_SUCCESS);
}


/// Parse command line options.
static void
parse_args(int argc, char **argv)
{
	enum {
		OPT_HELP,
		OPT_VERSION,
	};

	static const struct option long_opts[] = {
		{ "help",    no_argument, NULL, OPT_HELP },
		{ "version", no_argument, NULL, OPT_VERSION },
		{ NULL,      0,           NULL, 0 }
	};

	int c;
	while ((c = getopt_long(argc, argv, "", long_opts, NULL)) != -1) {
		switch (c) {
		case OPT_HELP:
			help();

		case OPT_VERSION:
			version();

		default:
			exit(EXIT_FAILURE);
		}
	}

	return;
}


/// Primitive base-2 logarithm for integers
static uint32_t
my_log2(uint32_t n)
{
	uint32_t e;
	for (e = 0; n > 1; ++e, n /= 2) ;
	return e;
}


/// Parse the .lzma header and display information about it.
static bool
lzmainfo(const char *name, FILE *f)
{
	uint8_t buf[13];
	const size_t size = fread(buf, 1, sizeof(buf), f);
	if (size != 13) {
		fprintf(stderr, "%s: %s: %s\n", argv0, name,
				ferror(f) ? strerror(errno)
				: _("File is too small to be a .lzma file"));
		return true;
	}

	lzma_filter filter = { .id = LZMA_FILTER_LZMA1 };

	// Parse the first five bytes.
	switch (lzma_properties_decode(&filter, NULL, buf, 5)) {
	case LZMA_OK:
		break;

	case LZMA_OPTIONS_ERROR:
		fprintf(stderr, "%s: %s: %s\n", argv0, name,
				_("Not a .lzma file"));
		return true;

	case LZMA_MEM_ERROR:
		fprintf(stderr, "%s: %s\n", argv0, strerror(ENOMEM));
		exit(EXIT_FAILURE);

	default:
		fprintf(stderr, "%s: %s\n", argv0, _("Internal error (bug)"));
		exit(EXIT_FAILURE);
	}

	// Uncompressed size
	uint64_t uncompressed_size = 0;
	for (size_t i = 0; i < 8; ++i)
		uncompressed_size |= (uint64_t)(buf[5 + i]) << (i * 8);

	// Display the results. We don't want to translate these and also
	// will use MB instead of MiB, because someone could be parsing
	// this output and we don't want to break that when people move
	// from LZMA Utils to XZ Utils.
	if (f != stdin)
		printf("%s\n", name);

	printf("Uncompressed size:             ");
	if (uncompressed_size == UINT64_MAX)
		printf("Unknown");
	else
		printf("%" PRIu64 " MB (%" PRIu64 " bytes)",
				(uncompressed_size + 512 * 1024)
					/ (1024 * 1024),
				uncompressed_size);

	lzma_options_lzma *opt = filter.options;

	printf("\nDictionary size:               "
			"%u MB (2^%u bytes)\n"
			"Literal context bits (lc):     %" PRIu32 "\n"
			"Literal pos bits (lp):         %" PRIu32 "\n"
			"Number of pos bits (pb):       %" PRIu32 "\n",
			(opt->dict_size + 512 * 1024) / (1024 * 1024),
			my_log2(opt->dict_size), opt->lc, opt->lp, opt->pb);

	free(opt);

	return false;
}


extern int
main(int argc, char **argv)
{
	int ret = EXIT_SUCCESS;
	argv0 = argv[0];

	parse_args(argc, argv);

	// We print empty lines around the output only when reading from
	// files specified on the command line. This is due to how
	// LZMA Utils did it.
	if (optind == argc) {
		lzmainfo("(stdin)", stdin);
	} else {
		printf("\n");

		do {
			if (strcmp(argv[optind], "-") == 0) {
				if (lzmainfo("(stdin)", stdin))
					ret = EXIT_FAILURE;
			} else {
				FILE *f = fopen(argv[optind], "r");
				if (f == NULL) {
					ret = EXIT_FAILURE;
					fprintf(stderr, "%s: %s: %s\n",
							argv0, argv[optind],
							strerror(errno));
					continue;
				}

				if (lzmainfo(argv[optind], f))
					ret = EXIT_FAILURE;

				printf("\n");
				fclose(f);
			}
		} while (++optind < argc);
	}

	my_exit(ret);
}
