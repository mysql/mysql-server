/* Copyright (c) 2020, 2022, Oracle and/or its affiliates.

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

/*
  This file contains private definitions for use within the logger,
  but not by loadable logging components or code that uses the logger
  but is not part of the logger.
*/

#ifndef LOG_BUILTINS_DATA_H
#define LOG_BUILTINS_DATA_H

#include <mysql/components/services/log_builtins_filter.h>
#include <mysql/components/services/log_service.h>
#include <mysql/components/services/log_shared.h>  // public data types

/**
  When the logger-core was initialized.

  @retval 0  logger-core is not currently available
  @retval >0 time (micro-seconds since the epoch) the logger became available
*/
extern ulonglong log_builtins_started();

/**
  MySQL server's default log-processor.

  Apply all components (filters, sinks, ...) in the log stack to a given event.

  @param  ll    the log-event to process

  @retval true  failure
  @retval false success
*/
extern bool log_line_error_stack_run(log_line *ll);

/**
  Finding and acquiring a service in the component framework is
  expensive, and we may use services a log (depending on how many
  events are logged per second), so we cache the relevant data.
  This struct describes a given service.
*/
struct log_service_cache_entry {
  char *name;            ///< name of this service
  size_t name_len;       ///< service-name's length
  char *urn;             ///< URN of loaded if implicitly loaded, or NULL
  my_h_service service;  ///< handle (service framework)
  int opened;            ///< currently open instances
  int requested;         ///< requested instances
  int chistics;          ///< multi-open supported, etc.
};

/**
  State of a given instance of a service. A service may support being
  opened several times.
*/
typedef struct _log_service_instance {
  log_service_cache_entry *sce;        ///< the service in question
  void *instance;                      ///< instance handle (multi-open)
  struct _log_service_instance *next;  ///< next instance (any service)
} log_service_instance;

extern log_service_instance *log_service_instances;  ///< anchor
extern log_service_instance *log_sink_pfs_source;    ///< log-reader

/**
  Maximum number of key/value pairs in a log event.
  May be changed or abolished later.
*/
#define LOG_ITEM_MAX 64

/**
  Iterator over the key/value pairs of a log_line.
  At present, only one iter may exist per log_line.
*/
typedef struct _log_item_iter {
  struct _log_line *ll;  ///< log_line this is the iter for
  int index;             ///< index of current key/value pair
} log_item_iter;

/**
  log_line ("log event")
*/
typedef struct _log_line {
  log_item_type_mask seen;      ///< bit field flagging item-types contained
  log_item_iter iter;           ///< iterator over key/value pairs
  log_item output_buffer;       ///< buffer a service can return its output in
  int count;                    ///< number of key/value pairs ("log items")
  log_item item[LOG_ITEM_MAX];  ///< log items
} log_line;

extern log_filter_ruleset *log_filter_builtin_rules;  // what it says on the tin

/**
  Create a log-file name (path + name + extension).
  The path will be taken from @@log_error.
  If name + extension are given, they are used.
  If only an extension is given (argument starts with '.'),
  the name is taken from @@log_error, and the extension is used.
  If only a name is given (but no extension), the name and a
  default extension are used.

  @param  result        Buffer to return to created path+name+extension in.
                        Size must be FN_REFLEN.
  @param  name_or_ext   if beginning with '.':
                          @@global.log_error, except with this extension
                        otherwise:
                          use this as file name in the same location as
                          @@global.log_error

                        Value may not contain folder separators!

  @retval LOG_SERVICE_SUCCESS                   buffer contains a valid result
  @retval LOG_SERVICE_BUFFER_SIZE_INSUFFICIENT  an error occurred
*/
log_service_error make_log_path(char *result, const char *name_or_ext);

/**
  Acquire an exclusive lock on the error logger core.

  Used e.g. to pause all logging while the previous run's
  log is read to performance_schema.error_log.
*/
void log_builtins_error_stack_wrlock();

/**
  Release a lock on the error logger core.
*/
void log_builtins_error_stack_unlock();

#endif /* LOG_BUILTINS_DATA_H */
