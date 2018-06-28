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

LOG SINKS: WRITERS

There are two types of writers.  "Modern services" like the XML or JSON
writers don't care whether a key/value pair is a table name or an error
message, or what -- a string is a string.  Thus, these services have to
cover just three cases:  format as string, format as integer, or format
as float.  Trivial.

The other type is services that write to a fixed format, like the
traditional MySQL error log.  The error log has a timestamp,
connection ID, severity label, and an error message, in that exact order.
The traditional error log service therefore doesn't attempt to process
all the information we might send its way; it just needs to know how to
retrieve the few items it's interested in, and about printing them in
the prescribed order.  All other information is ignored.  Still simple,
just a different kind of simple.


INPUT

So as we've seen, both kinds of writers are quite straightforward.
It's either "handle 3 storage classes" (which is simple) or "handle as
many specific item types are you're interested in".  The first group
(which handles any and all items) is actually the simplest, and perhaps
easier to read and understand than the second group, as they're little
more than a glorified

  switch (item->item_class) {
  case STRING:   sprintf(buf, "%s", item->value.data_string);
                 break;
  case INTEGER:  sprintf(buf, "%d", item->value.data_integer);
                 break;
  case FLOAT:    sprintf(buf, "%f", item->value.data_float);
                 break;
  }

Anything beyond that in a given source file is usually either
boilerplate code needed by the component framework (which is
going to be there no matter how simple or complex our actual
writer is) or required by the specific output format the sink
is intended to generate (writing e.g. JSON rows or XML tags,
escaping characters/entities in string values, indentation,
and so on).


ERROR MESSAGES THAT AREN'T STRINGS, AND OTHER INSANITY

So what if a service is looking for the error message, but it turns
out that message is not a string?

Well, first off, the well-know items (log message, error number, ...)
all also have a well-known storage class, so an error message for example
shall always be of string class.  Our code (submission API, filter, ...)
ensures this.  Additionally, there is code in the server to help guard
against broken modules that violate these guarantees, so individual
loadable services won't have to check that themselves, either.

In summary, if a case like that happens, it's a bug, and it's a bug that
we have tools to detect.  The expectation is NOT that services have to
convert data-types on the fly and be able to handle error messages that
are numbers, or integers that come wrapped in a string, or any such
nonsense.  It is true that modern writers like XML or JSON gracefully
handle this case at zero overhead (as they only look at storage class,
not type or semantics), but this is very specifically NOT required of
any service.
*/

#ifndef LOG_SERVICE_IMP_H
#define LOG_SERVICE_IMP_H

#include <mysql/components/services/log_service.h>
#include <mysql/components/services/log_shared.h>

extern REQUIRES_SERVICE_PLACEHOLDER(registry);

class log_service_imp {
 public:
  /**
    Initialize a loadable logging service.
  */
  static void init();

  /**
    De-initialize a loadable logging service.
  */
  static void exit();

 public: /* Service Implementations */
  /**
    Have the service process one log line.
    If a run function wishes to itself use error logging
    in case of severe failure, it may do so after FIRST
    securing the all further calls to run() will be rejected.
    "log_sink_test" implements an example of this.

    @param   instance  State-pointer that was returned on open.
    @param   ll        The log_line collection of log_items.

    @retval  <0        an error occurred
    @retval  =0        no work was done
    @retval  >0        number of processed entities
  */
  static DEFINE_METHOD(int, run, (void *instance, log_line *ll));

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
  static DEFINE_METHOD(int, flush, (void **instance));

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
  static DEFINE_METHOD(int, open, (log_line * ll, void **instance));

  /**
    Close and release an instance. Flushes any buffers.

    @param   instance  State-pointer that was returned on open.
                       If memory was allocated for this state,
                       it should be released, and the pointer
                       set to nullptr.

    @retval  <0        an error occurred
    @retval  =0        success
  */
  static DEFINE_METHOD(int, close, (void **instance));

  /**
    Get characteristics of a log-service.

    @retval  <0        an error occurred
    @retval  >=0       characteristics (a set of log_service_chistics flags)
  */
  static DEFINE_METHOD(int, characteristics, (void));
};

#endif /* LOG_SERVICE_IMP_H */
