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

#include "log_sink_trad.h"
#include "log_sink_perfschema.h"  // log_sink_pfs_event
#include "my_systime.h"           // my_micro_time()
#include "sql/log.h"  // log_write_errstream(), log_prio_from_label()

extern int log_item_inconsistent(log_item *li);

/**
  @file log_sink_trad.cc

  The built-in log-sink (i.e. the writer for the traditional
  MySQL error log):

  a) writing of an error log event to the traditional error log file
  b) parsing of a line from the traditional error log file
*/

/**
  Find the end of the current field (' ')

  @param  parse_from  start of the token
  @param  token_end   where to store the address of the delimiter found
  @param  buf_end     end of the input line

  @retval -1   delimiter not found, "parsing" failed
  @retval >=0  length of token
*/
ssize_t parse_trad_field(const char *parse_from, const char **token_end,
                         const char *buf_end) {
  assert(token_end != nullptr);
  *token_end = (const char *)memchr(parse_from, ' ', buf_end - parse_from);
  return (*token_end == nullptr) ? -1 : (*token_end - parse_from);
}

/**
  Parse a single line in the traditional error log

  @param line_start   pointer to the beginning of the line ('2' of the ISO-date)
  @param line_length  length of the line

  @retval LOG_SERVICE_ARGUMENT_TOO_LONG  Token too long for its field
  @retval LOG_SERVICE_PARSE_ERROR        No more spaces in line, cannot find
                                         expected end of token, or input
                                         otherwise malformed
  @retval LOG_SERVICE_SUCCESS            Event added to ring-buffer
*/
log_service_error log_sink_trad_parse_log_line(const char *line_start,
                                               size_t line_length) {
  char timestamp[iso8601_size];  // space for timestamp + '\0'
  char label[16];
  char msg[LOG_BUFF_MAX];
  ssize_t len;
  const char *line_end = line_start + line_length;
  const char *start = line_start, *end = line_end;

  log_sink_pfs_event e;
  memset(&e, 0, sizeof(log_sink_pfs_event));

  // sanity check: must start with timestamp
  if (*line_start != '2') return LOG_SERVICE_PARSE_ERROR;

  // parse timestamp
  if ((len = parse_trad_field(start, &end, line_end)) <= 0)
    return LOG_SERVICE_PARSE_ERROR;
  if (len >= iso8601_size) return LOG_SERVICE_ARGUMENT_TOO_LONG;

  memcpy(timestamp, start, len);
  timestamp[len] = '\0';  // terminate target buffer
  start = end + 1;
  e.m_timestamp = iso8601_timestamp_to_microseconds(timestamp, len);

  // thread_id
  e.m_thread_id = atoi(start);
  if ((len = parse_trad_field(start, &end, line_end)) <= 0)
    return LOG_SERVICE_PARSE_ERROR;
  start = end + 1;

  // parse prio/label
  if ((len = parse_trad_field(start, &end, line_end)) <= 0)
    return LOG_SERVICE_PARSE_ERROR;
  if ((len > ((ssize_t)sizeof(label))) || (len < 3))
    return LOG_SERVICE_ARGUMENT_TOO_LONG;
  len -= 2;  // We won't copy [ ]
  memcpy(label, start + 1, len);
  label[len] = '\0';
  start = end + 1;
  e.m_prio = log_prio_from_label(label);

  // parse err_code
  if ((len = parse_trad_field(start, &end, line_end)) <= 0)
    return LOG_SERVICE_PARSE_ERROR;
  if ((len < 4) || (0 != strncmp(start, "[MY-", 4)))
    return LOG_SERVICE_PARSE_ERROR;
  len -= 2;  // We won't copy [ ]
  if (len >= LOG_SINK_PFS_ERROR_CODE_LENGTH)
    return LOG_SERVICE_ARGUMENT_TOO_LONG;
  strncpy(e.m_error_code, ++start, len);
  e.m_error_code[len] = '\0';
  e.m_error_code_length = len;  // Should always be 3+6
  start = end + 1;

  // parse subsys
  if ((len = parse_trad_field(start, &end, line_end)) <= 0)
    return LOG_SERVICE_PARSE_ERROR;
  len -= 2;  // We won't copy [ ]
  if (len >= LOG_SINK_PFS_SUBSYS_LENGTH) return LOG_SERVICE_ARGUMENT_TOO_LONG;
  memcpy(e.m_subsys, start + 1, len);
  e.m_subsys_length = len;
  e.m_subsys[len] = '\0';
  start = end + 1;

  // parse message - truncate if needed.
  len = line_end - start;

  /*
    If we have a message for this, it becomes more easily searchable.
    This is provided in the hope that between error code (which it appears
    we have) and subsystem (which it appears we also have), a human reader
    can find out what happened here even if the log file is not available
    to them. If the log file IS available, they should be able to just find
    this event's time stamp in that file and see whether the line contains
    anything that would break parsing.
  */
  const char *parsing_failed =
      "No message found for this event while parsing a traditional error log! "
      "If you wish to investigate this, use this event's timestamp to find the "
      "offending line in the error log file.";
  if (len <= 0) {
    start = parsing_failed;
    len = strlen(parsing_failed);
  }

  // Truncate length if needed.
  if (len >= ((ssize_t)sizeof(msg))) {
    len = sizeof(msg) - 1;
  }

  // Copy as much of the message as we have space for.
  strncpy(msg, start, len);
  msg[len] = '\0';

  /*
    Store adjusted length in log-event.
    m_message_length is a uint while len is ssize_t, but we capped at
    sizeof(msg) above which is less than either, so we won't
    assert(len <= UINT_MAX) here.
    log_sink_pfs_event_add() below will assert() if m_message_length==0,
    but this should be prevented by us setting a fixed message above if
    parsed resulting in an empty message field. (If parsing any of the
    other fields failed, we won't try to add a message to the
    performance-schema table in the first place.)
  */
  e.m_message_length = len;

  // Add event to ring-buffer.
  return log_sink_pfs_event_add(&e, msg);
}

/**
  services: log sinks: basic logging ("classic error-log")
  Will write timestamp, label, thread-ID, and message to stderr/file.
  If you should not be able to specify a label, one will be generated
  for you from the line's priority field.

  @param           instance             instance handle
  @param           ll                   the log line to write

  @retval          int                  number of added fields, if any
*/
int log_sink_trad(void *instance [[maybe_unused]], log_line *ll) {
  const char *label = "", *msg = "";
  int c, out_fields = 0;
  size_t msg_len = 0, iso_len = 0, label_len = 0, subsys_len = 0;
  enum loglevel prio = ERROR_LEVEL;
  unsigned int errcode = 0;
  log_item_type item_type = LOG_ITEM_END;
  log_item_type_mask out_types = 0;
  const char *iso_timestamp = "", *subsys = "";
  my_thread_id thread_id = 0;
  char *line_buffer = nullptr;

  if (ll->count > 0) {
    for (c = 0; c < ll->count; c++) {
      item_type = ll->item[c].type;

      if (log_item_inconsistent(&ll->item[c])) continue;

      out_fields++;

      switch (item_type) {
        case LOG_ITEM_SQL_ERRCODE:
          errcode = (unsigned int)ll->item[c].data.data_integer;
          break;
        case LOG_ITEM_LOG_PRIO:
          prio = (enum loglevel)ll->item[c].data.data_integer;
          break;
        case LOG_ITEM_LOG_MESSAGE: {
          const char *nl;
          msg = ll->item[c].data.data_string.str;
          msg_len = ll->item[c].data.data_string.length;

          /*
            If the message contains a newline, copy the message and
            replace the newline so we may print a valid log line,
            i.e. one that doesn't have a line-break in the middle
            of its message.
          */
          if ((nl = (const char *)memchr(msg, '\n', msg_len)) != nullptr) {
            assert(line_buffer == nullptr);

            if (line_buffer != nullptr) my_free(line_buffer);

            if ((line_buffer = (char *)my_malloc(
                     PSI_NOT_INSTRUMENTED, msg_len + 1, MYF(0))) == nullptr) {
              msg =
                  "The submitted error message contains a newline, "
                  "and a buffer to sanitize it for the traditional "
                  "log could not be allocated. File a bug against "
                  "the message corresponding to this MY-... code.";
              msg_len = strlen(msg);
            } else {
              memcpy(line_buffer, msg, msg_len);
              line_buffer[msg_len] = '\0';
              char *nl2 = line_buffer;
              while ((nl2 = strchr(nl2, '\n')) != nullptr) *(nl2++) = ' ';
              msg = line_buffer;
            }
          }
        } break;
        case LOG_ITEM_LOG_LABEL:
          label = ll->item[c].data.data_string.str;
          label_len = ll->item[c].data.data_string.length;
          break;
        case LOG_ITEM_SRV_SUBSYS:
          subsys = ll->item[c].data.data_string.str;
          if ((subsys_len = ll->item[c].data.data_string.length) > 12)
            subsys_len = 12;
          break;
        case LOG_ITEM_LOG_TIMESTAMP:
          iso_timestamp = ll->item[c].data.data_string.str;
          iso_len = ll->item[c].data.data_string.length;
          break;
        case LOG_ITEM_SRV_THREAD:
          thread_id = (my_thread_id)ll->item[c].data.data_integer;
          break;
        default:
          out_fields--;
      }
      out_types |= item_type;
    }

    if (!(out_types & LOG_ITEM_LOG_MESSAGE)) {
      msg =
          "No error message, or error message of non-string type. "
          "This is almost certainly a bug!";
      msg_len = strlen(msg);

      prio = ERROR_LEVEL;                  // force severity
      out_types &= ~(LOG_ITEM_LOG_LABEL);  // regenerate label
      out_types |= LOG_ITEM_LOG_MESSAGE;   // we added a message

      out_fields = LOG_SERVICE_INVALID_ARGUMENT;
    }

    {
      char internal_buff[LOG_BUFF_MAX];
      size_t buff_size = sizeof(internal_buff);
      char *buff_line = internal_buff;
      size_t len;

      if (!(out_types & LOG_ITEM_LOG_LABEL)) {
        label = (prio == ERROR_LEVEL) ? "ERROR" : log_label_from_prio(prio);
        label_len = strlen(label);
      }

      if (!(out_types & LOG_ITEM_LOG_TIMESTAMP)) {
        char buff_local_time[iso8601_size];

        make_iso8601_timestamp(buff_local_time, my_micro_time(),
                               iso8601_sysvar_logtimestamps);
        iso_timestamp = buff_local_time;
        iso_len = strlen(buff_local_time);
      }

      /*
        WL#11009 adds "error identifier" as a field in square brackets
        that directly precedes the error message. As a result, new
        tools can check for the presence of this field by testing
        whether the first character of the presumed message field is '['.
        Older tools will just consider this identifier part of the
        message; this should therefore not affect log aggregation.
        Tools reacting to the contents of the message may wish to
        use the new field instead as it's simpler to parse.
        The rules are like so:

          '[' [ <namespace> ':' ] <identifier> ']'

        That is, an error identifier may be namespaced by a
        subsystem/component name and a ':'; the identifier
        itself should be considered opaque; in particular, it
        may be non-numerical: [ <alpha> | <digit> | '_' | '.' | '-' ]
      */
      len =
          snprintf(buff_line, buff_size, "%.*s %u [%.*s] [MY-%06u] [%.*s] %.*s",
                   (int)iso_len, iso_timestamp, thread_id, (int)label_len,
                   label, errcode, (int)subsys_len, subsys, (int)msg_len, msg);

      // We return only the message, not the whole line, so memcpy() is needed.
      log_item *output_buffer = log_line_get_output_buffer(ll);

      if (output_buffer != nullptr) {
        if (msg_len < output_buffer->data.data_buffer.length)
          output_buffer->data.data_buffer.length = msg_len;
        else  // truncate message to buffer-size (and leave space for '\0')
          msg_len = output_buffer->data.data_buffer.length - 1;

        memcpy((char *)output_buffer->data.data_buffer.str, msg, msg_len);
        output_buffer->data.data_buffer.str[msg_len] = '\0';

        output_buffer->type = LOG_ITEM_RET_BUFFER;
      }

      // write log-event to log-file
      log_write_errstream(buff_line, len);
    }
  }

  if (line_buffer != nullptr) my_free(line_buffer);

  return out_fields;  // returning number of processed items
}
