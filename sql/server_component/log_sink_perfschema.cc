/* Copyright (c) 2020, 2024, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License, version 2.0,
as published by the Free Software Foundation.

This program is designed to work with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have either included with
the program or referenced in the documentation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License, version 2.0, for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/**
  @file log_sink_perfschema.cc

  This file contains

  a) the ring-buffer that stores a backlog of error-messages so
  they can be exposed to the SQL layer via performance_schema.error_log;

  b) the log-sink that adds errors logged at run-time to the ring-buffer.

  c) the error log reader that reads error log file at start-up
  (These functions will in turn use a parse-function defined
  in a log-sink. Whichever log-sink that has a parse-function
  is listed first in @@global.log_error_services will be used;
  that service will decide what log-file to read (i.e. its name)
  and how to parse it. We initially support the reading of JSON-
  formatted error log files and of the traditional MySQL error
  log files.)
  This lets us restore error log information from previous runs
  when the server starts.
  These functions are called from mysqld.cc at start-up.
*/

#include "log_sink_perfschema.h"
#include <mysql/components/services/log_shared.h>  // data types
#include <string.h>                                // memset()
#include "log_builtins_internal.h"
#include "log_sink_perfschema_imp.h"
#include "log_sink_trad.h"  // log_sink_trad_parse_log_line()
#include "my_dir.h"
#include "my_systime.h"  // my_micro_time()
#include "mysql/components/services/log_builtins.h"
#include "mysql/my_loglevel.h"
#include "mysqld_error.h"
#include "mysys_err.h"
#include "sql/log.h"

/**
  In the interest of not adding more settings to confuse the using,
  the error-log ring-buffer is of a static size for now. This will
  be easy enough to change later if needs or policy change.

  While a log-event can currently be up to 8 KB in size (and with
  minor changes be of practically arbitrary size), a majority of
  common events seem to be in the 150 - 200 bytes range (in trad
  mode, perhaps 100 more each in JSON mode) at the time of this
  writing.  That lead us to expect a yield of 4-6 events per KB,
  and thus about 25,000 for a buffer of 5 MB.
*/
static const size_t ring_buffer_size = 5 * 1024 * 1024;

static char *ring_buffer_start = nullptr;  ///< buffer start
static char *ring_buffer_end = nullptr;    ///< buffer end (for convenience)

static char *ring_buffer_write = nullptr;  ///< write position ("head")
static char *ring_buffer_read = nullptr;   ///< read pos (oldest entry, "tail")

ulong log_sink_pfs_buffered_bytes = 0;   ///< bytes in use (now)
ulong log_sink_pfs_buffered_events = 0;  ///< events in buffer (now)
ulong log_sink_pfs_expired_events = 0;   ///< number of expired entries (ever)
ulong log_sink_pfs_longest_event = 0;    ///< longest event seen (ever)
ulonglong log_sink_pfs_latest_timestamp =
    0;  ///< timestamp of most recent write

PSI_memory_key key_memory_log_sink_pfs;  ///< memory instrumentation

/**
  ring-buffer rwlock
  Local to the sink that feeds the table performance_schema.error_log.
  Code outside of the sink can acquire / release this lock using
  log_sink_pfs_read_start() / log_sink_pfs_read_start().
*/
static mysql_rwlock_t THR_LOCK_log_perfschema;

#define LOG_ERR_READ_LINE_SIZE (LOG_BUFF_MAX * 2)

/**
  Calculate the size of the given event (header + blob + '\0' + alignment).

  The header is followed by a blob (error message or JSON representation
  of the complete event) and a '\0' terminator (for safety); it is then
  aligned to the correct address boundary if needed.

  @param  e  The event we're interested in. Must be non-NULL.

  @retval    The total size (header + message + '\0' + padding) in bytes.
*/
static inline size_t log_sink_pfs_event_size(log_sink_pfs_event *e) {
  assert(e != nullptr);

  size_t s = sizeof(log_sink_pfs_event) + ((size_t)e->m_message_length) + 1;
  s = MY_ALIGN(s, sizeof(ulonglong));

  return s;
}

/**
  Test whether we're so close to the end of the ring-buffer that
  another event header would not fit.

  @param p      The address where we'd like to write (e.g. ring_buffer_write)

  @retval true  There is enough space to write a header.
  @retval false Insufficient space, must wrap around to buffer-start to write.
*/
static inline bool log_sink_pfs_event_header_fits(char *p) {
  return ((p + sizeof(log_sink_pfs_event)) <= ring_buffer_end);
}

/**
  Acquire a read-lock on the ring-buffer.
*/
void log_sink_pfs_read_start() {
  mysql_rwlock_rdlock(&THR_LOCK_log_perfschema);
}

/**
  Release read-lock on ring-buffer.
*/
void log_sink_pfs_read_end() { mysql_rwlock_unlock(&THR_LOCK_log_perfschema); }

/**
  Get number of events currently in ring-buffer.
  Caller should hold THR_LOCK_log_perschema when reading this.

  @returns  number of events current in ring-buffer (0..)
*/
size_t log_sink_pfs_event_count() { return log_sink_pfs_buffered_events; }

/**
  Get oldest event still in ring-buffer.
  Caller should hold read-lock on THR_LOCK_log_perfschema when calling this.

  @retval  nullptr    No events in buffer
  @retval  otherwise  Address of oldest event in ring-buffer
*/
log_sink_pfs_event *log_sink_pfs_event_first() {
  if (log_sink_pfs_buffered_events == 0) return nullptr;
  return (log_sink_pfs_event *)ring_buffer_read;
}

/**
  Get event following the supplied one.
  Caller should hold read-lock on THR_LOCK_log_perfschema when calling this.

  If advancing the read position puts the read-pointer beyond the
  highest-address event in the ring-buffer (which isn't necessarily
  the latest event, which is defined as the last event before catching
  up with the write-pointer), i.e. at a position where either a wrap-
  around marker exists, or there is not enough space for a header,
  we wrap around to the start of the ring-buffer.

  @param   e          Last event the caller was processing.
                      This event should be valid, non-NULL,
                      and should not be a wrap-around marker
                      (m_messages_length == 0).

  @retval  nullptr    No more events in ring-buffer (caught up with writer)
  @retval  otherwise  Address of the next event in the ring-buffer
*/
log_sink_pfs_event *log_sink_pfs_event_next(log_sink_pfs_event *e) {
  char *n;  // pointer to next event

  // pre-condition: e must be a valid event
  assert(e != nullptr);  // do not accept nullptr
  assert(e->m_message_length !=
         0);  // current event should not be wrap-around marker

  // next event's location is current event's location plus its size
  n = ((char *)e) + log_sink_pfs_event_size(e);

  // We've caught up with the head (write-position): no more events
  if (n == ring_buffer_write) return nullptr;

  /*
    Wrap-around handling: The Ring

    If we're so close to the end of the ring-buffer that there is not
    enough space for another header, or a header exists but indicates
    its blob didn't fit, wrap around to start of ring-buffer.
  */
  if (!log_sink_pfs_event_header_fits(n) ||
      (((log_sink_pfs_event *)n)->m_message_length == 0)) {
    n = ring_buffer_start;

    /*
      Fail-safe: If there is only space for a single event,
      head and tail are now the same. In this case, we fail to read.
      This is a somewhat synthetic case in that it should only happen
      with minuscule buffer sizes (a few KB or less).
    */
    if (n == ring_buffer_write) return nullptr;
  }

  // next event should not be wrap-around marker
  assert(((log_sink_pfs_event *)n)->m_message_length != 0);

  return (log_sink_pfs_event *)n;
}

/**
  Use timestamp to check whether a given event-pointer still points
  to a valid event in the ring-buffer.
  Caller should hold read-lock on THR_LOCK_log_perfschema when calling this.

  @param   e          Address of event
  @param   logged     unique timestamp of event

  @retval  nullptr    Event no longer exists in ring-buffer
  @retval  otherwise  Address of the event in the ring-buffer
*/
log_sink_pfs_event *log_sink_pfs_event_valid(log_sink_pfs_event *e,
                                             ulonglong logged) {
  assert(e != nullptr);
  assert(ring_buffer_read != nullptr);

  // If the ring-buffer is empty, the event won't be there.
  if (log_sink_pfs_buffered_events == 0) return nullptr;

  /*
    If the requested timestamp is older than the oldest item
    in the ring-buffer, bail.  This is a valid condition.
    This usually means that the event existed earlier, but
    has since been discarded from the ring-buffer to make
    room for new events.
  */
  if (logged < ((log_sink_pfs_event *)ring_buffer_read)->m_timestamp)
    return nullptr; /* purecov: inspected */

  // Request's timestamp shouldn't be in the future.
  assert(logged <= log_sink_pfs_latest_timestamp);

  // Request's timestamp should equal that at the address we were given.
  assert(logged == e->m_timestamp);

  // If we got here, the event still exists in the ring-buffer.
  return e;
}

// expire tail event (oldest event in buffer) by adjusting the read-pointer
static inline void log_sink_pfs_event_expire(void) {
  assert(log_sink_pfs_buffered_events > 0);
  assert(ring_buffer_read != nullptr);

  log_sink_pfs_buffered_bytes -=
      log_sink_pfs_event_size((log_sink_pfs_event *)ring_buffer_read);
  log_sink_pfs_buffered_events--;
  log_sink_pfs_expired_events++;

  ring_buffer_read =
      (char *)log_sink_pfs_event_next((log_sink_pfs_event *)ring_buffer_read);
}

/**
  If the current event can fit in the ring-buffer, but the write position
  is so close to the physical end of the ring-buffer that the event won't
  fit there, wrap to the beginning of the ring-buffer. Write a wrap-marker
  if possible. Adjust the pointers as needed:

  If there isn't enough space to write the current event (of size ``s``):
  a) if the read-pointer > write-pointer (which should always be the case
     after the first wrap), we wrap the read-pointer to the physical start
     of the ring-buffer
  b) write a wrap-marker if space permits
  c) wrap the write-pointer to the (physical) start of the ring-buffer

  Caller must guarantee that event size <= ring-buffer-size.

  @retval 0  didn't need to wrap
  @retval 1  write-pointer wrapped
  @retval 2  write-pointer and read-pointer wrapped
*/
static inline int log_sink_pfs_write_wrap(size_t s) {
  int ret = 0;

  assert(s <= ring_buffer_size);

  // Writing the event would go past the end of the buffer. Wrap around!
  if ((ring_buffer_write + s) > ring_buffer_end) {
    ret = 1;

    // After the first wrap (i.e. read > write): If we wrap write, also wrap
    // read. The buffer is empty, read is NULL here and we skip this branch.
    if (ring_buffer_read >= ring_buffer_write) {
      ret = 2;

      assert(log_sink_pfs_buffered_events > 0);

      // Expire high-address entries individually so statistics are correct
      while (ring_buffer_read >= ring_buffer_write) {
        assert(log_sink_pfs_buffered_events > 0);
        log_sink_pfs_event_expire();
      }

      /*
        If the tail didn't wrap (so we ran out of events before the read pointer
        would wrap, resulting in ring_buffer_read == NULL), something's strange.
        (It is implied here that (ring_buffer_write > ring_buffer_start) --
        otherwise, (s<=ring_buffer_size) would be false --, so wrapping around
        (ring_buffer_read = ring_buffer_start) and immediately satisfying
        ring_buffer_read == ring_buffer_write (with the resulting setting
        of ring_buffer_read to NULL) should not happen.
      */
      assert(ring_buffer_read == ring_buffer_start);
    }

    /*
      If there is enough space for a header, write a wrap-around marker.
      (The header is of a fixed size, so if there isn't enough space for
      a wrap-around marker, the reader can reliably detect that. The
      reader logic is therefore, is there enough space for a header?
      if no, wrap around. if yes, read it: if it's a wrap-around marker,
      wrap around; otherwise, read the paylog string (the DATA field).
    */
    if (log_sink_pfs_event_header_fits(ring_buffer_write))
      memset(ring_buffer_write, 0, sizeof(log_sink_pfs_event));

    // wrap write pointer
    ring_buffer_write = ring_buffer_start;
  }

  return ret;
}

// make sure that the event contains a sane timestamp
static inline void log_sink_pfs_sanitize_timestamp(log_sink_pfs_event *e) {
  // failsafe: if no timestamp was given, create one now.
  if (e->m_timestamp == 0) e->m_timestamp = my_micro_time();

  // make sure timestamps are unique
  if (e->m_timestamp <= log_sink_pfs_latest_timestamp)
    e->m_timestamp = ++log_sink_pfs_latest_timestamp;
  else
    log_sink_pfs_latest_timestamp = e->m_timestamp;
}

/**
  Add a log-event to the ring buffer.

  In the ring-buffer, each event exists as a header and a blob.
  The header is a log_sink_pfs_event struct containing the
  traditional error-log columns. It is followed by a variable-length
  blob that contains just the message string in traditional log mode,
  and the complete event as JSON in JSON log format.
  The length of the event will be align to the correct boundary.

  If writing the event would go past the end of the ring-buffer,
  we wrap around to the beginning of the buffer.

  After the function success, ``ring_buffer_read`` will be set to
  a valid, non-zero value.

  @param  e         filled-in event struct to copy into the ring-buffer
  @param  blob_src  variable-length part of the event to add to the ring-buffer

  @retval LOG_SERVICE_SUCCESS            event was added
  @retval LOG_SERVICE_NOT_AVAILABLE      ring-buffer not available
  @retval LOG_SERVICE_INVALID_ARGUMENT   event has no message
  @retval LOG_SERVICE_ARGUMENT_TOO_LONG  event is larger than buffer(!)
*/
log_service_error log_sink_pfs_event_add(log_sink_pfs_event *e,
                                         const char *blob_src) {
  char *blob_dst;  // address to write the blob to in the ring-buffer
  size_t s;
  log_service_error ret = LOG_SERVICE_ARGUMENT_TOO_LONG;

  // If either of these fail, the ring-buffer's not been set up (yet).
  if ((ring_buffer_start == nullptr) || (ring_buffer_write == nullptr))
    return LOG_SERVICE_NOT_AVAILABLE;

  // Have we been given an invalid event (one with no message)?
  assert(e->m_message_length > 0);

  // No-message event: Fail gracefully in production.
  if (e->m_message_length == 0) return LOG_SERVICE_INVALID_ARGUMENT;

  // How much space do we need in the ring-buffer (including alignment padding)?
  s = log_sink_pfs_event_size(e);

  // write-lock ring-buffer
  mysql_rwlock_wrlock(&THR_LOCK_log_perfschema);

  // Statistics: track longest event seen.
  if (s > log_sink_pfs_longest_event) log_sink_pfs_longest_event = s;

  // Let's not process events that are larger than the buffer.
  if (s >= ring_buffer_size) goto done;

  /*
    We've made sure the event will fit in the ring-buffer,
    but it may not fit at the current position.
    In that case,
    - if the read>write (as it will be after the first wrap), expire all
      events from read/tail until the phys-end of the buffer
    - if there is space for a wrap-marker, write one.
    - wrap the write-pointer to the start of the buffer
  */
  log_sink_pfs_write_wrap(s);

  /*
    If the write position is <= the read position,
    but writing the event would write past the read position,
    there is an overlap, and we need to expire enough old
    events to write the new one:

    We move the read-position forward towards younger events,
    thereby expiring older ones that the writer is about to
    overwrite.

    Since the blob portion of the event is of variable size,
    writing one (large) new event may require expiring several
    (smaller) old events.

    If we already expired all events above, or the buffer was
    empty to begin with, log_sink_pfs_buffered_events is 0
    here and we won't enter the loop.
  */
  while ((log_sink_pfs_buffered_events > 0) &&
         (ring_buffer_write <= ring_buffer_read) &&
         ((ring_buffer_write + s) > ring_buffer_read)) {
    /*
      Skip forward to the next oldest event until we have expired
      enough old events to make room for the new one. The "next"
      function called by "expire" automatically handles the
      wrap-around at the end of the ring-buffer.

      ``ring_buffer_read`` can become NULL here if we end up
      throwing every last event in the buffer.
    */
    log_sink_pfs_event_expire();
  }

  /*
    If the ring-buffer was empty to begin with, or if we had
    to expire all existing events to make room of the new event
    (and ended up with an empty ring-buffer that way),
    ring_buffer_read is NULL now, and we'll re-initialize the
    read-pointer to a sensible value:
    The buffer is empty, and we're about to add the (now) first
    event to it:
    a) set read-pointer = write-pointer
    b) write the new event
    c) adjust the write-pointer to after the new event
    The read-pointer now points to the beginning of the sole event,
    the write-pointer points just behind it.

    If a write wrap-around was needed, this has already happened
    above, and we're copying a post-wrap pointer, i.e. one to the
    start of the buffer.

    At start-up, this should be equivalent to setting the read-pointer
    to ring_buffer_start (because we just wrote the very first event
    to the very start of the buffer).
  */
  if (ring_buffer_read == nullptr) {
    assert(log_sink_pfs_buffered_events == 0);
    ring_buffer_read = ring_buffer_write;
  }

  // make sure that the event contains a sane timestamp
  log_sink_pfs_sanitize_timestamp(e);

  // copy the event header
  memcpy(ring_buffer_write, e, sizeof(log_sink_pfs_event));

  // append the variable length message
  blob_dst = ring_buffer_write + sizeof(log_sink_pfs_event);
  memcpy(blob_dst, blob_src, e->m_message_length);
  // terminate for safety; this is accounted for in log_sink_pfs_event_size()
  blob_dst[e->m_message_length] = '\0';

  // move the write-pointer behind the new event to the next write position
  ring_buffer_write += s;
  log_sink_pfs_buffered_events++;
  log_sink_pfs_buffered_bytes += s;

  // ensure that we leave the read-pointer in a valid state
  assert(((log_sink_pfs_event *)ring_buffer_read)->m_message_length != 0);

  ret = LOG_SERVICE_SUCCESS;

done:
  mysql_rwlock_unlock(&THR_LOCK_log_perfschema);

  return ret;
}

// Restoring the log from the file

/**
  Add all rows from a log file to the error-log ring-buffer.

  We have to guesstimate where to start reading in the log:

  - The error_log table is kept in a ring-buffer.
    Reading more items than we have space for is therefore harmless;
    we should however try to keep the waste down for performance
    reasons.

  - In the traditional log, the part of the row before the message
    is 63 characters long.  This gets converted into an event header.
    The header's size is platform-dependent, but usually shorter
    than 63 bytes. Thus, the size of each record in the input will
    be more or less the size of its corresponding record in the
    output. As a consequence, reading the ring-buffer's size from
    the input should be about right.

  - When reading the JSON log, we'll fill in the event header from
    the parsed values, but we will also attach the entire JSON record
    to the event. Each record in the ring-buffer is therefore the
    size of the original JSON record, plus the size of a record header.
    As a consequence reading the ring-buffer's size from the input
    will give us more events than we need (because we "lose" about
    50 bytes to the header for each event). However, the input is of
    variable length and we can not tell whether it's a few long rows
    or a lot of short ones. Therefore, we assume the worst (rather
    than the average) case and try to read input the size of the
    ring-buffer. This will mean that we read some more rows than we
    have space for, but since it's a ring-buffer, that means that
    the oldest entries will be discarded to make room for the younger
    ones, and we'll end up with the correct result.

  @param  log_file   The file's name
  @param  size       length of the input file (in bytes)

  @retval LOG_SERVICE_SUCCESS            success
  @retval LOG_SERVICE_OPEN_FAILED        failed to open file
  @retval LOG_SERVICE_SEEK_FAILED        seek failed
  @retval LOG_SERVICE_UNABLE_TO_READ     read failed
  @retval LOG_SERVICE_PARSE_ERROR        could not find delimiter ('\n')
*/
static log_service_error log_error_read_loop(const char *log_file,
                                             size_t size) {
  FILE *fh;
  off_t pos;
  char *chunk;
  const char *line_start, *line_end;
  log_service_error ret = LOG_SERVICE_SUCCESS;

  assert(log_sink_pfs_source != nullptr);
  assert(log_sink_pfs_source->sce != nullptr);

  if (size <= 0) return LOG_SERVICE_UNABLE_TO_READ;

  if ((fh = fopen(log_file, "r")) == nullptr) return LOG_SERVICE_OPEN_FAILED;

  if ((chunk = (char *)my_malloc(key_memory_log_sink_pfs,
                                 LOG_ERR_READ_LINE_SIZE, MYF(0))) == nullptr)
    goto fail_with_close;

  /*
    If the file would fit into the ring-buffer entirely, we'll read
    it from the beginning.

    (We don't actually read it to the ring-buffer, but it's a good
    guide-line.)

    Otherwise, we start reading from a point in the file where about
    the size of the ring-buffer remains as input.
  */
  if (size <= ring_buffer_size)
    pos = 0;
  else {
    pos = size - ring_buffer_size;

    // seek to the approximate position of the row to start reading at
    if (fseek(fh, (long)pos, SEEK_SET)) { /* purecov: begin inspected */
      ret = LOG_SERVICE_SEEK_FAILED;
      goto fail_with_free; /* purecov: end */
    }

    // we're likely in the middle of a row, skip forward to the next
    if ((line_start = fgets(chunk, LOG_ERR_READ_LINE_SIZE, fh)) == nullptr) {
      ret = LOG_SERVICE_UNABLE_TO_READ; /* purecov: begin inspected */
      goto fail_with_free;              /* purecov: end */
    }
  }

  while ((line_start = fgets(chunk, LOG_ERR_READ_LINE_SIZE, fh)) != nullptr) {
    /*
      If we did not manage to read a full line, skip to next record.
      Excessively long records are thus skipped, but do not abort the reading.
    */
    if ((line_end = (const char *)memchr(line_start, '\n',
                                         LOG_ERR_READ_LINE_SIZE)) == nullptr) {
      // Skip the rest of the overly long row, until we find a newline.
      do {
        if ((line_start = fgets(chunk, LOG_ERR_READ_LINE_SIZE, fh)) ==
            nullptr) {
          ret = LOG_SERVICE_PARSE_ERROR;
          goto fail_with_free;
        }
        line_end =
            (const char *)memchr(line_start, '\n', LOG_ERR_READ_LINE_SIZE);
      } while (line_end == nullptr);

      /*
        We now have (fragment of) a skipped line in the buffer.
        We do not try to parse it; just proceed with reading the next line.
      */
    } else {
      // get a log event from the read line and add it to the ring-buffer
      if (!(log_sink_pfs_source->sce->chistics & LOG_SERVICE_BUILTIN)) {
        SERVICE_TYPE(log_service) * ls;

        ls = reinterpret_cast<SERVICE_TYPE(log_service) *>(
            log_sink_pfs_source->sce->service);

        assert(ls != nullptr);

        if (ls != nullptr)
          ret = ls->parse_log_line(line_start, line_end - line_start);
      } else
        ret = log_sink_trad_parse_log_line(line_start, line_end - line_start);
    }
  }

fail_with_free:
  my_free(chunk);

fail_with_close:
  fclose(fh);

  return ret;
}

// The public interface to restoring the error-log to the ring-buffer:

/**
  Restore error log messages from previous shutdown.

  We try restoring from the first (leftmost) of those services
  listed in @@global.log_error_services that have the
  LOG_SERVICE_LOG_PARSER characteristic.

  It is assumed that the last run's log file name is the same
  as the current one's. That is to say, we check whether the
  file supplied to --log-error already exists.

  Once we have determined what file to read from, we'll call
  @see log_error_read_loop() to do the actual reading and parsing.

  It should be noted that at the point this function is normally
  called, buffered error logging will not have flushed yet.

  a) If we are using the built-in "trad" sink/reader, the start-up
  messages are usually not buffered, and have already been written
  to the error log. In this case, they will be restored from the
  log (and flushing won't add another event to the ring-buffer).

  b) If we are using a reader in a loadable log-service,

  @param  log_name  The log file to read (log_error_dest).

  @retval LOG_SERVICE_SUCCESS                 Success (log read and parsed)
  @retval LOG_SERVICE_UNABLE_TO_READ          Could not read/access() file
  @retval LOG_SERVICE_INVALID_ARGUMENT        Invalid file-name (no '.')
  @retval LOG_SERVICE_NOT_AVAILABLE           No log_component active that can
                                              parse log-files
  @retval LOG_SERVICE_ARGUMENT_TOO_LONG       File-name too long
  @retval LOG_SERVICE_COULD_NOT_MAKE_LOG_NAME Could not determine file extension
  @retval otherwise                           Return value from reader
*/
log_service_error log_error_read_log(const char *log_name) {
  MY_STAT stat_log;
  char path[FN_REFLEN];
  log_service_error ret;

  assert((log_name != nullptr) && (log_name[0] != '\0'));
  assert(ring_buffer_start != nullptr);

  // No log-service configured that could parse a log
  if ((log_sink_pfs_source == nullptr) ||
      !(log_sink_pfs_source->sce->chistics & LOG_SERVICE_LOG_PARSER)) {
    LogErr(INFORMATION_LEVEL, ER_NO_ERROR_LOG_PARSER_CONFIGURED);
    return LOG_SERVICE_NOT_AVAILABLE;
  }

  // If --log-err=... does not name a file, there's nothing we can do here.
  assert(log_name != nullptr);
  assert(log_name[0] != '\0');
  assert(0 != strcmp(log_name, "stderr"));

  // If we're not using the built-in (trad log) reader, fix the log file name
  if (!(log_sink_pfs_source->sce->chistics & LOG_SERVICE_BUILTIN)) {
    char ext[32];
    SERVICE_TYPE(log_service) * ls;

    ls = reinterpret_cast<SERVICE_TYPE(log_service) *>(
        log_sink_pfs_source->sce->service);

    assert(ls != nullptr);

    // try to determine file extension for this log-service
    if ((ls == nullptr) || (ls->get_log_name(nullptr, ext, sizeof(ext)) < 0))
      return LOG_SERVICE_COULD_NOT_MAKE_LOG_NAME; /* purecov: inspected */

    if (make_log_path(path, ext) != LOG_SERVICE_SUCCESS)
      return LOG_SERVICE_COULD_NOT_MAKE_LOG_NAME; /* purecov: inspected */
  } else if (strlen(log_name) >= FN_REFLEN)
    return LOG_SERVICE_ARGUMENT_TOO_LONG; /* purecov: inspected */
  else
    strcpy(path, log_name);  // trad log. use default @@log_error.

  /*
    Lock the error-logger while we're restoring the error-log so nobody
    writes to the log-file while we're reading it. That way, we won't
    have to deal with half-written lines or the file-size changing.
  */
  log_builtins_error_stack_wrlock();

  if (my_access(path, R_OK) || (my_stat(path, &stat_log, MYF(0)) == nullptr))
    ret = LOG_SERVICE_UNABLE_TO_READ; /* purecov: inspected */
  else
    ret = log_error_read_loop(path, (size_t)stat_log.st_size);

  log_builtins_error_stack_unlock();

  return ret;
}

/**
  Release error log ring-buffer.

  @retval 0 Success - buffer was released, or did not exist in the first place.
*/
int log_error_read_log_exit() {
  mysql_rwlock_wrlock(&THR_LOCK_log_perfschema);

  if (ring_buffer_start != nullptr) {
    my_free(ring_buffer_start);
  }

  log_sink_pfs_buffered_bytes = 0;
  log_sink_pfs_expired_events = 0;
  log_sink_pfs_buffered_events = 0;

  ring_buffer_write = ring_buffer_read = ring_buffer_start = ring_buffer_end =
      nullptr;

  mysql_rwlock_unlock(&THR_LOCK_log_perfschema);
  mysql_rwlock_destroy(&THR_LOCK_log_perfschema);

  return 0;
}

/**
  Set up ring-buffer for error-log.

  @retval 0    Success - buffer was allocated.
  @retval !=0  Failure - buffer was not allocated.
*/
int log_error_read_log_init() {
  assert(ring_buffer_start == nullptr);

  char *b;

  if ((b = (char *)my_malloc(key_memory_log_sink_pfs, ring_buffer_size,
                             MYF(0))) == nullptr)
    return -1; /* purecov: inspected */

  if (mysql_rwlock_init(0, &THR_LOCK_log_perfschema)) {
    my_free(b); /* purecov: inspected */
    return -1;  /* purecov: inspected */
  }

  log_sink_pfs_buffered_bytes = 0;
  log_sink_pfs_expired_events = 0;
  log_sink_pfs_buffered_events = 0;

  ring_buffer_start = b;
  ring_buffer_read = ring_buffer_start;
  ring_buffer_write = ring_buffer_start;
  ring_buffer_end = ring_buffer_start + ring_buffer_size;  // convenience

  return 0;
}

/**
  services: log sinks: logging to performance_schema ring-buffer

  Will write timestamp, label, thread-ID, and message to stderr/file.
  If you should not be able to specify a label, one will be generated
  for you from the line's priority field.

  @param           instance             instance handle
  @param           ll                   the log line to write

  @retval          int                  number of added fields, if any
*/
int log_sink_perfschema(void *instance [[maybe_unused]], log_line *ll) {
  log_sink_pfs_event e;
  memset(&e, 0, sizeof(log_sink_pfs_event));

  assert(ring_buffer_start != nullptr);

  const char *msg = nullptr;
  int c, out_fields = 0;
  size_t iso_len = 0;
  unsigned int err_code = 0;
  log_item_type item_type = LOG_ITEM_END;
  log_item_type_mask out_types = 0;
  const char *iso_timestamp = "";

  log_item *capture_buffer = log_line_get_output_buffer(ll);

  if (capture_buffer != nullptr) {
    msg = capture_buffer->data.data_string.str;
    e.m_message_length = capture_buffer->data.data_string.length;
  }

  e.m_prio = ERROR_LEVEL;

  if (ll->count > 0) {
    for (c = 0; c < ll->count; c++) {
      item_type = ll->item[c].type;

      out_fields++;

      switch (item_type) {
        case LOG_ITEM_LOG_BUFFERED:
          e.m_timestamp = (ulonglong)ll->item[c].data.data_integer;
          break;
        case LOG_ITEM_SQL_ERRCODE:
          err_code = (unsigned int)ll->item[c].data.data_integer;
          e.m_error_code_length =
              snprintf(e.m_error_code, LOG_SINK_PFS_ERROR_CODE_LENGTH,
                       "MY-%06u", err_code);
          break;
        case LOG_ITEM_LOG_PRIO:
          e.m_prio = (enum loglevel)ll->item[c].data.data_integer;
          break;
        case LOG_ITEM_LOG_MESSAGE:
          if (msg == nullptr) {
            msg = ll->item[c].data.data_string.str;
            e.m_message_length = ll->item[c].data.data_string.length;
          }
          break;
        case LOG_ITEM_SRV_SUBSYS:
          if ((e.m_subsys_length = ll->item[c].data.data_string.length) >=
              LOG_SINK_PFS_SUBSYS_LENGTH)
            e.m_subsys_length = LOG_SINK_PFS_SUBSYS_LENGTH - 1;
          strncpy(e.m_subsys, ll->item[c].data.data_string.str,
                  e.m_subsys_length);
          e.m_subsys[e.m_subsys_length] = '\0';
          break;
        case LOG_ITEM_LOG_TIMESTAMP:
          iso_timestamp = ll->item[c].data.data_string.str;
          iso_len = ll->item[c].data.data_string.length;
          e.m_timestamp =
              iso8601_timestamp_to_microseconds(iso_timestamp, iso_len);
          break;
        case LOG_ITEM_SRV_THREAD:
          e.m_thread_id = (my_thread_id)ll->item[c].data.data_integer;
          break;
        default:
          out_fields--;
      }
      out_types |= item_type;
    }

    // failsafe: guard against no or zero-length message
    if (!(out_types & LOG_ITEM_LOG_MESSAGE) || (msg == nullptr) ||
        (e.m_message_length == 0)) {
      msg =
          "No error message, or error message of non-string type. "
          "This is almost certainly a bug!";
      e.m_message_length = strlen(msg);

      e.m_prio = ERROR_LEVEL;              // force severity
      out_types &= ~(LOG_ITEM_LOG_LABEL);  // regenerate label
      out_types |= LOG_ITEM_LOG_MESSAGE;   // we added a message
    }

    if (log_sink_pfs_event_add(&e, msg) != LOG_SERVICE_SUCCESS) return 0;

    return out_fields;  // returning number of processed items
  }

  // no item in log-line
  return 0; /* purecov: inspected */
}

/**
  Add a log-event to the ring buffer.

  We require the various pieces of information to be passed individually
  rather than accepting a log_sink_pfs_event so we can sanity check each
  part individually and don't have to worry about different components
  using different versions/sizes of the struct.

  We copy the data as needed, so caller may free their copy once this
  call returns.

  @param timestamp          Timestamp (in microseconds), or
                            0 to have one generated
  @param thread_id          thread_id of the thread that detected
                            the issue
  @param prio               (INFORMATION|WARNING|ERROR|SYSTEM)_LEVEL
  @param error_code         "MY-123456"-style error-code, or nullptr
  @param error_code_length  length in bytes of error_code
  @param subsys             Subsystem ("InnoDB", "Server", "Repl"), or nullptr
  @param subsys_length      length in bytes of subsys
  @param message            data (error message/JSON record/...). required.
  @param message_length     length of data field

  @retval LOG_SERVICE_SUCCESS                success
  @retval LOG_SERVICE_ARGUMENT_TOO_LONG      argument too long
  @retval LOG_SERVICE_INVALID_ARGUMENT       invalid argument
*/
DEFINE_METHOD(log_service_error, log_sink_perfschema_imp::event_add,
              (ulonglong timestamp, ulonglong thread_id, ulong prio,
               const char *error_code, uint error_code_length,
               const char *subsys, uint subsys_length, const char *message,
               uint message_length)) {
  const log_service_error ret = LOG_SERVICE_ARGUMENT_TOO_LONG;

  log_sink_pfs_event e;
  memset(&e, 0, sizeof(log_sink_pfs_event));

  // prio
  if (prio > INFORMATION_LEVEL) return LOG_SERVICE_INVALID_ARGUMENT;
  e.m_prio = prio;

  // thread_id
  e.m_thread_id = thread_id;

  // message (mandatory. ring-buffer doesn't have LOG_BUFF_MAX limitation.)
  if ((message == nullptr) || (message_length == 0))
    return LOG_SERVICE_INVALID_ARGUMENT;
  e.m_message_length = message_length;

  // subsys
  if ((subsys != nullptr) && (subsys_length < LOG_SINK_PFS_SUBSYS_LENGTH)) {
    memcpy(e.m_subsys, subsys, subsys_length);
    e.m_subsys_length = subsys_length;
    e.m_subsys[subsys_length] = '\0';
  } else
    return ret;

  // error-code
  if ((error_code != nullptr) &&
      (error_code_length < LOG_SINK_PFS_ERROR_CODE_LENGTH)) {
    memcpy(e.m_error_code, error_code, error_code_length);
    e.m_error_code_length = error_code_length;
    e.m_error_code[error_code_length] = '\0';
  } else
    return ret;

  /*
    The add-function below will provide a current timestamp
    if 0 was given, and make sure the values are strictly
    increasing, so we're not sanity testing here.
  */
  e.m_timestamp = timestamp;

  // This function will deep-copy the data as needed.
  return log_sink_pfs_event_add(&e, message);
}
