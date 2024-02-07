/* Copyright (c) 2013, 2024, Oracle and/or its affiliates.

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

#include "plugin/group_replication/include/pipeline_factory.h"

#include <stddef.h>

#include <mysql/components/services/log_builtins.h>
#include "my_dbug.h"
#include "plugin/group_replication/include/handlers/applier_handler.h"
#include "plugin/group_replication/include/handlers/certification_handler.h"
#include "plugin/group_replication/include/handlers/event_cataloger.h"

int get_pipeline(Handler_pipeline_type pipeline_type,
                 Event_handler **pipeline) {
  DBUG_TRACE;

  Handler_id *handler_list = nullptr;
  int num_handlers = get_pipeline_configuration(pipeline_type, &handler_list);
  int error = configure_pipeline(pipeline, handler_list, num_handlers);
  if (handler_list != nullptr) {
    delete[] handler_list;
  }
  // when there are no handlers, the pipeline is not valid
  return error || num_handlers == 0;
}

int get_pipeline_configuration(Handler_pipeline_type pipeline_type,
                               Handler_id **pipeline_conf) {
  DBUG_TRACE;
  /*
    When a new pipeline is defined the developer shall define here what are
    the handlers that belong to it and their order.
  */
  switch (pipeline_type) {
    case STANDARD_GROUP_REPLICATION_PIPELINE:
      (*pipeline_conf) = new Handler_id[3];
      (*pipeline_conf)[0] = CATALOGING_HANDLER;
      (*pipeline_conf)[1] = CERTIFICATION_HANDLER;
      (*pipeline_conf)[2] = SQL_THREAD_APPLICATION_HANDLER;
      return 3;
    default:
      /* purecov: begin inspected */
      LogPluginErr(ERROR_LEVEL,
                   ER_GRP_RPL_UNKNOWN_GRP_RPL_APPLIER_PIPELINE_REQUESTED);
      /* purecov: end */
  }
  return 0; /* purecov: inspected */
}

int configure_pipeline(Event_handler **pipeline, Handler_id handler_list[],
                       int num_handlers) {
  DBUG_TRACE;
  int error = 0;

  for (int i = 0; i < num_handlers; ++i) {
    Event_handler *handler = nullptr;

    /*
      When a new handler is define the developer shall insert it here
    */
    switch (handler_list[i]) {
      case CATALOGING_HANDLER:
        handler = new Event_cataloger();
        break;
      case CERTIFICATION_HANDLER:
        handler = new Certification_handler();
        break;
      case SQL_THREAD_APPLICATION_HANDLER:
        handler = new Applier_handler();
        break;
      default:
        /* purecov: begin inspected */
        error = 1;
        LogPluginErr(
            ERROR_LEVEL,
            ER_GRP_RPL_FAILED_TO_BOOTSTRAP_EVENT_HANDLING_INFRASTRUCTURE,
            handler_list[i]);
        /* purecov: end */
    }

    if (!handler) {
      /* purecov: begin inspected */
      if (!error)  // not an unknown handler but a initialization error
      {
        LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_APPLIER_HANDLER_NOT_INITIALIZED);
      }
      return 1;
      /* purecov: end */
    }

    /*
      TODO: This kind of tests don't belong here. We need a way to
      do this in a static way before initialization.
    */
    // Record the handler role if unique
    if (handler->is_unique()) {
      for (int z = 0; z < i; ++z) {
        DBUG_EXECUTE_IF("double_unique_handler",
                        handler_list[z] = handler_list[i];);

        // Check to see if the handler was already used in this pipeline
        if (handler_list[i] == handler_list[z]) {
          LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_APPLIER_HANDLER_IS_IN_USE);
          delete handler;
          return 1;
        }

        // check to see if no other handler has the same role
        Event_handler *handler_with_same_role = nullptr;
        Event_handler::get_handler_by_role(*pipeline, handler->get_role(),
                                           &handler_with_same_role);
        if (handler_with_same_role != nullptr) {
          /* purecov: begin inspected */
          LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_APPLIER_HANDLER_ROLE_IS_IN_USE);
          delete handler;
          return 1;
          /* purecov: end */
        }
      }
    }

    if ((error = handler->initialize())) {
      /* purecov: begin inspected */
      LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_FAILED_TO_INIT_APPLIER_HANDLER);
      return error;
      /* purecov: end */
    }

    // Add the handler to the pipeline
    Event_handler::append_handler(pipeline, handler);
  }
  return 0;
}
