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

#ifndef PIPELINE_FACTORY_INCLUDED
#define PIPELINE_FACTORY_INCLUDED

#include "plugin/group_replication/include/pipeline_interfaces.h"

/**
  @enum Handler_id

  Enumeration type for the different types of handlers.
*/
enum Handler_id {
  CERTIFICATION_HANDLER,
  SQL_THREAD_APPLICATION_HANDLER,
  CATALOGING_HANDLER,
};

/**
  @enum Handler_id

  Enumeration type for the different types of handlers.
*/
enum Handler_pipeline_type { STANDARD_GROUP_REPLICATION_PIPELINE = 0 };

/**
  This method joins the two above method, assembling a pipeline accordingly
  with the given configuration.

  @param[in]    pipeline_type    the selected pipeline
  @param[out]   pipeline         the assembled pipeline

  @return the end status
    @retval 0      OK
    @retval !=0    Error returned on the execution
*/
int get_pipeline(Handler_pipeline_type pipeline_type, Event_handler **pipeline);

/**
  This method returns the configured handlers for the received pipeline.

  @param[in]    pipeline_type    the selected pipeline
  @param[out]   pipeline_conf    the returned list of handler ids

  @return the number of handlers in the pipeline
*/
int get_pipeline_configuration(Handler_pipeline_type pipeline_type,
                               Handler_id **pipeline_conf);

/**
  This method configures the pipeline accordingly to the received handlers.

  Taking the received handlers, this method initializes each one of them,
  appending them to the pipeline. It also checks the handler role, checking
  for duplicated handlers that were marked as being unique.

  @param[out]  pipeline            the pipeline to configure
  @param[in]   handler_list        the list of handler ids
  @param[in]   num_handlers        the number of handlers to configure

  @return the end status
    @retval 0      OK
    @retval !=0    Error returned on the execution
*/
int configure_pipeline(Event_handler **pipeline, Handler_id handler_list[],
                       int num_handlers);

#endif /* PIPELINE_FACTORY_INCLUDED */
