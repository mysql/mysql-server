/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
///////////////////////////////////////////////////////////////////////////////
//
/// \file       message.c
/// \brief      Printing messages to stderr
//
//  Author:     Lasse Collin
//
//  This file has been put into the public domain.
//  You can do whatever you want with this file.
//
///////////////////////////////////////////////////////////////////////////////

#include "private.h"

#ifdef HAVE_SYS_TIME_H
#	include <sys/time.h>
#endif

#include <stdarg.h>


/// Name of the program which is prefixed to the error messages.
static const char *argv0;

/// Number of the current file
static unsigned int files_pos = 0;

/// Total number of input files; zero if unknown.
static unsigned int files_total;

/// Verbosity level
static enum message_verbosity verbosity = V_WARNING;

/// Filename which we will print with the verbose messages
static const char *filename;

/// True once the a filename has been printed to stderr as part of progress
/// message. If automatic progress updating isn't enabled, this becomes true
/// after the first progress message has been printed due to user sending
/// SIGINFO, SIGUSR1, or SIGALRM. Once this variable is true, we will print
/// an empty line before the next filename to make the output more readable.
static bool first_filename_printed = false;

/// This is set to true when we have printed the current filename to stderr
/// as part of a progress message. This variable is useful only if not
/// updating progress automatically: if user sends many SIGINFO, SIGUSR1, or
/// SIGALRM signals, we won't print the name of the same file multiple times.
static bool current_filename_printed = false;

/// True if we should print progress indicator and update it automatically
/// if also verbose >= V_VERBOSE.
static bool progress_automatic;

/// True if message_progress_start() has been called but
/// message_progress_end() hasn't been called yet.
static bool progress_started = false;

/// This is true when a progress message was printed and the cursor is still
/// on the same line with the progress message. In that case, a newline has
/// to be printed before any error messages.
static bool progress_active = false;

/// Pointer to lzma_stream used to do the encoding or decoding.
static lzma_stream *progress_strm;

/// Expected size of the input stream is needed to show completion percentage
/// and estimate remaining time.
static uint64_t expected_in_size;

/// Time when we started processing the file
static uint64_t start_time;


// Use alarm() and SIGALRM when they are supported. This has two minor
// advantages over the alternative of polling gettimeofday():
//  - It is possible for the user to send SIGINFO, SIGUSR1, or SIGALRM to
//    get intermediate progress information even when --verbose wasn't used
//    or stderr is not a terminal.
//  - alarm() + SIGALRM seems to have slightly less overhead than polling
//    gettimeofday().
#ifdef SIGALRM

/// The signal handler for SIGALRM sets this to true. It is set back to false
/// once the progress message has been updated.
static volatile sig_atomic_t progress_needs_updating = false;

/// Signal handler for SIGALRM
static void
progress_signal_handler(int sig lzma_attribute((unused)))
{
	progress_needs_updating = true;
	return;
}

#else

/// This is true when progress message printing is wanted. Using the same
/// variable name as above to avoid some ifdefs.
static bool progress_needs_updating = false;

/// Elapsed time when the next progress message update should be done.
static uint64_t progress_next_update;

#endif


/// Get the current time as microseconds since epoch
static uint64_t
my_time(void)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (uint64_t)(tv.tv_sec) * UINT64_C(1000000) + tv.tv_usec;
}


/// Wrapper for snprintf() to help constructing a string in pieces.
static void lzma_attribute((format(printf, 3, 4)))
my_snprintf(char **pos, size_t *left, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	const int len = vsnprintf(*pos, *left, fmt, ap);
	va_end(ap);

	// If an error occurred, we want the caller to think that the whole
	// buffer was used. This way no more data will be written to the
	// buffer. We don't need better error handling here.
	if (len < 0 || (size_t)(len) >= *left) {
		*left = 0;
	} else {
		*pos += len;
		*left -= len;
	}

	return;
}


extern void
message_init(const char *given_argv0)
{
	// Name of the program
	argv0 = given_argv0;

	// If --verbose is used, we use a progress indicator if and only
	// if stderr is a terminal. If stderr is not a terminal, we print
	// verbose information only after finishing the file. As a special
	// exception, even if --verbose was not used, user can send SIGALRM
	// to make us print progress information once without automatic
	// updating.
	progress_automatic = isatty(STDERR_FILENO);

	// Commented out because COLUMNS is rarely exported to environment.
	// Most users have at least 80 columns anyway, let's think something
	// fancy here if enough people complain.
/*
	if (progress_automatic) {
		// stderr is a terminal. Check the COLUMNS environment
		// variable to see if the terminal is wide enough. If COLUMNS
		// doesn't exist or it has some unparseable value, we assume
		// that the terminal is wide enough.
		const char *columns_str = getenv("COLUMNS");
		if (columns_str != NULL) {
			char *endptr;
			const long columns = strtol(columns_str, &endptr, 10);
			if (*endptr != '\0' || columns < 80)
				progress_automatic = false;
		}
	}
*/

#ifdef SIGALRM
	// At least DJGPP lacks SA_RESTART. It's not essential for us (the
	// rest of the code can handle interrupted system calls), so just
	// define it zero.
#	ifndef SA_RESTART
#		define SA_RESTART 0
#	endif
	// Establish the signal handlers which set a flag to tell us that
	// progress info should be updated. Since these signals don't
	// require any quick action, we set SA_RESTART.
	static const int sigs[] = {
#ifdef SIGALRM
		SIGALRM,
#endif
#ifdef SIGINFO
		SIGINFO,
#endif
#ifdef SIGUSR1
		SIGUSR1,
#endif
	};

	struct sigaction sa;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	sa.sa_handler = &progress_signal_handler;

	for (size_t i = 0; i < ARRAY_SIZE(sigs); ++i)
		if (sigaction(sigs[i], &sa, NULL))
			message_signal_handler();
#endif

	return;
}


extern void
message_verbosity_increase(void)
{
	if (verbosity < V_DEBUG)
		++verbosity;

	return;
}


extern void
message_verbosity_decrease(void)
{
	if (verbosity > V_SILENT)
		--verbosity;

	return;
}


extern void
message_set_files(unsigned int files)
{
	files_total = files;
	return;
}


/// Prints the name of the current file if it hasn't been printed already,
/// except if we are processing exactly one stream from stdin to stdout.
/// I think it looks nicer to not print "(stdin)" when --verbose is used
/// in a pipe and no other files are processed.
static void
print_filename(void)
{
	if (!current_filename_printed
			&& (files_total != 1 || filename != stdin_filename)) {
		signals_block();

		// If a file was already processed, put an empty line
		// before the next filename to improve readability.
		if (first_filename_printed)
			fputc('\n', stderr);

		first_filename_printed = true;
		current_filename_printed = true;

		// If we don't know how many files there will be due
		// to usage of --files or --files0.
		if (files_total == 0)
			fprintf(stderr, "%s (%u)\n", filename,
					files_pos);
		else
			fprintf(stderr, "%s (%u/%u)\n", filename,
					files_pos, files_total);

		signals_unblock();
	}

	return;
}


extern void
message_progress_start(
		lzma_stream *strm, const char *src_name, uint64_t in_size)
{
	// Store the pointer to the lzma_stream used to do the coding.
	// It is needed to find out the position in the stream.
	progress_strm = strm;

	// Store the processing start time of the file and its expected size.
	// If we aren't printing any statistics, then these are unused. But
	// since it is possible that the user sends us a signal to show
	// statistics, we need to have these available anyway.
	start_time = my_time();
	filename = src_name;
	expected_in_size = in_size;

	// Indicate that progress info may need to be printed before
	// printing error messages.
	progress_started = true;

	// Indicate the name of this file hasn't been printed to
	// stderr yet.
	current_filename_printed = false;

	// Start numbering the files starting from one.
	++files_pos;

	// If progress indicator is wanted, print the filename and possibly
	// the file count now.
	if (verbosity >= V_VERBOSE && progress_automatic) {
		// Print the filename to stderr if that is appropriate with
		// the current settings.
		print_filename();

		// Start the timer to display the first progress message
		// after one second. An alternative would be to show the
		// first message almost immediatelly, but delaying by one
		// second looks better to me, since extremely early
		// progress info is pretty much useless.
#ifdef SIGALRM
		// First disable a possibly existing alarm.
		alarm(0);
		progress_needs_updating = false;
		alarm(1);
#else
		progress_needs_updating = true;
		progress_next_update = 1000000;
#endif
	}

	return;
}


/// Make the string indicating completion percentage.
static const char *
progress_percentage(uint64_t in_pos, bool final)
{
	static char buf[sizeof("100.0 %")];

	double percentage;

	if (final) {
		// Use floating point conversion of snprintf() also for
		// 100.0 % instead of fixed string, because the decimal
		// separator isn't a dot in all locales.
		percentage = 100.0;
	} else {
		// If the size of the input file is unknown or the size told us is
		// clearly wrong since we have processed more data than the alleged
		// size of the file, show a static string indicating that we have
		// no idea of the completion percentage.
		if (expected_in_size == 0 || in_pos > expected_in_size)
			return "--- %";

		// Never show 100.0 % before we actually are finished.
		percentage = (double)(in_pos) / (double)(expected_in_size)
				* 99.9;
	}

	snprintf(buf, sizeof(buf), "%.1f %%", percentage);

	return buf;
}


static void
progress_sizes_helper(char **pos, size_t *left, uint64_t value, bool final)
{
	// Allow high precision only for the final message, since it looks
	// stupid for in-progress information.
	if (final) {
		// At maximum of four digits is allowed for exact byte count.
		if (value < 10000) {
			my_snprintf(pos, left, "%s B",
					uint64_to_str(value, 0));
			return;
		}

		// At maximum of five significant digits is allowed for KiB.
		if (value < UINT64_C(10239900)) {
			my_snprintf(pos, left, "%s KiB", double_to_str(
					(double)(value) / 1024.0));
			return;
		}
	}

	// Otherwise we use MiB.
	my_snprintf(pos, left, "%s MiB",
			double_to_str((double)(value) / (1024.0 * 1024.0)));

	return;
}


/// Make the string containing the amount of input processed, amount of
/// output produced, and the compression ratio.
static const char *
progress_sizes(uint64_t compressed_pos, uint64_t uncompressed_pos, bool final)
{
	// This is enough to hold sizes up to about 99 TiB if thousand
	// separator is used, or about 1 PiB without thousand separator.
	// After that the progress indicator will look a bit silly, since
	// the compression ratio no longer fits with three decimal places.
	static char buf[44];

	char *pos = buf;
	size_t left = sizeof(buf);

	// Print the sizes. If this the final message, use more reasonable
	// units than MiB if the file was small.
	progress_sizes_helper(&pos, &left, compressed_pos, final);
	my_snprintf(&pos, &left, " / ");
	progress_sizes_helper(&pos, &left, uncompressed_pos, final);

	// Avoid division by zero. If we cannot calculate the ratio, set
	// it to some nice number greater than 10.0 so that it gets caught
	// in the next if-clause.
	const double ratio = uncompressed_pos > 0
			? (double)(compressed_pos) / (double)(uncompressed_pos)
			: 16.0;

	// If the ratio is very bad, just indicate that it is greater than
	// 9.999. This way the length of the ratio field stays fixed.
	if (ratio > 9.999)
		snprintf(pos, left, " > %.3f", 9.999);
	else
		snprintf(pos, left, " = %.3f", ratio);

	return buf;
}


/// Make the string containing the processing speed of uncompressed data.
static const char *
progress_speed(uint64_t uncompressed_pos, uint64_t elapsed)
{
	// Don't print the speed immediatelly, since the early values look
	// like somewhat random.
	if (elapsed < 3000000)
		return "";

	static const char unit[][8] = {
		"KiB/s",
		"MiB/s",
		"GiB/s",
	};

	size_t unit_index = 0;

	// Calculate the speed as KiB/s.
	double speed = (double)(uncompressed_pos)
			/ ((double)(elapsed) * (1024.0 / 1e6));

	// Adjust the unit of the speed if needed.
	while (speed > 999.0) {
		speed /= 1024.0;
		if (++unit_index == ARRAY_SIZE(unit))
			return ""; // Way too fast ;-)
	}

	// Use decimal point only if the number is small. Examples:
	//  - 0.1 KiB/s
	//  - 9.9 KiB/s
	//  - 99 KiB/s
	//  - 999 KiB/s
	static char buf[sizeof("999 GiB/s")];
	snprintf(buf, sizeof(buf), "%.*f %s",
			speed > 9.9 ? 0 : 1, speed, unit[unit_index]);
	return buf;
}


/// Make a string indicating elapsed or remaining time. The format is either
/// M:SS or H:MM:SS depending on if the time is an hour or more.
static const char *
progress_time(uint64_t useconds)
{
	// 9999 hours = 416 days
	static char buf[sizeof("9999:59:59")];

	uint32_t seconds = useconds / 1000000;

	// Don't show anything if the time is zero or ridiculously big.
	if (seconds == 0 || seconds > ((9999 * 60) + 59) * 60 + 59)
		return "";

	uint32_t minutes = seconds / 60;
	seconds %= 60;

	if (minutes >= 60) {
		const uint32_t hours = minutes / 60;
		minutes %= 60;
		snprintf(buf, sizeof(buf),
				"%" PRIu32 ":%02" PRIu32 ":%02" PRIu32,
				hours, minutes, seconds);
	} else {
		snprintf(buf, sizeof(buf), "%" PRIu32 ":%02" PRIu32,
				minutes, seconds);
	}

	return buf;
}


/// Make the string to contain the estimated remaining time, or if the amount
/// of input isn't known, how much time has elapsed.
static const char *
progress_remaining(uint64_t in_pos, uint64_t elapsed)
{
	// Show the amount of time spent so far when making an estimate of
	// remaining time wouldn't be reasonable:
	//  - Input size is unknown.
	//  - Input has grown bigger since we started (de)compressing.
	//  - We haven't processed much data yet, so estimate would be
	//    too inaccurate.
	//  - Only a few seconds has passed since we started (de)compressing,
	//    so estimate would be too inaccurate.
	if (expected_in_size == 0 || in_pos > expected_in_size
			|| in_pos < (UINT64_C(1) << 19) || elapsed < 8000000)
		return progress_time(elapsed);

	// Calculate the estimate. Don't give an estimate of zero seconds,
	// since it is possible that all the input has been already passed
	// to the library, but there is still quite a bit of output pending.
	uint32_t remaining = (double)(expected_in_size - in_pos)
			* ((double)(elapsed) / 1e6) / (double)(in_pos);
	if (remaining < 1)
		remaining = 1;

	static char buf[sizeof("9 h 55 min")];

	// Select appropriate precision for the estimated remaining time.
	if (remaining <= 10) {
		// At maximum of 10 seconds remaining.
		// Show the number of seconds as is.
		snprintf(buf, sizeof(buf), "%" PRIu32 " s", remaining);

	} else if (remaining <= 50) {
		// At maximum of 50 seconds remaining.
		// Round up to the next multiple of five seconds.
		remaining = (remaining + 4) / 5 * 5;
		snprintf(buf, sizeof(buf), "%" PRIu32 " s", remaining);

	} else if (remaining <= 590) {
		// At maximum of 9 minutes and 50 seconds remaining.
		// Round up to the next multiple of ten seconds.
		remaining = (remaining + 9) / 10 * 10;
		snprintf(buf, sizeof(buf), "%" PRIu32 " min %" PRIu32 " s",
				remaining / 60, remaining % 60);

	} else if (remaining <= 59 * 60) {
		// At maximum of 59 minutes remaining.
		// Round up to the next multiple of a minute.
		remaining = (remaining + 59) / 60;
		snprintf(buf, sizeof(buf), "%" PRIu32 " min", remaining);

	} else if (remaining <= 9 * 3600 + 50 * 60) {
		// At maximum of 9 hours and 50 minutes left.
		// Round up to the next multiple of ten minutes.
		remaining = (remaining + 599) / 600 * 10;
		snprintf(buf, sizeof(buf), "%" PRIu32 " h %" PRIu32 " min",
				remaining / 60, remaining % 60);

	} else if (remaining <= 23 * 3600) {
		// At maximum of 23 hours remaining.
		// Round up to the next multiple of an hour.
		remaining = (remaining + 3599) / 3600;
		snprintf(buf, sizeof(buf), "%" PRIu32 " h", remaining);

	} else if (remaining <= 9 * 24 * 3600 + 23 * 3600) {
		// At maximum of 9 days and 23 hours remaining.
		// Round up to the next multiple of an hour.
		remaining = (remaining + 3599) / 3600;
		snprintf(buf, sizeof(buf), "%" PRIu32 " d %" PRIu32 " h",
				remaining / 24, remaining % 24);

	} else if (remaining <= 999 * 24 * 3600) {
		// At maximum of 999 days remaining. ;-)
		// Round up to the next multiple of a day.
		remaining = (remaining + 24 * 3600 - 1) / (24 * 3600);
		snprintf(buf, sizeof(buf), "%" PRIu32 " d", remaining);

	} else {
		// The estimated remaining time is so big that it's better
		// that we just show the elapsed time.
		return progress_time(elapsed);
	}

	return buf;
}


/// Calculate the elapsed time as microseconds.
static uint64_t
progress_elapsed(void)
{
	return my_time() - start_time;
}


/// Get information about position in the stream. This is currently simple,
/// but it will become more complicated once we have multithreading support.
static void
progress_pos(uint64_t *in_pos,
		uint64_t *compressed_pos, uint64_t *uncompressed_pos)
{
	*in_pos = progress_strm->total_in;

	if (opt_mode == MODE_COMPRESS) {
		*compressed_pos = progress_strm->total_out;
		*uncompressed_pos = progress_strm->total_in;
	} else {
		*compressed_pos = progress_strm->total_in;
		*uncompressed_pos = progress_strm->total_out;
	}

	return;
}


extern void
message_progress_update(void)
{
	if (!progress_needs_updating)
		return;

	// Calculate how long we have been processing this file.
	const uint64_t elapsed = progress_elapsed();

#ifndef SIGALRM
	if (progress_next_update > elapsed)
		return;

	progress_next_update = elapsed + 1000000;
#endif

	// Get our current position in the stream.
	uint64_t in_pos;
	uint64_t compressed_pos;
	uint64_t uncompressed_pos;
	progress_pos(&in_pos, &compressed_pos, &uncompressed_pos);

	// Block signals so that fprintf() doesn't get interrupted.
	signals_block();

	// Print the filename if it hasn't been printed yet.
	print_filename();

	// Print the actual progress message. The idea is that there is at
	// least three spaces between the fields in typical situations, but
	// even in rare situations there is at least one space.
	fprintf(stderr, "  %7s %43s   %9s   %10s\r",
		progress_percentage(in_pos, false),
		progress_sizes(compressed_pos, uncompressed_pos, false),
		progress_speed(uncompressed_pos, elapsed),
		progress_remaining(in_pos, elapsed));

#ifdef SIGALRM
	// Updating the progress info was finished. Reset
	// progress_needs_updating to wait for the next SIGALRM.
	//
	// NOTE: This has to be done before alarm(1) or with (very) bad
	// luck we could be setting this to false after the alarm has already
	// been triggered.
	progress_needs_updating = false;

	if (verbosity >= V_VERBOSE && progress_automatic) {
		// Mark that the progress indicator is active, so if an error
		// occurs, the error message gets printed cleanly.
		progress_active = true;

		// Restart the timer so that progress_needs_updating gets
		// set to true after about one second.
		alarm(1);
	} else {
		// The progress message was printed because user had sent us
		// SIGALRM. In this case, each progress message is printed
		// on its own line.
		fputc('\n', stderr);
	}
#else
	// When SIGALRM isn't supported and we get here, it's always due to
	// automatic progress update. We set progress_active here too like
	// described above.
	assert(verbosity >= V_VERBOSE);
	assert(progress_automatic);
	progress_active = true;
#endif

	signals_unblock();

	return;
}


static void
progress_flush(bool finished)
{
	if (!progress_started || verbosity < V_VERBOSE)
		return;

	uint64_t in_pos;
	uint64_t compressed_pos;
	uint64_t uncompressed_pos;
	progress_pos(&in_pos, &compressed_pos, &uncompressed_pos);

	// Avoid printing intermediate progress info if some error occurs
	// in the beginning of the stream. (If something goes wrong later in
	// the stream, it is sometimes useful to tell the user where the
	// error approximately occurred, especially if the error occurs
	// after a time-consuming operation.)
	if (!finished && !progress_active
			&& (compressed_pos == 0 || uncompressed_pos == 0))
		return;

	progress_active = false;

	const uint64_t elapsed = progress_elapsed();
	const char *elapsed_str = progress_time(elapsed);

	signals_block();

	// When using the auto-updating progress indicator, the final
	// statistics are printed in the same format as the progress
	// indicator itself.
	if (progress_automatic) {
		// Using floating point conversion for the percentage instead
		// of static "100.0 %" string, because the decimal separator
		// isn't a dot in all locales.
		fprintf(stderr, "  %7s %43s   %9s   %10s\n",
			progress_percentage(in_pos, finished),
			progress_sizes(compressed_pos, uncompressed_pos, true),
			progress_speed(uncompressed_pos, elapsed),
			elapsed_str);
	} else {
		// The filename is always printed.
		fprintf(stderr, "%s: ", filename);

		// Percentage is printed only if we didn't finish yet.
		// FIXME: This may look weird when size of the input
		// isn't known.
		if (!finished)
			fprintf(stderr, "%s, ",
					progress_percentage(in_pos, false));

		// Size information is always printed.
		fprintf(stderr, "%s", progress_sizes(
				compressed_pos, uncompressed_pos, true));

		// The speed and elapsed time aren't always shown.
		const char *speed = progress_speed(uncompressed_pos, elapsed);
		if (speed[0] != '\0')
			fprintf(stderr, ", %s", speed);

		if (elapsed_str[0] != '\0')
			fprintf(stderr, ", %s", elapsed_str);

		fputc('\n', stderr);
	}

	signals_unblock();

	return;
}


extern void
message_progress_end(bool success)
{
	assert(progress_started);
	progress_flush(success);
	progress_started = false;
	return;
}


static void
vmessage(enum message_verbosity v, const char *fmt, va_list ap)
{
	if (v <= verbosity) {
		signals_block();

		progress_flush(false);

		fprintf(stderr, "%s: ", argv0);
		vfprintf(stderr, fmt, ap);
		fputc('\n', stderr);

		signals_unblock();
	}

	return;
}


extern void
message(enum message_verbosity v, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vmessage(v, fmt, ap);
	va_end(ap);
	return;
}


extern void
message_warning(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vmessage(V_WARNING, fmt, ap);
	va_end(ap);

	set_exit_status(E_WARNING);
	return;
}


extern void
message_error(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vmessage(V_ERROR, fmt, ap);
	va_end(ap);

	set_exit_status(E_ERROR);
	return;
}


extern void
message_fatal(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vmessage(V_ERROR, fmt, ap);
	va_end(ap);

	my_exit(E_ERROR);
}


extern void
message_bug(void)
{
	message_fatal(_("Internal error (bug)"));
}


extern void
message_signal_handler(void)
{
	message_fatal(_("Cannot establish signal handlers"));
}


extern const char *
message_strm(lzma_ret code)
{
	switch (code) {
	case LZMA_NO_CHECK:
		return _("No integrity check; not verifying file integrity");

	case LZMA_UNSUPPORTED_CHECK:
		return _("Unsupported type of integrity check; "
				"not verifying file integrity");

	case LZMA_MEM_ERROR:
		return strerror(ENOMEM);

	case LZMA_MEMLIMIT_ERROR:
		return _("Memory usage limit reached");

	case LZMA_FORMAT_ERROR:
		return _("File format not recognized");

	case LZMA_OPTIONS_ERROR:
		return _("Unsupported options");

	case LZMA_DATA_ERROR:
		return _("Compressed data is corrupt");

	case LZMA_BUF_ERROR:
		return _("Unexpected end of input");

	case LZMA_OK:
	case LZMA_STREAM_END:
	case LZMA_GET_CHECK:
	case LZMA_PROG_ERROR:
		return _("Internal error (bug)");
	}

	return NULL;
}


extern void
message_filters(enum message_verbosity v, const lzma_filter *filters)
{
	if (v > verbosity)
		return;

	fprintf(stderr, _("%s: Filter chain:"), argv0);

	for (size_t i = 0; filters[i].id != LZMA_VLI_UNKNOWN; ++i) {
		fprintf(stderr, " --");

		switch (filters[i].id) {
		case LZMA_FILTER_LZMA1:
		case LZMA_FILTER_LZMA2: {
			const lzma_options_lzma *opt = filters[i].options;
			const char *mode;
			const char *mf;

			switch (opt->mode) {
			case LZMA_MODE_FAST:
				mode = "fast";
				break;

			case LZMA_MODE_NORMAL:
				mode = "normal";
				break;

			default:
				mode = "UNKNOWN";
				break;
			}

			switch (opt->mf) {
			case LZMA_MF_HC3:
				mf = "hc3";
				break;

			case LZMA_MF_HC4:
				mf = "hc4";
				break;

			case LZMA_MF_BT2:
				mf = "bt2";
				break;

			case LZMA_MF_BT3:
				mf = "bt3";
				break;

			case LZMA_MF_BT4:
				mf = "bt4";
				break;

			default:
				mf = "UNKNOWN";
				break;
			}

			fprintf(stderr, "lzma%c=dict=%" PRIu32
					",lc=%" PRIu32 ",lp=%" PRIu32
					",pb=%" PRIu32
					",mode=%s,nice=%" PRIu32 ",mf=%s"
					",depth=%" PRIu32,
					filters[i].id == LZMA_FILTER_LZMA2
						? '2' : '1',
					opt->dict_size,
					opt->lc, opt->lp, opt->pb,
					mode, opt->nice_len, mf, opt->depth);
			break;
		}

		case LZMA_FILTER_X86:
			fprintf(stderr, "x86");
			break;

		case LZMA_FILTER_POWERPC:
			fprintf(stderr, "powerpc");
			break;

		case LZMA_FILTER_IA64:
			fprintf(stderr, "ia64");
			break;

		case LZMA_FILTER_ARM:
			fprintf(stderr, "arm");
			break;

		case LZMA_FILTER_ARMTHUMB:
			fprintf(stderr, "armthumb");
			break;

		case LZMA_FILTER_SPARC:
			fprintf(stderr, "sparc");
			break;

		case LZMA_FILTER_DELTA: {
			const lzma_options_delta *opt = filters[i].options;
			fprintf(stderr, "delta=dist=%" PRIu32, opt->dist);
			break;
		}

		default:
			fprintf(stderr, "UNKNOWN");
			break;
		}
	}

	fputc('\n', stderr);
	return;
}


extern void
message_try_help(void)
{
	// Print this with V_WARNING instead of V_ERROR to prevent it from
	// showing up when --quiet has been specified.
	message(V_WARNING, _("Try `%s --help' for more information."), argv0);
	return;
}


extern void
message_version(void)
{
	// It is possible that liblzma version is different than the command
	// line tool version, so print both.
	printf("xz (" PACKAGE_NAME ") " LZMA_VERSION_STRING "\n");
	printf("liblzma %s\n", lzma_version_string());
	my_exit(E_SUCCESS);
}


extern void
message_help(bool long_help)
{
	printf(_("Usage: %s [OPTION]... [FILE]...\n"
			"Compress or decompress FILEs in the .xz format.\n\n"),
			argv0);

	puts(_("Mandatory arguments to long options are mandatory for "
			"short options too.\n"));

	if (long_help)
		puts(_(" Operation mode:\n"));

	puts(_(
"  -z, --compress      force compression\n"
"  -d, --decompress    force decompression\n"
"  -t, --test          test compressed file integrity\n"
"  -l, --list          list information about files"));

	if (long_help)
		puts(_("\n Operation modifiers:\n"));

	puts(_(
"  -k, --keep          keep (don't delete) input files\n"
"  -f, --force         force overwrite of output file and (de)compress links\n"
"  -c, --stdout        write to standard output and don't delete input files"));

	if (long_help)
		puts(_(
"  -S, --suffix=.SUF   use the suffix `.SUF' on compressed files\n"
"      --files=[FILE]  read filenames to process from FILE; if FILE is\n"
"                      omitted, filenames are read from the standard input;\n"
"                      filenames must be terminated with the newline character\n"
"      --files0=[FILE] like --files but use the null character as terminator"));

	if (long_help) {
		puts(_("\n Basic file format and compression options:\n"));
		puts(_(
"  -F, --format=FMT    file format to encode or decode; possible values are\n"
"                      `auto' (default), `xz', `lzma', and `raw'\n"
"  -C, --check=CHECK   integrity check type: `crc32', `crc64' (default),\n"
"                      or `sha256'"));
	}

	puts(_(
"  -0 .. -9            compression preset; 0-2 fast compression, 3-5 good\n"
"                      compression, 6-9 excellent compression; default is 6"));

	puts(_(
"  -e, --extreme       use more CPU time when encoding to increase compression\n"
"                      ratio without increasing memory usage of the decoder"));

	if (long_help)
		puts(_(
"  -M, --memory=NUM    use roughly NUM bytes of memory at maximum; 0 indicates\n"
"                      the default setting, which depends on the operation mode\n"
"                      and the amount of physical memory (RAM)"));

	if (long_help) {
		puts(_(
"\n Custom filter chain for compression (alternative for using presets):"));

#if defined(HAVE_ENCODER_LZMA1) || defined(HAVE_DECODER_LZMA1) \
		|| defined(HAVE_ENCODER_LZMA2) || defined(HAVE_DECODER_LZMA2)
		puts(_(
"\n"
"  --lzma1[=OPTS]      LZMA1 or LZMA2; OPTS is a comma-separated list of zero or\n"
"  --lzma2[=OPTS]      more of the following options (valid values; default):\n"
"                        preset=NUM reset options to preset number NUM (0-9)\n"
"                        dict=NUM   dictionary size (4KiB - 1536MiB; 8MiB)\n"
"                        lc=NUM     number of literal context bits (0-4; 3)\n"
"                        lp=NUM     number of literal position bits (0-4; 0)\n"
"                        pb=NUM     number of position bits (0-4; 2)\n"
"                        mode=MODE  compression mode (fast, normal; normal)\n"
"                        nice=NUM   nice length of a match (2-273; 64)\n"
"                        mf=NAME    match finder (hc3, hc4, bt2, bt3, bt4; bt4)\n"
"                        depth=NUM  maximum search depth; 0=automatic (default)"));
#endif

		puts(_(
"\n"
"  --x86[=OPTS]        x86 BCJ filter\n"
"  --powerpc[=OPTS]    PowerPC BCJ filter (big endian only)\n"
"  --ia64[=OPTS]       IA64 (Itanium) BCJ filter\n"
"  --arm[=OPTS]        ARM BCJ filter (little endian only)\n"
"  --armthumb[=OPTS]   ARM-Thumb BCJ filter (little endian only)\n"
"  --sparc[=OPTS]      SPARC BCJ filter\n"
"                      Valid OPTS for all BCJ filters:\n"
"                        start=NUM  start offset for conversions (default=0)"));

#if defined(HAVE_ENCODER_DELTA) || defined(HAVE_DECODER_DELTA)
		puts(_(
"\n"
"  --delta[=OPTS]      Delta filter; valid OPTS (valid values; default):\n"
"                        dist=NUM   distance between bytes being subtracted\n"
"                                   from each other (1-256; 1)"));
#endif

#if defined(HAVE_ENCODER_SUBBLOCK) || defined(HAVE_DECODER_SUBBLOCK)
		puts(_(
"\n"
"  --subblock[=OPTS]   Subblock filter; valid OPTS (valid values; default):\n"
"                        size=NUM   number of bytes of data per subblock\n"
"                                   (1 - 256Mi; 4Ki)\n"
"                        rle=NUM    run-length encoder chunk size (0-256; 0)"));
#endif
	}

	if (long_help)
		puts(_("\n Other options:\n"));

	puts(_(
"  -q, --quiet         suppress warnings; specify twice to suppress errors too\n"
"  -v, --verbose       be verbose; specify twice for even more verbose"));

	if (long_help)
		puts(_(
"  -Q, --no-warn       make warnings not affect the exit status"));

	if (long_help)
		puts(_(
"\n"
"  -h, --help          display the short help (lists only the basic options)\n"
"  -H, --long-help     display this long help"));
	else
		puts(_(
"  -h, --help          display this short help\n"
"  -H, --long-help     display the long help (lists also the advanced options)"));

	puts(_(
"  -V, --version       display the version number"));

	puts(_("\nWith no FILE, or when FILE is -, read standard input.\n"));

	if (long_help) {
		printf(_(
"On this system and configuration, this program will use at maximum of roughly\n"
"%s MiB RAM and "), uint64_to_str(hardware_memlimit_get() / (1024 * 1024), 0));
		printf(N_("one thread.\n\n", "%s threads.\n\n",
				hardware_threadlimit_get()),
				uint64_to_str(hardware_threadlimit_get(), 0));
	}

	printf(_("Report bugs to <%s> (in English or Finnish).\n"),
			PACKAGE_BUGREPORT);
	printf(_("%s home page: <%s>\n"), PACKAGE_NAME, PACKAGE_HOMEPAGE);

	my_exit(E_SUCCESS);
}
