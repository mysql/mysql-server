/* Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.

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

#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_logging_system.h"
#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/xplatform/my_xp_util.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_xcom_interface.h"

extern void cb_xcom_logger(const int64_t level, const char *message);
extern void cb_xcom_debugger(const char *format, ...);
extern int cb_xcom_debugger_check(const int64_t options);

/**
  Class that defines basic logging infra-structure to be used in the test
  cases, for example.
*/
class Gcs_basic_logging
{
public:
  /*
    Pointer to a logger object that is responsible for handling fatal, error,
    warning and information messages.
  */
  Gcs_default_logger *logger;

  /*
    Pointer to a debugger object that is responsible for handling debug and
    trace messages.
  */
  Gcs_default_debugger *debugger;

  /*
    Pointer to a sink where both the messages produced by a logger or debugger
    will be written to. This is a simple logging infra-structure and messages
    are always written to the standard output.
  */
  Gcs_async_buffer *sink;

  /*
    Save debug options that will be restored when the object is destructed.
  */
  int64_t saved_debug_options;

  /**
    Constructor that creates the logger, debugger and sink.
  */
  Gcs_basic_logging() : logger(NULL), debugger(NULL), sink(NULL),
    saved_debug_options(GCS_DEBUG_NONE)
  {
    saved_debug_options= Gcs_debug_options::get_current_debug_options();
    Gcs_debug_options::force_debug_options(GCS_DEBUG_ALL);

    sink= new Gcs_async_buffer(new Gcs_output_sink());

    logger= new Gcs_default_logger(sink);
    Gcs_log_manager::initialize(logger);

    debugger= new Gcs_default_debugger(sink);
    Gcs_debug_manager::initialize(debugger);

    ::set_xcom_logger(cb_xcom_logger);
    ::set_xcom_debugger(cb_xcom_debugger);
    ::set_xcom_debugger_check(cb_xcom_debugger_check);

    Gcs_xcom_utils::init_net();
  }

  /**
    Destructor that cleans up and deallocates the logger, debugger and sink.
  */
  virtual ~Gcs_basic_logging()
  {
    Gcs_log_manager::finalize();
    logger->finalize();
    delete logger;

    Gcs_debug_manager::finalize();
    debugger->finalize();
    delete debugger;

    sink->finalize();
    delete sink;

    Gcs_debug_options::force_debug_options(saved_debug_options);
  }
};
