/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
///////////////////////////////////////////////////////////////////////////////
//
/// \file       main.c
/// \brief      main()
//
//  Author:     Lasse Collin
//
//  This file has been put into the public domain.
//  You can do whatever you want with this file.
//
///////////////////////////////////////////////////////////////////////////////

#include "private.h"
#include <ctype.h>


/// Exit status to use. This can be changed with set_exit_status().
static enum exit_status_type exit_status = E_SUCCESS;

/// True if --no-warn is specified. When this is true, we don't set
/// the exit status to E_WARNING when something worth a warning happens.
static bool no_warn = false;


extern void
set_exit_status(enum exit_status_type new_status)
{
	assert(new_status == E_WARNING || new_status == E_ERROR);

	if (exit_status != E_ERROR)
		exit_status = new_status;

	return;
}


extern void
set_exit_no_warn(void)
{
	no_warn = true;
	return;
}


extern void
my_exit(enum exit_status_type status)
{
	// Close stdout. If something goes wrong, print an error message
	// to stderr.
	{
		const int ferror_err = ferror(stdout);
		const int fclose_err = fclose(stdout);
		if (ferror_err || fclose_err) {
			// If it was fclose() that failed, we have the reason
			// in errno. If only ferror() indicated an error,
			// we have no idea what the reason was.
			message(V_ERROR, "%s: %s", _("Writing to standard "
						"output failed"),
					fclose_err ? strerror(errno)
						: _("Unknown error"));
			status = E_ERROR;
		}
	}

	// Close stderr. If something goes wrong, there's nothing where we
	// could print an error message. Just set the exit status.
	{
		const int ferror_err = ferror(stderr);
		const int fclose_err = fclose(stderr);
		if (fclose_err || ferror_err)
			status = E_ERROR;
	}

	// Suppress the exit status indicating a warning if --no-warn
	// was specified.
	if (status == E_WARNING && no_warn)
		status = E_SUCCESS;

	// If we have got a signal, raise it to kill the program.
	// Otherwise we just call exit().
	signals_exit();
	exit(status);
}


static const char *
read_name(const args_info *args)
{
	// FIXME: Maybe we should have some kind of memory usage limit here
	// like the tool has for the actual compression and uncompression.
	// Giving some huge text file with --files0 makes us to read the
	// whole file in RAM.
	static char *name = NULL;
	static size_t size = 256;

	// Allocate the initial buffer. This is never freed, since after it
	// is no longer needed, the program exits very soon. It is safe to
	// use xmalloc() and xrealloc() in this function, because while
	// executing this function, no files are open for writing, and thus
	// there's no need to cleanup anything before exiting.
	if (name == NULL)
		name = xmalloc(size);

	// Write position in name
	size_t pos = 0;

	// Read one character at a time into name.
	while (!user_abort) {
		const int c = fgetc(args->files_file);

		if (ferror(args->files_file)) {
			// Take care of EINTR since we have established
			// the signal handlers already.
			if (errno == EINTR)
				continue;

			message_error(_("%s: Error reading filenames: %s"),
					args->files_name, strerror(errno));
			return NULL;
		}

		if (feof(args->files_file)) {
			if (pos != 0)
				message_error(_("%s: Unexpected end of input "
						"when reading filenames"),
						args->files_name);

			return NULL;
		}

		if (c == args->files_delim) {
			// We allow consecutive newline (--files) or '\0'
			// characters (--files0), and ignore such empty
			// filenames.
			if (pos == 0)
				continue;

			// A non-empty name was read. Terminate it with '\0'
			// and return it.
			name[pos] = '\0';
			return name;
		}

		if (c == '\0') {
			// A null character was found when using --files,
			// which expects plain text input separated with
			// newlines.
			message_error(_("%s: Null character found when "
					"reading filenames; maybe you meant "
					"to use `--files0' instead "
					"of `--files'?"), args->files_name);
			return NULL;
		}

		name[pos++] = c;

		// Allocate more memory if needed. There must always be space
		// at least for one character to allow terminating the string
		// with '\0'.
		if (pos == size) {
			size *= 2;
			name = xrealloc(name, size);
		}
	}

	return NULL;
}


int
main(int argc, char **argv)
{
	// Initialize the file I/O as the very first step. This makes sure
	// that stdin, stdout, and stderr are something valid.
	io_init();

#ifdef DOSLIKE
	// Adjust argv[0] to make it look nicer in messages, and also to
	// help the code in args.c.
	{
		// Strip the leading path.
		char *p = argv[0] + strlen(argv[0]);
		while (argv[0] < p && p[-1] != '/' && p[-1] != '\\')
			--p;

		argv[0] = p;

		// Strip the .exe suffix.
		p = strrchr(p, '.');
		if (p != NULL)
			*p = '\0';

		// Make it lowercase.
		for (p = argv[0]; *p != '\0'; ++p)
			if (*p >= 'A' && *p <= 'Z')
				*p = *p - 'A' + 'a';
	}
#endif

	// Set up the locale.
	setlocale(LC_ALL, "");

#ifdef ENABLE_NLS
	// Set up the message translations too.
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
#endif

	// Set the program invocation name used in various messages, and
	// do other message handling related initializations.
	message_init(argv[0]);

	// Set hardware-dependent default values. These can be overriden
	// on the command line, thus this must be done before parse_args().
	hardware_init();

	// Parse the command line arguments and get an array of filenames.
	// This doesn't return if something is wrong with the command line
	// arguments. If there are no arguments, one filename ("-") is still
	// returned to indicate stdin.
	args_info args;
	args_parse(&args, argc, argv);

	// Tell the message handling code how many input files there are if
	// we know it. This way the progress indicator can show it.
	if (args.files_name != NULL)
		message_set_files(0);
	else
		message_set_files(args.arg_count);

	// Refuse to write compressed data to standard output if it is
	// a terminal and --force wasn't used.
	if (opt_mode == MODE_COMPRESS && !opt_force) {
		if (opt_stdout || (args.arg_count == 1
				&& strcmp(args.arg_names[0], "-") == 0)) {
			if (is_tty_stdout()) {
				message_try_help();
				my_exit(E_ERROR);
			}
		}
	}

	if (opt_mode == MODE_LIST) {
		message_fatal("--list is not implemented yet.");
	}

	// Hook the signal handlers. We don't need these before we start
	// the actual action, so this is done after parsing the command
	// line arguments.
	signals_init();

	// Process the files given on the command line. Note that if no names
	// were given, parse_args() gave us a fake "-" filename.
	for (size_t i = 0; i < args.arg_count && !user_abort; ++i) {
		if (strcmp("-", args.arg_names[i]) == 0) {
			// Processing from stdin to stdout. Unless --force
			// was used, check that we aren't writing compressed
			// data to a terminal or reading it from terminal.
			if (!opt_force) {
				if (opt_mode == MODE_COMPRESS) {
					if (is_tty_stdout())
						continue;
				} else if (is_tty_stdin()) {
					continue;
				}
			}

			// It doesn't make sense to compress data from stdin
			// if we are supposed to read filenames from stdin
			// too (enabled with --files or --files0).
			if (args.files_name == stdin_filename) {
				message_error(_("Cannot read data from "
						"standard input when "
						"reading filenames "
						"from standard input"));
				continue;
			}

			// Replace the "-" with a special pointer, which is
			// recognized by coder_run() and other things.
			// This way error messages get a proper filename
			// string and the code still knows that it is
			// handling the special case of stdin.
			args.arg_names[i] = (char *)stdin_filename;
		}

		// Do the actual compression or uncompression.
		coder_run(args.arg_names[i]);
	}

	// If --files or --files0 was used, process the filenames from the
	// given file or stdin. Note that here we don't consider "-" to
	// indicate stdin like we do with the command line arguments.
	if (args.files_name != NULL) {
		// read_name() checks for user_abort so we don't need to
		// check it as loop termination condition.
		while (true) {
			const char *name = read_name(&args);
			if (name == NULL)
				break;

			// read_name() doesn't return empty names.
			assert(name[0] != '\0');
			coder_run(name);
		}

		if (args.files_name != stdin_filename)
			(void)fclose(args.files_file);
	}

	my_exit(exit_status);
}
