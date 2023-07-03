/* Copyright (c) 2017, 2023, Oracle and/or its affiliates.

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
  @brief

  This is a "modern" log writer, i.e. it doesn't care what type a
  log_item on an error log event is; as long as it's one of the
  wellknown classes (string, float, int), it can and will write it.

  By default, each line will contain one log event, in a format
  somewhat similar to that emitted by "journalctl -o json" on
  platforms that use systemd's journal:

{ "prio" : 2, "err_code" : 3581, "msg" : "Parser saw: SET
@@global.log_error_filter_rules= DEFAULT", "err_symbol" : "ER_PARSER_TRACE",
"label" : "Note" } { "prio" : 2, "err_code" : 3581, "subsystem" : "parser",
"SQL_state" : "XX999", "source_file" : "sql_parse", "function" :
"dispatch_command", "msg" : "Parser saw: SELECT \"logging as traditional MySQL
error log and as JSON\"", "time" : "1970-01-01T00:00:00.000000Z", "thread" : 0,
"err_symbol" : "ER_PARSER_TRACE", "label" : "Note" }
*/

// define to support logging to performance_schema.error_log
#define WITH_PFS_SUPPORT

// define to support reading log files (from previous runs)
#define WITH_LOG_PARSER

#include <mysql/components/services/log_builtins.h>
#include <mysql/components/services/log_shared.h>
#include <mysql/components/services/log_sink_perfschema.h>
#include <string>
#include "log_service_imp.h"
#include "my_compiler.h"

#ifdef WITH_LOG_PARSER
#include "my_rapidjson_size_t.h"

#include <rapidjson/document.h>
#endif

REQUIRES_SERVICE_PLACEHOLDER(log_builtins);
REQUIRES_SERVICE_PLACEHOLDER(log_builtins_string);
REQUIRES_SERVICE_PLACEHOLDER(log_sink_perfschema);

SERVICE_TYPE(log_builtins) *log_bi = nullptr;
SERVICE_TYPE(log_builtins_string) *log_bs = nullptr;
#ifdef WITH_LOG_PARSER
SERVICE_TYPE(log_sink_perfschema) *log_ps = nullptr;
#endif

/// Log-file extension
#define LOG_EXT ".json"

static bool inited = false;
static int opened = 0;

/**
  Pretty-print (with indents and line-breaks between key/value pairs),
  or one event per line? jq for example doesn't mind, but we play it
  safe by defaulting to the latter; this is also the format that
  "journalctl -o json" renders on platforms that use systemd's journal.
*/
enum enum_log_json_pretty_print {
  JSON_NOSPACE = 0,   ///< emit no whitespace padding
  JSON_PAD = 1,       ///< similar to systemd's journal
  JSON_MULTILINE = 2  ///< multi-line pretty-print
};
static enum_log_json_pretty_print pretty = JSON_PAD;

// This is private and specific to the component, and opaque to the server.
struct my_state {
  int id;           ///< stream-id
  void *errstream;  ///< pointer to errstream in the server
  char *ext;        ///< file extension of a given error stream
};

#ifdef WITH_LOG_PARSER

#define MY_RAPID_INT(key, val, dflt)                         \
  ulonglong val = dflt;                                      \
  iter = document.FindMember(key);                           \
  if ((iter != document.MemberEnd()) && iter->value.IsInt()) \
    val = iter->value.GetInt();

#define MY_RAPID_STR(key, val, len)                               \
  const char *val = nullptr;                                      \
  size_t len = 0;                                                 \
  iter = document.FindMember(key);                                \
  if ((iter != document.MemberEnd()) && iter->value.IsString()) { \
    val = iter->value.GetString();                                \
    len = iter->value.GetStringLength();                          \
  }

/**
  Parse a single line in an error log of this format.

  @param line_start   pointer to the beginning of the line ('{')
  @param line_length  length of the line

  @retval  0   Success
  @retval !=0  Failure (out of memory, malformed argument, etc.)
*/
DEFINE_METHOD(log_service_error, log_service_imp::parse_log_line,
              (const char *line_start, size_t line_length)) {
  using namespace rapidjson;

  Document document;
  document.Parse(line_start, line_length);
  if (!document.IsObject()) return LOG_SERVICE_PARSE_ERROR;

  Value::ConstMemberIterator iter;
  MY_RAPID_STR("time", j_time, j_time_len);
  MY_RAPID_INT("thread", j_thread_id, 0);
  MY_RAPID_INT("prio", j_prio, ERROR_LEVEL);
  MY_RAPID_STR("err_symbol", j_sym, j_sym_len);
  MY_RAPID_INT("err_code", j_code, 0);
  MY_RAPID_STR("subsystem", j_subsys, j_subsys_len);

  // If err_code is not present, fall back on err_symbol
  char err_code_buffer[32];
  const char *err_code_ptr = nullptr;
  size_t err_code_length = 0;
  int err_code_num = -1;

  if (j_code > 0)
    err_code_num = j_code;
  else if (j_sym != nullptr) {
    std::string error_symbol_with_terminator(j_sym, j_sym_len);
    err_code_num =
        log_bi->errcode_by_errsymbol(error_symbol_with_terminator.c_str());
  }

  if (err_code_num >= 0) {
    err_code_length = snprintf(err_code_buffer, sizeof(err_code_buffer) - 1,
                               "MY-%06u", err_code_num);
    err_code_ptr = err_code_buffer;
  }

  // convert ISO8601 timestamp to microsecond representation
  ulonglong microseconds = 0;
  if (j_time)
    microseconds = log_bi->parse_iso8601_timestamp(j_time, j_time_len);

  return log_ps->event_add(microseconds, j_thread_id, j_prio, err_code_ptr,
                           err_code_length, j_subsys, j_subsys_len, line_start,
                           line_length);
}
#endif

/**
  services: log sinks: JSON structured dump writer
  Will write structured info to stderr/file. Binary will be escaped according
  to JSON rules.
  If you should not be able to specify a label, one will be generated
  for you from the line's priority field.

  @returns         >=0                  Number of accepted fields, if any
  @returns         <0      c            Error
*/
DEFINE_METHOD(int, log_service_imp::run, (void *instance, log_line *ll)) {
  char internal_buff[LOG_BUFF_MAX];  // output buffer if none given by caller
  char esc_buff[LOG_BUFF_MAX];       // buffer to JSON-escape strings in
  char *out_buff = internal_buff;    // result buffer
  const char *inp_readpos;           // read-position in input
  char *out_writepos;                // write-position in output
  size_t len, out_left, out_size = sizeof(internal_buff), inp_left;
  int wellknown_label, out_fields = 0;
  const char *comma = (pretty != JSON_NOSPACE) ? " " : "";
  const char *separator;
  enum loglevel level = ERROR_LEVEL;
  log_item_type item_type = LOG_ITEM_END;
  log_item_type_mask out_types = 0;
  log_item_iter *it;
  log_item *li;
#ifdef WITH_PFS_SUPPORT
  log_item *output_buffer = log_bi->line_get_output_buffer(ll);

  // use caller's buffer if one was provided
  if (output_buffer != nullptr) {
    out_buff = (char *)output_buffer->data.data_buffer.str;
    out_size = output_buffer->data.data_buffer.length;
  }
#endif

  out_writepos = out_buff;
  out_left = out_size;

  if (instance == nullptr) return LOG_SERVICE_INVALID_ARGUMENT;

  if (pretty == JSON_NOSPACE)
    separator = ":";
  else if (pretty == JSON_MULTILINE)
    separator = ": ";
  else
    separator = " : ";

  if ((it = log_bi->line_item_iter_acquire(ll)) == nullptr)
    return LOG_SERVICE_MISC_ERROR; /* purecov: inspected */

  if ((li = log_bi->line_item_iter_first(it)) != nullptr) {
    len = log_bs->substitute(out_writepos, out_left, "{");
    out_left -= len;
    out_writepos += len;

    // Iterate until we're out of items, or out of space in the resulting row.
    while ((li != nullptr) && (out_left > 0)) {
      item_type = li->type;
      len = 0;

      // Sanity-check the item.
      if (log_bi->item_inconsistent(li)) {
        len =
            log_bs->substitute(out_writepos, out_left,
                               "%s\"%s\"%s\"log_sink_json: broken item with "
                               "class %d, type %d\"",
                               comma, (li->key == nullptr) ? "_null" : li->key,
                               separator, li->item_class, li->type);
        item_type = LOG_ITEM_END;  // do not flag current item-type as added
        goto broken_item;  // add this notice if there is enough space left
      }

      if (item_type == LOG_ITEM_LOG_PRIO) {
        level = static_cast<enum loglevel>(li->data.data_integer);
      }

      switch (li->item_class) {
        case LOG_LEX_STRING:
          inp_readpos = li->data.data_string.str;
          inp_left = li->data.data_string.length;

          if (inp_readpos != nullptr) {
            size_t esc_len = 0;  // characters used in escape buffer

            // escape value for JSON: \ " \x00..\x1f  (RfC7159)
            while (inp_left-- > 0) {
              if ((*inp_readpos == '\\') || (*inp_readpos == '\"')) {
                if (esc_len >= (sizeof(esc_buff) - 2)) goto skip_item;
                esc_buff[esc_len++] = '\\';
                esc_buff[esc_len++] = *(inp_readpos++);
              } else if (((unsigned char)*inp_readpos) <= 0x1f) {
                size_t esc_have = sizeof(esc_buff) - esc_len - 1;
                size_t esc_want;
                esc_want = log_bs->substitute(
                    &esc_buff[esc_len], esc_have, "\\u%04x",
                    (unsigned int)((unsigned char)*(inp_readpos++)));
                if (esc_want >= esc_have) goto skip_item;

                esc_len += esc_want;
              } else {
                if (esc_len >= (sizeof(esc_buff) - 1)) goto skip_item;
                esc_buff[esc_len++] = *(inp_readpos++);
              }
            }
            esc_buff[esc_len] = '\0';

            len = log_bs->substitute(out_writepos, out_left,
                                     "%s\"%s\"%s\"%.*s\"", comma, li->key,
                                     separator, (int)esc_len, esc_buff);
          }
          break;

        case LOG_INTEGER:
          len = log_bs->substitute(out_writepos, out_left, "%s\"%s\"%s%lld",
                                   comma, li->key, separator,
                                   li->data.data_integer);
          break;

        case LOG_FLOAT:
          len = log_bs->substitute(out_writepos, out_left, "%s\"%s\"%s%.12lf",
                                   comma, li->key, separator,
                                   li->data.data_float);
          break;

        default:
          // unknown item-class
          goto skip_item; /* purecov: inspected */
          break;
      }

      // label: item is malformed or otherwise broken; notice inserted instead
    broken_item:

      // item is too large, skip it
      if (len > out_left) {
        *out_writepos = '\0';  // "remove" truncated write
        goto skip_item;
      }

      out_types |= item_type;  // successfully added; flag item-type as present

      out_fields++;
      out_left -= len;
      out_writepos += len;

      comma = (pretty == JSON_MULTILINE)
                  ? ",\n  "
                  : ((pretty == JSON_NOSPACE) ? "," : ", ");

      // label: item was too large and therefore was skipped
    skip_item:
      li = log_bi->line_item_iter_next(it);
    }

    if (out_fields > 0) {
      if ((out_types & LOG_ITEM_LOG_PRIO) &&
          !(out_types & LOG_ITEM_LOG_LABEL) && (out_left > 0)) {
        const char *label = log_bi->label_from_prio(level);

        wellknown_label = log_bi->wellknown_by_type(LOG_ITEM_LOG_LABEL);
        len = log_bs->substitute(
            out_writepos, out_left, "%s\"%s\"%s\"%.*s\"", comma,
            log_bi->wellknown_get_name((log_item_type)wellknown_label),
            separator, (int)log_bs->length(label), label);
        if (len < out_left) {
          out_fields++;
          out_left -= len;
          out_writepos += len;
          out_types |= LOG_ITEM_LOG_LABEL;
        } else                  // prio didn't fit
          *out_writepos = '\0'; /* purecov: inspected */
      }

      /*
        We're multiplexing several JSON streams into the same output
        stream, so we add a stream_id so they can be told apart.
      */
      if ((log_bi->dedicated_errstream(((my_state *)instance)->errstream) <
           1) &&
          (opened > 1)) {
        len = log_bs->substitute(out_writepos, out_left, "%s\"%s\"%s\"%d\"",
                                 comma, "stream_id", separator,
                                 ((my_state *)instance)->id);
        if (len < out_left) {
          out_fields++;
          out_left -= len;
          out_writepos += len;
        } else       // no soft-fail for "cannot write needed stream_id"
          goto fail; /* purecov: inspected */
      }

      len = log_bs->substitute(out_writepos, out_left,
                               (pretty != JSON_NOSPACE) ? " }" : "}");
      if (len >= out_left)  // no soft-fail for "cannot write needed terminator"
        goto fail;          /* purecov: inspected */

      out_left -= len;
      out_writepos += len;

#ifdef WITH_PFS_SUPPORT
      // support for performance_schema.error_log
      if (output_buffer != nullptr) {
        output_buffer->data.data_buffer.length = out_size - out_left;
        // we update this only if we created a valid record
        output_buffer->type = LOG_ITEM_RET_BUFFER;
      }
#endif

      // write the record to the stream / log-file
      log_bi->write_errstream(((my_state *)instance)->errstream, out_buff,
                              (size_t)out_size - out_left);
    }
  }

fail:
  log_bi->line_item_iter_release(it);

  return out_fields;
}

// See log_service_imp::get_log_name below for description
static log_service_error get_json_log_name(void *instance, char *buf,
                                           size_t bufsize) {
  my_state *mi = (my_state *)instance;
  int stream_id = 0;  // default stream-ID
  size_t len;

  if (buf == nullptr) {
    return LOG_SERVICE_BUFFER_SIZE_INSUFFICIENT; /* purecov: inspected */
  }

  // instance was given and has an existing extension. return it.
  if ((mi != nullptr) && (mi->ext != nullptr)) {
    if (bufsize <= strlen(mi->ext)) {  // caller's buffer is too small
      return LOG_SERVICE_BUFFER_SIZE_INSUFFICIENT; /* purecov: inspected */
    }
    strcpy(buf, mi->ext);
    return LOG_SERVICE_SUCCESS;
  }

  // no extension exists. make one.

  // is there enough space?
  if (bufsize < (3 + sizeof(LOG_EXT)))           // caller's buffer is too small
    return LOG_SERVICE_BUFFER_SIZE_INSUFFICIENT; /* purecov: inspected */

  if (mi != nullptr)  // non-default session was given; retrieve its stream-ID.
    stream_id = mi->id;

  // make the name with the correct stream-ID.
  len = log_bs->substitute(buf, bufsize, ".%02d" LOG_EXT, stream_id);

  if (len >= bufsize)
    return LOG_SERVICE_BUFFER_SIZE_INSUFFICIENT; /* purecov: inspected */

  return LOG_SERVICE_SUCCESS;
}

/**
  Provide the name for a log file this service would access.

  @param instance  instance info returned by open() if requesting
                   the file-name for a specific open instance.
                   nullptr to get the name of the default instance
                   (even if it that log is not open). This is used
                   to determine the name of the log-file to load on
                   start-up.
  @param buf       Address of a buffer allocated in the caller.
                   The callee may return an extension starting
                   with '.', in which case the path and file-name
                   will be the system's default, except with the
                   given extension.
                   Alternatively, the callee may return a file-name
                   which is assumed to be in the same directory
                   as the default log.
                   Values are C-strings.
  @param bufsize   The size of the allocation in the caller.

  @retval  0   Success
  @retval -1   Mode not supported (only default / only instances supported)
  @retval -2   Buffer not large enough
  @retval -3   Misc. error
*/
DEFINE_METHOD(log_service_error, log_service_imp::get_log_name,
              (void *instance, char *buf, size_t bufsize)) {
  return get_json_log_name(instance, buf, bufsize);
}

/**
  Open a new instance.

  @retval  <0        a new instance could not be created
  @retval  =0        success, returned handle is valid
*/
DEFINE_METHOD(log_service_error, log_service_imp::open,
              (log_line * ll [[maybe_unused]], void **instance)) {
  log_service_error rr;
  my_state *mi;
  char buff[10];
  size_t len;

  if (instance == nullptr)               // nowhere to return the handle
    return LOG_SERVICE_INVALID_ARGUMENT; /* purecov: inspected */

  *instance = nullptr;

  if (opened >= 99)  // limit instances, and thus file-name length
    return LOG_SERVICE_TOO_MANY_INSTANCES; /* purecov: inspected */

  // malloc state failed
  if ((mi = (my_state *)log_bs->malloc(sizeof(my_state))) == nullptr) {
    rr = LOG_SERVICE_OUT_OF_MEMORY; /* purecov: inspected */
    goto fail;                      /* purecov: inspected */
  }

  mi->ext = nullptr;
  mi->id = opened;
  mi->errstream = nullptr;

  if ((rr = get_json_log_name(mi, buff, sizeof(buff))) != LOG_SERVICE_SUCCESS)
    goto fail_with_free; /* purecov: inspected */

  len = strlen(buff);

  if ((mi->ext = log_bs->strndup(buff, len + 1)) ==
      nullptr) {                    // copy ext failed
    rr = LOG_SERVICE_OUT_OF_MEMORY; /* purecov: inspected */
    goto fail_with_free;            /* purecov: inspected */
  }

  if ((rr = log_bi->open_errstream(mi->ext, &mi->errstream)) >= 0) {
    opened++;
    *instance = (void *)mi;
    return LOG_SERVICE_SUCCESS;
  }

  log_bs->free(mi->ext); /* purecov: begin inspected */

fail_with_free:
  log_bs->free(mi);

fail:
  return rr; /* purecov: end */
}

/**
  Close and release an instance. Flushes any buffers.

  @retval  <0        an error occurred
  @retval  =0        success
*/
DEFINE_METHOD(log_service_error, log_service_imp::close, (void **instance)) {
  my_state *mi;
  log_service_error rr;

  if (instance == nullptr) return LOG_SERVICE_INVALID_ARGUMENT;

  mi = (my_state *)*instance;

  *instance = nullptr;

  opened--;

  rr = log_bi->close_errstream(&mi->errstream);

  if (mi->ext != nullptr) log_bs->free(mi->ext);

  log_bs->free(mi);

  return rr;
}

/**
  Flush any buffers.  This function will be called by the server
  on FLUSH ERROR LOGS.  The service may write its buffers, close
  and re-open any log files to work with log-rotation, etc.
  The flush function MUST NOT itself log anything (as the caller
  holds THR_LOCK_log_stack)!
  A service implementation may provide a nullptr if it does not
  wish to provide a flush function.

  @retval  LOG_SERVICE_NOTHING_DONE        no work was done
  @retval  LOG_SERVICE_SUCCESS             flush completed without incident
  @retval  otherwise                       an error occurred
*/
DEFINE_METHOD(log_service_error, log_service_imp::flush, (void **instance)) {
  my_state *mi;

  if (instance == nullptr) return LOG_SERVICE_INVALID_ARGUMENT;

  if ((mi = *((my_state **)instance)) == nullptr)
    return LOG_SERVICE_INVALID_ARGUMENT; /* purecov: inspected */

  return log_bi->reopen_errstream(mi->ext, &mi->errstream);
}

/**
  Get characteristics of a log-service.

  @retval  <0        an error occurred
  @retval  >=0       characteristics (a set of log_service_chistics flags)
*/
DEFINE_METHOD(int, log_service_imp::characteristics, (void)) {
  return LOG_SERVICE_SINK
#ifdef WITH_LOG_PARSER
         | LOG_SERVICE_LOG_PARSER
#endif
#ifdef WITH_PFS_SUPPORT
         | LOG_SERVICE_PFS_SUPPORT
#endif
      ;
}

/**
  De-initialization method for Component used when unloading the Component.

  @return Status of performed operation
  @retval false success
  @retval true  failure
*/
mysql_service_status_t log_service_exit() {
  if (inited) {
    inited = false;

    return false;
  }
  return true;
}

/**
  Initialization entry method for Component used when loading the Component.

  @return Status of performed operation
  @retval false success
  @retval true  failure
*/
mysql_service_status_t log_service_init() {
  if (inited) return true;

  inited = true;
  opened = 0;

  log_bi = mysql_service_log_builtins;
  log_bs = mysql_service_log_builtins_string;
#ifdef WITH_LOG_PARSER
  log_ps = mysql_service_log_sink_perfschema;
#endif

  return false;
}

/* implementing a service: log_service */
BEGIN_SERVICE_IMPLEMENTATION(log_sink_json, log_service)
log_service_imp::run, log_service_imp::flush, log_service_imp::open,
    log_service_imp::close, log_service_imp::characteristics,
#ifdef WITH_LOG_PARSER
    log_service_imp::parse_log_line,
#else
    nullptr,
#endif
    log_service_imp::get_log_name END_SERVICE_IMPLEMENTATION();

/* component provides: just the log_service service, for now */
BEGIN_COMPONENT_PROVIDES(log_sink_json)
PROVIDES_SERVICE(log_sink_json, log_service), END_COMPONENT_PROVIDES();

/* component requires: log-builtins */
BEGIN_COMPONENT_REQUIRES(log_sink_json)
REQUIRES_SERVICE(log_builtins), REQUIRES_SERVICE(log_builtins_string),
    REQUIRES_SERVICE(log_sink_perfschema), END_COMPONENT_REQUIRES();

/* component description */
BEGIN_COMPONENT_METADATA(log_sink_json)
METADATA("mysql.author", "Oracle Corporation"),
    METADATA("mysql.license", "GPL"), METADATA("log_service_type", "sink"),
    END_COMPONENT_METADATA();

/* component declaration */
DECLARE_COMPONENT(log_sink_json, "mysql:log_sink_json")
log_service_init, log_service_exit END_DECLARE_COMPONENT();

/* components contained in this library.
   for now assume that each library will have exactly one component. */
DECLARE_LIBRARY_COMPONENTS &COMPONENT_REF(log_sink_json)
    END_DECLARE_LIBRARY_COMPONENTS

    /* EOT */
