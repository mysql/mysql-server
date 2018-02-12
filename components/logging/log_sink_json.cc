/* Copyright (c) 2017, 2018, Oracle and/or its affiliates. All rights reserved.

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
  wellknown types (string, float, int), it can and will write it.

  By default, each line will contain one log event, in a format
  somewhat similar to that emitted by "journalctl -o json" on
  platforms that use systemd's journal:

{ "prio" : 2, "err_code" : 3581, "msg" : "Parser saw: SET
@@global.log_error_filter_rules= DEFAULT", "err_symbol" : "ER_PARSER_TRACE",
"label" : "Note" } { "prio" : 2, "err_code" : 3581, "subsystem" : "parser",
"SQL_state" : "XX999", "source_file" : "sql_parse", "function" :
"dispatch_command", "msg" : "Parser saw: SELECT \"loging as traditional MySQL
error log and as JSON\"", "time" : "1970-01-01T00:00:00.000000Z", "thread" : 0,
"err_symbol" : "ER_PARSER_TRACE", "label" : "Note" }
*/

#include <mysql/components/services/log_builtins.h>
#include <mysql/components/services/log_shared.h>
#include "log_service_imp.h"
#include "my_compiler.h"

REQUIRES_SERVICE_PLACEHOLDER(log_builtins);
REQUIRES_SERVICE_PLACEHOLDER(log_builtins_string);

SERVICE_TYPE(log_builtins) *log_bi = nullptr;
SERVICE_TYPE(log_builtins_string) *log_bs = nullptr;

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

/**
  variable listener. This is a temporary solution until we have
  per-component system variables. "check" is where our component
  can veto.

  @param   ll  a list-item describing the variable (name, new value)

  @retval   0  for allow (including when we don't feel the event is for us),
  @retval  <0  deny (nullptr, malformed structures, etc. -- caller broken?)
  @retval  >0  deny (user input rejected)
*/
DEFINE_METHOD(int, log_service_imp::variable_check,
              (log_line * ll MY_ATTRIBUTE((unused)))) {
  return 0;
}

/**
  variable listener. This is a temporary solution until we have
  per-component system variables. "update" is where we're told
  to update our state (if the variable concerns us to begin with).

  @param   ll  a list-item describing the variable (name, new value)

  @retval  0  the event is not for us
  @retval <0  for failure
  @retval >0  for success (at least one item was updated)
*/
DEFINE_METHOD(int, log_service_imp::variable_update,
              (log_line * ll MY_ATTRIBUTE((unused)))) {
  return 0;
}

/**
  services: log sinks: JSON structured dump writer
  Will write structured info to stderr/file. Binary will be escaped according
  to JSON rules.
  If you should not be able to specify a label, one will be generated
  for you from the line's priority field.

  @param           instance             instance state
  @param           ll                   the log line to write
  @retval          int                  number of accepted fields, if any
*/
DEFINE_METHOD(int, log_service_imp::run, (void *instance, log_line *ll)) {
  char out_buff[LOG_BUFF_MAX];
  char esc_buff[LOG_BUFF_MAX];
  const char *inp_readpos;
  char *out_writepos = out_buff;
  size_t len, out_left = LOG_BUFF_MAX, inp_left;
  int wellknown_label, out_fields = 0;
  const char *comma = (pretty != JSON_NOSPACE) ? " " : "";
  const char *separator;
  enum loglevel level = ERROR_LEVEL;
  log_item_type item_type = LOG_ITEM_END;
  log_item_type_mask out_types = 0;
  log_item_iter *it;
  log_item *li;

  if (instance == nullptr) return out_fields;

  if (pretty == JSON_NOSPACE)
    separator = ":";
  else if (pretty == JSON_MULTILINE)
    separator = ": ";
  else
    separator = " : ";

  if ((it = log_bi->line_item_iter_acquire(ll)) == nullptr) return out_fields;

  if ((li = log_bi->line_item_iter_first(it)) != nullptr) {
    len = log_bs->substitute(out_writepos, out_left, "{");
    out_left -= len;
    out_writepos += len;

    while ((li != nullptr) && (out_left > 0)) {
      item_type = li->type;

      if (log_bi->item_inconsistent(li)) {
        len =
            log_bs->substitute(out_writepos, out_left,
                               "%s\"%s\"%s\"log_sink_json: broken item with "
                               "class %d, type %d\"",
                               comma, (li->key == nullptr) ? "_null" : li->key,
                               separator, li->item_class, li->type);
        goto broken_item;
      }

      if (item_type == LOG_ITEM_LOG_PRIO) {
        level = static_cast<enum loglevel>(li->data.data_integer);
      }

      switch (li->item_class) {
        case LOG_LEX_STRING:
          inp_readpos = li->data.data_string.str;
          inp_left = li->data.data_string.length;

          if (inp_readpos != nullptr) {
            size_t esc_len = 0;

            // escape value for JSON: \ " \x00..\x1f  (RfC7159)
            while ((inp_left-- > 0) && (esc_len < LOG_BUFF_MAX - 2)) {
              if ((*inp_readpos == '\\') || (*inp_readpos == '\"')) {
                esc_buff[esc_len++] = '\\';
                esc_buff[esc_len++] = *(inp_readpos++);
              } else if (*inp_readpos <= 0x1f) {
                esc_len += log_bs->substitute(&esc_buff[esc_len],
                                              LOG_BUFF_MAX - esc_len - 1,
                                              "\\u%04x", (int)*(inp_readpos++));
              } else
                esc_buff[esc_len++] = *(inp_readpos++);
            }
            esc_buff[esc_len] = '\0';

            len = log_bs->substitute(out_writepos, out_left,
                                     "%s\"%s\"%s\"%.*s\"", comma, li->key,
                                     separator, (int)esc_len, esc_buff);
          } else
            len = 0;
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
          break;
      }

      out_types |= item_type;

    broken_item:
      out_fields++;
      out_left -= len;
      out_writepos += len;

      comma = (pretty == JSON_MULTILINE)
                  ? ",\n  "
                  : ((pretty == JSON_NOSPACE) ? "," : ", ");

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
        out_left -= len;
        out_writepos += len;
        out_types |= item_type;
      }

      /*
        We're multiplexing several JSON streams into the same output
        stream, so add a stream_id to they can be told apart.
      */
      if ((log_bi->dedicated_errstream(((my_state *)instance)->errstream) <
           1) &&
          (opened > 1)) {
        len = log_bs->substitute(out_writepos, out_left, "%s\"%s\"%s\"%d\"",
                                 comma, "stream_id", separator,
                                 ((my_state *)instance)->id);
        out_left -= len;
        out_writepos += len;
      }

      len = log_bs->substitute(out_writepos, out_left,
                               (pretty != JSON_NOSPACE) ? " }" : "}");
      out_left -= len;
      out_writepos += len;

      log_bi->write_errstream(((my_state *)instance)->errstream, out_buff,
                              (size_t)LOG_BUFF_MAX - out_left);
    }
  }

  log_bi->line_item_iter_release(it);

  return out_fields;
}

/**
  Open a new instance.

  @param   ll        optional arguments
  @param   instance  If state is needed, the service may allocate and
                     initialize it and return a pointer to it here.
                     (This of course is particularly pertinent to
                     components that may be opened multiple times,
                     such as the JSON log writer.)
                     This state is for use of the log-service component
                     in question only and can take any layout suitable
                     to that component's need. The state is opaque to
                     the server/logging framework. It must be released
                     on close.

  @retval  <0        a new instance could not be created
  @retval  =0        success, returned hande is valid
*/
DEFINE_METHOD(int, log_service_imp::open,
              (log_line * ll MY_ATTRIBUTE((unused)), void **instance)) {
  int rr;
  my_state *mi;
  char buff[10];
  size_t len;

  if (instance == nullptr) return -1;

  *instance = nullptr;

  if ((mi = (my_state *)log_bs->malloc(sizeof(my_state))) == nullptr) {
    rr = -2;
    goto fail;
  }

  mi->id = opened;

  len = log_bs->substitute(buff, 9, ".%02d.json", mi->id);

  if ((mi->ext = log_bs->strndup(buff, len + 1)) == nullptr) {
    rr = -3;
    goto fail_with_free;
  }

  if ((rr = log_bi->open_errstream(mi->ext, &mi->errstream)) >= 0) {
    opened++;
    *instance = (void *)mi;
    return 0;
  }

  log_bs->free(mi->ext);
  rr = -4;

fail_with_free:
  log_bs->free(mi);

fail:
  return rr;
}

/**
  Close and release an instance. Flushes any buffers.

  @param   instance  State-pointer that was returned on open.
                     If memory was allocated for this state,
                     it should be released, and the pointer
                     set to nullptr.

  @retval  <0        an error occurred
  @retval  =0        success
*/
DEFINE_METHOD(int, log_service_imp::close, (void **instance)) {
  my_state *mi;
  int rr;

  if (instance == nullptr) return -1;

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
  The flush function MUST NOT itself log anything!
  A service implementation may provide a nullptr if it does not
  wish to provide a flush function.

  @param   instance  State-pointer that was returned on open.
                     Value may be changed in flush.

  @retval  <0        an error occurred
  @retval  =0        no work was done
  @retval  >0        flush completed without incident
*/
DEFINE_METHOD(int, log_service_imp::flush, (void **instance)) {
  my_state *mi;

  if (instance == nullptr) return -1;

  if ((mi = *((my_state **)instance)) == nullptr) return -2;

  log_bi->close_errstream(&mi->errstream);

  return log_bi->open_errstream(mi->ext, &mi->errstream);
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

  return false;
}

/* implementing a service: log_service */
BEGIN_SERVICE_IMPLEMENTATION(log_sink_json, log_service)
log_service_imp::run, log_service_imp::flush, log_service_imp::open,
    log_service_imp::close, log_service_imp::variable_check,
    log_service_imp::variable_update END_SERVICE_IMPLEMENTATION();

/* component provides: just the log_service service, for now */
BEGIN_COMPONENT_PROVIDES(log_sink_json)
PROVIDES_SERVICE(log_sink_json, log_service), END_COMPONENT_PROVIDES();

/* component requires: log-builtins */
BEGIN_COMPONENT_REQUIRES(log_sink_json)
REQUIRES_SERVICE(log_builtins), REQUIRES_SERVICE(log_builtins_string),
    END_COMPONENT_REQUIRES();

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
