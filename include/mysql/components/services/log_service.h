/* Copyright (c) 2017, 2022, Oracle and/or its affiliates.

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

  This defines the API used to call functions in logging components.
  When implementing such a service, refer to log_service_imp.h instead!

  A log service may take the shape of a writer for a specific log format
  (JSON, XML, traditional MySQL, etc.), it may implement a filter that
  removes or modifies log_items, etc.
*/

#ifndef LOG_SERVICE_H
#define LOG_SERVICE_H

#include <mysql/components/component_implementation.h>
#include <mysql/components/my_service.h>
#include <mysql/components/service_implementation.h>
#include <mysql/components/services/log_shared.h>

/* service helpers */
typedef enum enum_log_service_chistics {
  /** We do not have information about this service yet. */
  LOG_SERVICE_UNSPECIFIED = 0,

  /** Service is read-only -- it guarantees it will not modify the log-event.
      This information may later be used to e.g. run log-writers in parallel. */
  LOG_SERVICE_READ_ONLY = 1,

  /** Service is a singleton -- it may occur in the log service pipeline
      only once. */
  LOG_SERVICE_SINGLETON = 2,

  /** Service is built-in (and can not be INSTALLed/UNINSTALLed */
  LOG_SERVICE_BUILTIN = 4,

  // values from 8..64 are reserved for extensions

  /** Service is a source. It adds key/value pairs beyond those in the
      statement that first created the log-event. Log-sources are not
      normally READ_ONLY. */
  LOG_SERVICE_SOURCE = 128,

  /** Service is a filter. A filter should not be the last service in
      the log service pipeline. */
  LOG_SERVICE_FILTER = 256,

  /** Service is a sink (usually a log-writer). Sinks will normally
      not modify the log-event, but be READ_ONLY. */
  LOG_SERVICE_SINK = 512,

  /** Service supports the performance_schema.error_log table.
      If the caller provides a buffer, the service will write output
      to be displayed in the performance-schema table there.
      This can be the entirety of the log-entry, or a projection
      thereof (usually omitting key/value pairs that are already
      shown in other columns of said table).
      Services flagged this must also be flagged LOG_SERVICE_SINK! */
  LOG_SERVICE_PFS_SUPPORT = 1024,

  /** Service can parse lines in the format it outputs. Services flagged
      this must also be flagged LOG_SERVICE_SINK | LOG_SERVICE_PFS_SUPPORT! */
  LOG_SERVICE_LOG_PARSER = 2048,
} log_service_chistics;

/**
  Error codes. These are grouped (general issues, invalid data,
  file ops, etc.). Each group has a sparsely populated range so
  we may add entries as needed without introducing incompatibility
  by renumbering the existing ones.
*/
typedef enum enum_log_service_error {
  /// no error
  LOG_SERVICE_SUCCESS = 0,

  /// error not otherwise specified
  LOG_SERVICE_MISC_ERROR = -1,

  /// no error, but no effect either
  LOG_SERVICE_NOTHING_DONE = -2,

  /**
    arguments are valid, we just don't have the space (either pre-allocated
    in this function, or passed to us by the caller)
  */
  LOG_SERVICE_BUFFER_SIZE_INSUFFICIENT = -10,

  /// we cannot allocate a (temporary or return) buffer of the required size
  LOG_SERVICE_OUT_OF_MEMORY = -11,

  /// service uninavailable (bad internal state/underlying service unavailable)
  LOG_SERVICE_NOT_AVAILABLE = -20,

  /// for a method with modes, a mode unsupported by this service was requested
  LOG_SERVICE_UNSUPPORTED_MODE = -21,

  /// argument was invalid (out of range, malformed, etc.)
  LOG_SERVICE_INVALID_ARGUMENT = -30,

  /**
    argument too long (a special case of malformed).
    E.g. a path longer than FN_REFLEN, or a presumed data longer than
    specified in ISO-8601.
    use LOG_SERVICE_INVALID_ARGUMENT, LOG_SERVICE_BUFFER_SIZE_INSUFFICIENT,
    or LOG_SERVICE_OUT_OF_MEMORY if more appropriate.
  */
  LOG_SERVICE_ARGUMENT_TOO_LONG = -31,

  /**
    invalid data, but not arguments to a C++ function
    (bad log-file to parse, filter language statement, etc.)
  */
  LOG_SERVICE_PARSE_ERROR = -32,

  /// could not make log name
  LOG_SERVICE_COULD_NOT_MAKE_LOG_NAME = -40,

  /// lock error (could not init, or is not inited, etc.)
  LOG_SERVICE_LOCK_ERROR = -50,

  /// can not write
  LOG_SERVICE_UNABLE_TO_WRITE = -60,
  LOG_SERVICE_UNABLE_TO_READ = -61,
  LOG_SERVICE_OPEN_FAILED = -62,
  LOG_SERVICE_CLOSE_FAILED = -63,
  LOG_SERVICE_SEEK_FAILED = -64,

  /// no more instances of this service are possible.
  LOG_SERVICE_TOO_MANY_INSTANCES = -99
} log_service_error;

BEGIN_SERVICE_DEFINITION(log_service)
/**
  Have the service process one log line.

  @param   instance  State-pointer that was returned on open.
  @param   ll        The log_line collection of log_items.

  @retval  <0        an error occurred
  @retval  =0        no work was done
  @retval  >0        number of processed entities
*/
DECLARE_METHOD(int, run, (void *instance, log_line *ll));

/**
  Flush any buffers.  This function will be called by the server
  on FLUSH ERROR LOGS.  The service may write its buffers, close
  and re-open any log files to work with log-rotation, etc.
  The flush function MUST NOT itself log anything (as the caller
  holds THR_LOCK_log_stack)!
  A service implementation may provide a nullptr if it does not
  wish to provide a flush function.

  @param   instance  State-pointer that was returned on open.
                     Value may be changed in flush.

  @returns  LOG_SERVICE_NOTHING_DONE        no work was done
  @returns  LOG_SERVICE_SUCCESS             flush completed without incident
  @returns  otherwise                       an error occurred
*/
DECLARE_METHOD(log_service_error, flush, (void **instance));

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

  @returns  LOG_SERVICE_SUCCESS        success, returned handle is valid
  @returns  otherwise                  a new instance could not be created
*/
DECLARE_METHOD(log_service_error, open, (log_line * ll, void **instance));

/**
  Close and release an instance. Flushes any buffers.

  @param   instance  State-pointer that was returned on open.
                     If memory was allocated for this state,
                     it should be released, and the pointer
                     set to nullptr.

  @returns  LOG_SERVICE_SUCCESS        success
  @returns  otherwise                  an error occurred
*/
DECLARE_METHOD(log_service_error, close, (void **instance));

/**
  Get characteristics of a log-service.

  @retval  <0        an error occurred
  @retval  >=0       characteristics (a set of log_service_chistics flags)
*/
DECLARE_METHOD(int, characteristics, (void));

/**
  Parse a single line in an error log of this format.  (optional)

  @param line_start   pointer to the beginning of the line ('{')
  @param line_length  length of the line

  @retval  0   Success
  @retval !=0  Failure (out of memory, malformed argument, etc.)
*/
DECLARE_METHOD(log_service_error, parse_log_line,
               (const char *line_start, size_t line_length));

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
DECLARE_METHOD(log_service_error, get_log_name,
               (void *instance, char *buf, size_t bufsize));

END_SERVICE_DEFINITION(log_service)

#endif
