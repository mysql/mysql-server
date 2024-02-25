/* Copyright (c) 2020, 2023, Oracle and/or its affiliates.

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
  @file log_sink_buffer.cc

  This file contains

  a) the log-sink that buffers errors logged during start-up so they
  can be flushed once all configured log-components have become available;

  b) the functions for querying and setting the phase the server is in
  with regard to logging (buffering, normal operation, and so forth);

  c) the functions for flushing the buffered information (to force writing
  out this information in cases of early shutdowns and so on).
*/

#include "log_sink_buffer.h"
#include <mysql/components/services/log_shared.h>  // data types
#include "log_builtins_filter_imp.h"
#include "log_sink_trad.h"
#include "my_systime.h"  // my_micro_time()
#include "mysql/psi/mysql_mutex.h"
#include "sql/log.h"
#include "sql/psi_memory_key.h"  // key_memory_log_error_stack
#include "sql/server_component/log_builtins_internal.h"

/**
  Make sure only one instance of the buffered "writer" runs at a time.

  In normal operation, the log-event will be created dynamically, then
  it will be fed through the pipeline, and then it will be released.
  Since the event is allocated in the caller, we can be sure it won't
  go away wholesale during processing, and since the event is local to
  the caller, no other thread will tangle with it. It is therefore safe
  in those cases not to wrap a lock around the event.
  (The log-pipeline will still grab a shared lock, THR_LOCK_log_stack,
  to protect the pipeline (not the event) and the log-services cache from
  being changed while the pipeline is being applied.
  Likewise, log-services may protect their resources (file-writers will
  usually take a lock to serialize their writes; the built-in filter will
  take a lock on its rule-set as that is shared between concurrent
  threads running the filter, and so on).
  None of these are intended to protect the event itself though.

  In buffered mode on the other hand, we copy each log-event (the
  original of which, see above, is owned by the caller and local
  to the thread, and therefore safe without locking) to a global
  buffer / backlog. As this backlog can be added to by all threads,
  it must be protected by a lock (once we have fully initialized
  the subsystem with log_builtins_init() and support multi-threaded
  mode anyway, as indicated by log_builtins_started being non-zero,
  see below). This is that lock.
*/
mysql_mutex_t THR_LOCK_log_buffered;

/// Pointer to the first element in the list of buffered log messages
static log_line_buffer *log_line_buffer_start = nullptr;
/// Where to write the pointer to the newly-created tail-element of the list
static log_line_buffer **log_line_buffer_tail = &log_line_buffer_start;

/**
  Timestamp of the last event we put into the error-log buffer
  during buffered mode (while starting up). New items must
  receive a LOG_ITEM_LOG_BUFFERED timestamp greater than this.
*/
static ulonglong log_sink_buffer_latest_buffered = 0;

/**
  Duplicate a log-event. This is a deep copy where the items (key/value pairs)
  have their own allocated memory separate from that in the source item.

  @param   dst    log_line that will hold the copy
  @param   src    log_line we copy from

  @retval  false  on success
  @retval  true   if out of memory
*/
static bool log_line_duplicate(log_line *dst, log_line *src) {
  int c;

  assert((dst != nullptr) && (src != nullptr));

  *dst = *src;

  for (c = 0; c < src->count; c++) {
    dst->item[c].alloc = LOG_ITEM_FREE_NONE;

    if ((dst->item[c].key =
             my_strndup(key_memory_log_error_loaded_services, src->item[c].key,
                        strlen(src->item[c].key), MYF(0))) != nullptr) {
      // We just allocated a key, remember to free it later:
      dst->item[c].alloc = LOG_ITEM_FREE_KEY;

      // If the value is a string, duplicate it, and remember to free it later!
      if (log_item_string_class(src->item[c].item_class) &&
          (src->item[c].data.data_string.str != nullptr)) {
        if ((dst->item[c].data.data_string.str = my_strndup(
                 key_memory_log_error_loaded_services,
                 src->item[c].data.data_string.str,
                 src->item[c].data.data_string.length, MYF(0))) != nullptr)
          dst->item[c].alloc |= LOG_ITEM_FREE_VALUE;
        else
          goto fail; /* purecov: inspected */
      }
    } else
      goto fail; /* purecov: inspected */
  }

  return false;

fail:                           /* purecov: begin inspected */
  dst->count = c + 1;           // consider only the items we actually processed
  log_line_item_free_all(dst);  // free those items
  return true;                  // flag failure
  /* purecov: end */
}

/**
  services: log sinks: buffered logging

  During start-up, we buffer log-info until a) we have basic info for
  the built-in logger (what file to log to, verbosity, and so on), and
  b) advanced info (any logging components to load, any configuration
  for them, etc.).

  As a failsafe, if start-up takes very, very long, and a time-out is
  reached before reaching b) and we actually have something worth
  reporting (e.g. errors, as opposed to info), we try to keep the user
  informed by using the basic logger configured in a), while going on
  buffering all info and flushing it to any advanced loggers when b)
  is reached.

  1) This function checks and, if needed, updates the time-out, and calls
     the flush functions as needed. It is internal to the logger and should
     not be called from elsewhere.

  2) Function will save log-event (if given) for later filtering and output.

  3) Function acquires/releases THR_LOCK_log_buffered if initialized.

  @param           instance             instance handle
                                        Not currently used in this writer;
                                        if this changes later, keep in mind
                                        that nullptr will be passed if this
                                        is called before the structured
                                        logger's locks are initialized, so
                                        that must remain a valid argument!
  @param           ll                   The log line to write,
                                        or nullptr to not add a new logline,
                                        but to just check whether the time-out
                                        has been reached and if so, flush
                                        as needed.

  @retval          -1                   can not add event to buffer (OOM?)
  @retval          >0                   number of added fields
*/
int log_sink_buffer(void *instance [[maybe_unused]], log_line *ll) {
  log_line_buffer *llb = nullptr;  ///< log-line buffer
  ulonglong now = 0;
  int count = 0;

  /*
    If we were actually given an event, add it to the buffer.
  */
  if (ll != nullptr) {
    if ((llb = (log_line_buffer *)my_malloc(key_memory_log_error_stack,
                                            sizeof(log_line_buffer), MYF(0))) ==
        nullptr)
      return -1; /* purecov: inspected */

    llb->next = nullptr;

    /*
      Don't let the submitter free the keys/values; we'll do it later when
      the buffer is flushed and then de-allocated!
      (No lock needed for copy as the target-event is still private to this
      function, and the source-event is alloc'd in the caller so will be
      there at least until we return.)
    */
    log_line_duplicate(&llb->ll, ll);
  }

  /*
    Insert the new last event into the buffer
    (a singly linked list of events).
  */
  if (log_builtins_started()) mysql_mutex_lock(&THR_LOCK_log_buffered);

  now = my_micro_time();

  if (ll != nullptr) {
    /*
      Prevent two events from receiving the exact same timestamp on
      systems with low resolution clocks.
    */
    if (now > log_sink_buffer_latest_buffered)
      log_sink_buffer_latest_buffered = now;
    else
      log_sink_buffer_latest_buffered++;

    /*
      Save the current time so we can regenerate the textual timestamp
      later when we have the command-line options telling us what format
      it should be in (e.g. UTC or system time).
    */
    if (!log_line_full(&llb->ll)) {
      log_line_item_set(&llb->ll, LOG_ITEM_LOG_BUFFERED)->data_integer =
          log_sink_buffer_latest_buffered;
    }

    *log_line_buffer_tail = llb;
    log_line_buffer_tail = &(llb->next);

    /*
      Save the element-count now as the time-out flush below may release
      the underlying log line buffer, llb, making that info inaccessible.
    */
    count = llb->ll.count;
  }

  if (log_builtins_started()) mysql_mutex_unlock(&THR_LOCK_log_buffered);

  return count;
}

/*
  Release all buffered log-events (discard_error_log_messages()),
  optionally after running them through the error log stack first
  (flush_error_log_messages()). Safe to call repeatedly (though
  subsequent calls will only output anything if further events
  occurred after the previous flush).
*/
void log_sink_buffer_flush(enum log_sink_buffer_flush_mode mode) {
  log_line_buffer *llp, *local_head, *local_tail = nullptr;

  /*
    "steal" public list of buffered log events

    The general mechanism is that we move the buffered events from
    the global list to one local to this function, iterate over
    it, and then put it back. If anything was added to the global
    list we emptied while were working, we append the new items
    from the global list to the end of the "stolen" list, and then
    make that union the new global list. The grand idea here is
    that this way, we only have to acquire a lock very briefly
    (while updating the global list's head and tail, once for
    "stealing" it, once for giving it back), rather than holding
    a lock the entire time, or locking each event individually,
    while still remaining safe if one caller starts a
    flush-with-print, and then another runs a flush-to-delete
    that might catch up and cause trouble if we neither held
    a lock nor stole the list.
  */

  /*
    If the lock hasn't been init'd yet, don't get it.
  */
  if (log_builtins_started()) mysql_mutex_lock(&THR_LOCK_log_buffered);

  local_head = log_line_buffer_start;             // save list head
  log_line_buffer_start = nullptr;                // empty public list
  log_line_buffer_tail = &log_line_buffer_start;  // adjust tail of public list

  if (log_builtins_started()) mysql_mutex_unlock(&THR_LOCK_log_buffered);

  // get head element from list of buffered log events
  llp = local_head;

  while (llp != nullptr) {
    /*
      Forward the buffered lines to log-writers
      (other than the buffered writer), unless
      we're in "discard" mode, in which case,
      we'll just throw the information away.
    */
    if (mode != LOG_BUFFER_DISCARD_ONLY) {
      // Fetch integer timestamp of when we buffered the event.
      ulonglong now = 0;
      int index_buff = log_line_index_by_type(&llp->ll, LOG_ITEM_LOG_BUFFERED);

      if (index_buff >= 0) now = llp->ll.item[index_buff].data.data_integer;

      // We failed to set a timestamp earlier (OOM?). Use current time!
      if (now == 0) now = my_micro_time(); /* purecov: inspected */

      DBUG_EXECUTE_IF("log_error_normalize", { now = 0; });

      // Regenerate timestamp with the correct options.
      char local_time_buff[iso8601_size];
      make_iso8601_timestamp(local_time_buff, now,
                             iso8601_sysvar_logtimestamps);
      char *date = my_strndup(key_memory_log_error_stack, local_time_buff,
                              strlen(local_time_buff) + 1, MYF(0));

      if (date != nullptr) {
        int index_time =
            log_line_index_by_type(&llp->ll, LOG_ITEM_LOG_TIMESTAMP);

        if (index_time >= 0) {
          // release old timestamp value
          if (llp->ll.item[index_time].alloc & LOG_ITEM_FREE_VALUE) {
            my_free(const_cast<char *>(
                llp->ll.item[index_time].data.data_string.str));
          }
          // set new timestamp value
          llp->ll.item[index_time].data.data_string.str = date;
          llp->ll.item[index_time].data.data_string.length = strlen(date);
          llp->ll.item[index_time].alloc |= LOG_ITEM_FREE_VALUE;
        } else if (!log_line_full(&llp->ll)) { /* purecov: begin inspected */
          // set all new timestamp key/value pair; we didn't previously have one
          // This shouldn't happen unless we run out of space during submit()!
          log_item_data *d =
              log_line_item_set(&llp->ll, LOG_ITEM_LOG_TIMESTAMP);
          if (d != nullptr) {
            d->data_string.str = date;
            d->data_string.length = strlen(d->data_string.str);
            llp->ll.item[llp->ll.count - 1].alloc |= LOG_ITEM_FREE_VALUE;
          } else
            my_free(date);  // couldn't create key/value pair for timestamp
        } else
          my_free(date);  // log_line is full
      }                   /* purecov: end */

      /*
        If no log-service is configured (because log_builtins_init()
        hasn't run and initialized the log-pipeline), or if the
        log-service is the buffered writer (which is only available
        internally and used during start-up), it's still early in the
        start-up sequence, so we may not have all configured external
        log-components yet. Therefore, and since this is a fallback
        only, we ignore the configuration and use the built-in services
        that we know are available, on the grounds that when something
        goes wrong, information with undesired formatting is still
        better than not knowing about the issue at all.
      */
      if ((log_error_stage_get() != LOG_ERROR_STAGE_COMPONENTS) &&
          (log_error_stage_get() != LOG_ERROR_STAGE_COMPONENTS_AND_PFS)) {
        /*
          This is a fallback used when the user's log_error_services
          has not been made active yet (and loadable logging-components
          have not been loaded yet).

          In fallback modes, we run with default settings and
          services.
        */

        // LOG_BUFFER_PROCESS_AND_DISCARD

        /*
          This is a fallback used primarily when start-up is aborted.

          If we get here, flush_error_log_messages() was called
          before logging came out of buffered mode. (If it was
          called after buffered modes completes, we should land
          in the following branch instead.)

          We're asked to print all log-info so far using basic
          logging, and to then throw it away (rather than keep
          it around for proper logging). This usually implies
          that we're shutting down because some unrecoverable
          situation has arisen during start-up, so a) the user
          needs to know about it even if full logging (as
          configured) is not available yet, and b) we'll shut
          down before we'll ever get full logging, so keeping
          the info around is pointless.
        */

        if (log_filter_builtin_rules != nullptr)
          log_builtins_filter_run(log_filter_builtin_rules, &llp->ll);

        log_sink_trad(nullptr, &llp->ll);
      } else {  // no longer buffering
        /*
          If we get here, logging has left the buffered phase, and
          we can write out the log-events using the configuration
          requested by the user, as it should be!
        */
        log_line_submit(&llp->ll);  // frees keys + values (but not llp itself)
        goto kv_freed;              // skip freeing of keys + values
      }
    }

    log_line_item_free_all(&local_head->ll);  // free key/value pairs
  kv_freed:
    local_head = local_head->next;  // delist event
    my_free(llp);                   // free buffered event
    llp = local_head;
  }  // while (any_more_buffered_events_to_process)

  if (log_builtins_started()) mysql_mutex_lock(&THR_LOCK_log_buffered);

  /*
    At this point, if (local_head == nullptr), we either didn't have
    a list to begin with, or we just emptied the local version.
    Since we also emptied the global version at the top, whatever's
    in there now (still empty, or new events attached while we were
    processing) is now authoritative, and no change is needed here.

    If local_head is non-NULL, we started with a non-empty local list
    and mode was KEEP. In that case, we merge the local list back into
    the global one:
  */

  if (local_head != nullptr) {
    /*
      Since local_head was non-NULL, we started with a non-empty
      (local) list. Therefore, local_tail should point at the last
      element in that list (as it is only allowed to be NULL if
      the list is empty).
    */
    assert(local_tail != nullptr);

    /*
      Append global list to end of local list. If global list is still
      empty, the global tail pointer points at the global anchor, so
      we set the global tail pointer to the next-pointer in last stolen
      item. If global list was non-empty, the tail pointer already
      points at the correct element's next-pointer (the correct element
      being the last one in the global list, as we'll append the
      global list to the end of the local list).
    */
    if ((local_tail->next = log_line_buffer_start) == nullptr)
      log_line_buffer_tail = &local_tail->next;

    /*
      Return the stolen list, with any log-events that happened while
      we were processing appended to its end.
    */
    log_line_buffer_start = local_head;
  }

  if (log_builtins_started()) mysql_mutex_unlock(&THR_LOCK_log_buffered);
}

/**
  Prepend a list of log-events to the already buffered events.

  @param[in]  head  Head of the list to prepend to the main list
  @param[out] tail  Pointer to the `next` pointer in last element to prepend
*/
void log_sink_buffer_prepend_list(log_line_buffer *head,
                                  log_line_buffer **tail) {
  if ((head == nullptr) || (tail == nullptr)) return;

  mysql_mutex_lock(&THR_LOCK_log_buffered);

  *tail = log_line_buffer_start;
  log_line_buffer_start = head;

  mysql_mutex_unlock(&THR_LOCK_log_buffered);
}
