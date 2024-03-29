/* Copyright (c) 2022, 2023, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License, version 2.0,
as published by the Free Software Foundation.

This program is also distributed with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have included with MySQL.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License, version 2.0, for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/**
  @file log_source_backtrace.cc

  This file contains contains code to search for a recent stackdump in
  a file formatted in the "traditional error log" format. If one is
  found, it is prepended to the buffered log-events.
*/

#include <my_time.h>          // MYSQL_TIME_STATUS
#include <mysql_time.h>       // MYSQL_TIME
#include <iostream>           // std::cerr
#include "log_sink_buffer.h"  // struct log_line_buffer
#include "log_sink_trad.h"    // parse_trad_field()
#include "my_dir.h"           // MY_STAT
#include "my_loglevel.h"
#include "mysql/components/services/log_builtins.h"  // LogEvent()
#include "mysql/components/services/log_service.h"
#include "mysqld_error.h"  // ER_STACKTRACE
#include "sql/log.h"       // iso8601_timestamp_to_microseconds()
#include "sql/sql_list.h"  // List

/// How far back in the trad error-log should we start looking?
static const size_t max_backtrace = 32 * 1024;  // be generous, it's temporary

/// Latest ISO8601 timestamp found outside a stackdump.
static ulonglong iso8601_outside_stack = 0;
/// ISO8601 timestamp in current stackdump.
static ulonglong iso8601_during_stack = 0;

/// Pointer to first character of a textual stacktrace, or nullptr.
static char *backtrace_beg = nullptr;
/// Pointer to last character of a textual stacktrace, or nullptr.
static char *backtrace_end = nullptr;

/**
  If a stack backtrace was found in the traditional error log,
  prepend its lines to buffered logging (so the backtrace may
  be flushed to performance_schema.error_log and all configured
  log-sinks later).

  `backtrace_beg` must be set to the beginning of a char-buffer
  containing a textual stacktrace.

  `backtrace_end` must point to the last valid character in the
  textual stacktrace.

  The "textual representation" must consist of one or several
  lines terminated by '\n'. These '\n' will be replaced with '\0'.

  Each line will become one log-event.
  The list of log-events, if any, will be prepended to the list of log-events
  buffered during start-up.

  After this function returns, the caller is free to free() the buffer
  containing the log file fragment.
*/
static log_service_error log_source_backtrace_add_events() {
  // Head of the list of buffered log messages we're creating
  log_line_buffer *log_line_backtrace_head = nullptr;

  // Where to write the pointer to the newly-created tail-element of the list
  log_line_buffer **log_line_backtrace_tail = &log_line_backtrace_head;

  // List element we creating
  log_line_buffer *lle;

  char *my_line = backtrace_beg;
  log_service_error ret = LOG_SERVICE_SUCCESS;  // return value

  assert((backtrace_beg != nullptr) && (backtrace_end != nullptr));

  // Add backtrace to buffered log, each line becoming one log event.
  while (my_line < backtrace_end) {
    char *my_eol =
        (char *)memchr(my_line, '\n', (size_t)(backtrace_end - my_line + 1));

    // Fail-safe. Failing to parse is bad, but breaking start-up is worse.
    if (my_eol == nullptr) break;

    // Only add lines that aren't empty.
    if (my_eol > my_line) {
      *my_eol = '\0';

      // Allocate an element for our singly-linked list.
      if ((lle = (log_line_buffer *)my_malloc(
               key_memory_log_error_stack, sizeof(log_line_buffer), MYF(0))) ==
          nullptr) {
        ret = LOG_SERVICE_OUT_OF_MEMORY; /* purecov: inspected */
        break;                           /* purecov: inspected */
      }

      /*
        Create a log-event from the backtrace line.
        Note the use of message() instead of verbatim() in order to copy
        the message into the LogEvent's allocation.
        To use verbatim(), we'd have to manage the message's life-cycle
        ourselves instead.
      */
      log_line *ll;
      LogEvent()
          .type(LOG_TYPE_ERROR)
          .errcode(ER_STACK_BACKTRACE)
          .subsys(LOG_SUBSYSTEM_TAG)
          .prio(SYSTEM_LEVEL)      // make it unfilterable
          .message("%s", my_line)  // line from the backtrace
          .steal(&ll);             // obtain pointer to the event-data

      // Add a timestamp to the event.
      // The first event gets the timestamp we parsed from the textual
      // stacktrace (contained in `iso8601_during_stack`); subsequent
      // lines strictly increase this timestamp to guarantee row order.
      log_line_item_set(ll, LOG_ITEM_LOG_BUFFERED)->data_integer =
          iso8601_during_stack++;

      // Put event in our single-linked list list of backtrace-events.
      lle->next = nullptr;
      lle->ll = *ll;  // Shallow-copy the log-event we created above.

      // Release the event we created, but keep its allocations
      // (as they're still used by the copy).
      log_line_exit(ll);

      // Append event to list of backtrace log-events.
      *log_line_backtrace_tail = lle;  // current_tail->next = new_element
      log_line_backtrace_tail = &(lle->next);
    }

    my_line = ++my_eol;  // Next line in buffer starts after '\n'.
  }

  // Prepend our singly-linked list of stacktrace events
  // to the singly-linked list of start-up events.
  // Flushing this later will free it.
  log_sink_buffer_prepend_list(log_line_backtrace_head,
                               log_line_backtrace_tail);

  return ret;
}

/**
  Parse an ISO-8601 compliant timestamp.

  @param line_start  Pointer to the first character of the line.
  @param line_end    Pointer to the '\n' ending the line.

  @retval Number of micro-seconds since the epoch represented by the timestamp
*/
ulonglong log_iso8601_from_line(const char *line_start, const char *line_end) {
  ulonglong iso8601_time = 0;
  const char *end;
  char timestamp[iso8601_size];  // space for timestamp + '\0'
  ssize_t len;

  if ((len = parse_trad_field(line_start, &end, line_end)) > 0) {
    if (len < iso8601_size) {
      memcpy(timestamp, line_start, len);
      timestamp[len] = '\0';  // terminate target buffer

      // The parse-function corrects for timezone.
      iso8601_time = iso8601_timestamp_to_microseconds(timestamp, len);
    }
  }

  return iso8601_time;
}

/**
  Parse a single line in the traditional error-log.

  This function may be called to examine a buffer containing
  part of an error log line by line. If the header of a stack
  backtrace is recognized, certain variables are set up; if
  the end of a backtrace is recognized, those variables are
  cleared again. In other words, if the all the loaded log-lines
  have been examined and the variables are non-zero, the last
  chunk in the error log was a backtrace, and we have saved

  - `iso8601_during_stack` is non-zero and contains the
    time/date (micro-seconds since the epoch, UTC) of the backtrace.

  - `backtrace_beg` is non-null and points to the first character
    of the backtrace.

  - `backtrace_end` is non-null and points to the last character
    in the backtrace.

  `backtrace_beg` and `backtrace_end` if set point to addresses
  in the temporary buffer that contains the error log fragment.

  This function will attempt to determine whether the last item
  in the traditional error log is a backtrace and if so, identify
  the beginning and end of that backtrace in the buffer. It does
  _not_ copy the information, add it to performance_schema.error_log,
  and so forth; this is something the caller can see to if desired.

  @param line_start   Pointer to the first character of the line.
  @param line_length  Length of the line in bytes.

  @retval LOG_SERVICE_MISC_ERROR  log was written to after server start
*/
static log_service_error log_source_backtrace_parse_line(char *line_start,
                                                         size_t line_length) {
  const char *line_end = line_start + line_length;
  bool is_iso8601 = false;

  if (*line_end != '\n')
    return LOG_SERVICE_PARSE_ERROR;  // lines are '\n' separated

  /*
    ISO8601 sanity check ("2022-04-26T23:45:06[Z.]")
    If it's a stacktrace, there will be no micro-seconds ('Z');
    if it's a normal trad-log line, micro-seconds will follow ('.').

    Note that before WL#14955, stacktrace timestamps had no date part.
    These old stacktraces will be disregarded here. This is intentional.
  */
  if ((line_length >= 20) &&  // must start with timestamp
      (line_start[0] == '2') && (line_start[4] == '-') &&
      (line_start[7] == '-') && (line_start[10] == 'T') &&
      (line_start[13] == ':') && (line_start[16] == ':') &&
      ((line_start[19] == '.') || (line_start[19] == 'Z')))
    is_iso8601 = true;

  // First, let's see whether it's the beginning of a stacktrace.
  // e.g. "08:00:18 UTC - mysqld got signal 7 ;"
  // Stacktraces are always in UTC (i.e. 'Z': no timezone offset).
  if (is_iso8601 && (line_length >= 27) &&
      (0 == strncmp(line_start + 19, "Z UTC - ", 8))) {  // It's always UTC.
    // Get timestamp. There will be no micro-second part.
    iso8601_during_stack = log_iso8601_from_line(line_start, line_end);

    // If it's before the last full timestamp we've seen, adjust micro-seconds.
    if (iso8601_during_stack <= iso8601_outside_stack) {
      /*
        This can happen if both timestamps are in the same second
        since the trad-log timestamp has a micro-second part, and
        the backtrace-timestamp doesn't.
        In this case, we just advance the backtrace timestamp to
        the next "free" microsecond, i.e. the micro-second after
        the last full timestamp we've seen.
      */
      iso8601_during_stack = iso8601_outside_stack + 1;
    }

    // We save the beginning of the line as the beginning of the stacktrace.
    backtrace_beg = line_start;
    backtrace_end = line_start + line_length;
  }

  // See whether it's a trad log line. (If so, remember the timestamp.)
  // The trad log's timestamp will have microseconds ('.').
  // Its timezone could be anything.
  else if (is_iso8601 && (line_start[19] == '.')) {
    // ISO8601: 2022-02-21T03:30:34.561771 (timezone could be anything)

    /*
      If we find a trad-line after a backtrace, throw the backtrace away.
      The assumption is that either

      a) the server crashed, wrote the backtrace, and ended --
         in which case, the backtrace is the very last thing in the trad log
         (and we'll process it):

         ...
         2022-03-16T16:17:28.299339Z 0 [System] [MY-010116] [Server] trad stuff
         12:34:56 UTC - mysqld got signal 1 ;   // backtrace header
         // log ends

         or

      b) there is a backtrace, but it's not the last section in the trad log --
         in which case, the server has run between the failure we found and the
         current start-up. In this case, we assume that the stackdump has
         already been processed in that previous run. In this case, there
         should be a processed (correctly formatted) copy of the stackdump
         immediately following the raw version. Thus, we'll skip the raw
         version, since we'll be reading the processed one later, anyway.

         ...
         2022-03-16T06:17:28.299339Z 0 [System] [MY-010116] [Server] trad stuff
         12:34:56 UTC - mysqld got signal 1 ;   // backtrace header
         // log ends

         // server restarted, processed above backtrace, and logged it:
         2022-03-16T12:34:56.000001Z 0 [System] [MY-010116] [trace] 12:34:56 UTC
         2022-03-16T12:35:00.299339Z 0 [System] [MY-010116] [Server] trad stuff
         // Later, we shutdown cleanly, and the log ends, now containing both
         // the raw stackdump followed by the properly formatted one.

         // Server is restarted a second time. The raw backtrace may be found,
         // but is ignored as it is not the last section in the log. The
         // formatted version of the stackdump behind it however is parsed
         // and read into performance_schema.error_log -- if the traditional
         // log is configured to be the main error log. If a different log
         // (e.g. JSON) is configured to be the main error log, the formatted
         // stackdump will be read thence instead (i.e. different log, same
         // principle).
    */

    // Get timestamp from line.
    iso8601_outside_stack = log_iso8601_from_line(line_start, line_end);

    if (iso8601_outside_stack > 0) {  // If we got a timestamp from the line ...
      // If read timestamp is later than server_start, stop reading.
      // This should not happen, but we need a failsafe.
      if (iso8601_outside_stack > log_builtins_started()) {
        return LOG_SERVICE_MISC_ERROR;
      }

      // If we already processed a backtrace, ignore it, it's not current.
      backtrace_beg = backtrace_end = nullptr;
      iso8601_during_stack = 0;

      return LOG_SERVICE_NOTHING_DONE;  // success, of a sort
    }
  }  // if trad-line ...

  // It's not a trad-line, and we've already detected a backtrace header.
  // Expand the backtrace-buffer to include this line.
  if (iso8601_during_stack > 0) {
    backtrace_end = line_start + line_length;
    return LOG_SERVICE_SUCCESS;
  }

  /*
    If we get here, we haven't seen a backtrace-header yet (after which
    we relax the rules and accept all kinds of lines), but we didn't get
    a correct ISO8601 timestamp, either.

    This is slightly odd, but can happen in situations such as:

    a) A 3rd party library writes debug info to stdout/stderr.

    b) The last item in the log was a stacktrace written by an
       older, pre-WL#14955 server that does not include the date
       in stacktrace-timestamps. We intentionally disregard such
       stacktraces.

    c) The server stopped as the result of a failed assert(),
       the output of which we just encountered.

    d) mysql-test-run.pl includes "\nCURRENT_TEST: " line.
  */

  // We ignore empty lines (for mysql-test-run.pl etc.).
  if ((line_end - line_start) == 0) return LOG_SERVICE_NOTHING_DONE;

  // Unrecognized line in traditional error log file.
  return LOG_SERVICE_PARSE_ERROR;
}

/**
  Read tail end of the traditional error-log as a single chunk.
  Look for a recent stacktrace in that data.

  @param  log_file   The file's name
  @param  size       length of the input file (in bytes)

  @retval LOG_SERVICE_SUCCESS            success
  @retval LOG_SERVICE_OPEN_FAILED        failed to open file
  @retval LOG_SERVICE_SEEK_FAILED        seek failed
  @retval LOG_SERVICE_UNABLE_TO_READ     read failed or file empty
  @retval LOG_SERVICE_OUT_OF_MEMORY      out of memory
  @retval LOG_SERVICE_PARSE_ERROR        could not find delimiter ('\n')
  @retval LOG_SERVICE_MISC_ERROR         parsing: sanity test failed
*/
static log_service_error log_error_read_backtrace_loop(const char *log_file,
                                                       size_t size) {
  FILE *fh;                     // file-handle
  off_t pos;                    // where in the file to start reading
  char *chunk;                  // beginning of allocation
  char *line_start, *line_end;  // beginning/end of line
  log_service_error ret = LOG_SERVICE_SUCCESS;  // return value

  // Reset backtrace window.
  backtrace_beg = backtrace_end = nullptr;

  // Is there any data to read?
  if (size <= 0) return LOG_SERVICE_UNABLE_TO_READ;

  if ((fh = my_fopen(log_file, O_RDONLY, MYF(0))) == nullptr)
    return LOG_SERVICE_OPEN_FAILED;

  // Allocate memory to read the tail of the log into.
  if ((chunk = (char *)my_malloc(key_memory_log_sink_pfs, max_backtrace,
                                 MYF(0))) == nullptr) {
    ret = LOG_SERVICE_OUT_OF_MEMORY; /* purecov: inspected */
    goto fail_with_close;
  }

  /*
    If the file would fit into the buffer entirely, we'll read it from
    the beginning. Otherwise, we start reading from a point in the file
    where about the size of the buffer remains as input.
  */
  if (size <= max_backtrace)
    pos = 0;
  else {
    pos = size - max_backtrace;
    size = max_backtrace;  // Correct size to the part we'll actually read.

    // Seek to the approximate position of the row to start reading at.
    if (my_fseek(fh, (long)pos, SEEK_SET)) { /* purecov: begin inspected */
      ret = LOG_SERVICE_SEEK_FAILED;
      goto fail_with_free; /* purecov: end */
    }
  }

  if (my_fread(fh, (uchar *)chunk, size, MYF(0)) < size) {
    ret = LOG_SERVICE_UNABLE_TO_READ; /* purecov: begin inspected */
    goto fail_with_free;              /* purecov: end */
  }

  if (pos > 0) {
    // We're likely in the middle of a row, skip forward to the next.
    if ((line_start = (char *)memchr(chunk, '\n', size)) == nullptr) {
      ret = LOG_SERVICE_PARSE_ERROR;
      goto fail_with_free;
    }
  } else
    line_start = chunk;

  /*
    Process the data line-by-line until
    - we reach the end of the data
    - the line-delimiter '\n' is not found
    - the parse function suggests we stop (LOG_SERVICE_MISC_ERROR)
  */
  do {
    size_t processed = (size_t)(line_start - chunk);
    size_t rest = size - processed;

    // Find EOL ('\n'). If last line is partial, skip it.
    if ((line_end = (char *)memchr(line_start, '\n', rest)) == nullptr) break;

    // Parse the current line. We are trying to determine whether the
    // last part of the error log is a backtrace. If so, we'll prepend
    // its lines to buffered logging further below. This data will then
    // be flushed to all configured log-sinks in their respective formats
    // (e.g. in JSON for the JSON-log) as well as to
    // performance_schema.error_log.
    ret = log_source_backtrace_parse_line(line_start, line_end - line_start);

    // Proceed to the next line (i.e. past '\n').
    line_start = line_end + 1;

  } while ((ret != LOG_SERVICE_MISC_ERROR) && (line_start < (chunk + size)));

  // If we found a backtrace, prepend it to buffered logging.
  if (backtrace_beg != nullptr) ret = log_source_backtrace_add_events();

fail_with_free:
  backtrace_beg = backtrace_end = nullptr;  // Any pointers stale at this point.
  my_free(chunk);

fail_with_close:
  my_fclose(fh, MYF(0));

  return ret;
}

/**
  Read stacktrace from previous failure.

  The signal-handler attempts to write a stacktrace to stderr.
  As stderr (and stdout) are redirected to the "traditional"
  error-log, that's where we'll have to look for stacktraces,
  even if we use a different log-sink otherwise (e.g. JSON,
  syslog, etc.).

  Once we have determined whether such a log exists and is readable,
  we call @see log_error_read_backtrace_loop() to do the actual reading
  and parsing.

  It should be noted that at the point this function is normally
  called, buffered error logging will not have been flushed yet.

  @param  log_name  The log file to read (log_error_dest).

  @retval LOG_SERVICE_SUCCESS                 Success (log read and parsed)
  @retval LOG_SERVICE_UNABLE_TO_READ          Could not read/access() file
  @retval LOG_SERVICE_INVALID_ARGUMENT        Invalid log-file name
  @retval LOG_SERVICE_ARGUMENT_TOO_LONG       File-name too long
  @retval otherwise                           Return value from reader
*/
log_service_error log_error_read_backtrace(const char *log_name) {
  MY_STAT stat_log;
  log_service_error ret;

  if ((log_name == nullptr) || (log_name[0] == '\0'))
    return LOG_SERVICE_INVALID_ARGUMENT;
  if (strlen(log_name) >= FN_REFLEN)
    return LOG_SERVICE_ARGUMENT_TOO_LONG; /* purecov: inspected */

  assert(0 != strcmp(log_name, "stderr"));

  /*
    Lock the error-logger while we're restoring the error-log so nobody
    writes to the log-file while we're reading it. That way, we won't
    have to deal with half-written lines or the file-size changing.
  */

  if (my_access(log_name, R_OK) ||
      (my_stat(log_name, &stat_log, MYF(0)) == nullptr))
    ret = LOG_SERVICE_UNABLE_TO_READ; /* purecov: inspected */
  else
    ret = log_error_read_backtrace_loop(log_name, (size_t)stat_log.st_size);

  return ret;
}
