/* Copyright (c) 2016, 2017, Oracle and/or its affiliates. All rights reserved.

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

#include "plugin/group_replication/include/gcs_logger.h"

#include "my_dbug.h"
#include "plugin/group_replication/include/plugin_log.h"

enum_gcs_error Gcs_gr_logger_impl::initialize()
{
  DBUG_ENTER("Gcs_gr_logger_impl::initialize");
  DBUG_RETURN(GCS_OK);
}

enum_gcs_error Gcs_gr_logger_impl::finalize()
{
  DBUG_ENTER("Gcs_gr_logger_impl::finalize");
  DBUG_RETURN(GCS_OK);
}

void Gcs_gr_logger_impl::log_event(const gcs_log_level_t level,
                                   const std::string &message)
{
  DBUG_ENTER("Gcs_gr_logger_impl::log_event");

  switch (level)
  {
    case GCS_INFO:
      log_message(MY_INFORMATION_LEVEL, message.c_str());
      break;

    case GCS_WARN:
      log_message(MY_WARNING_LEVEL, message.c_str());
      break;

    case GCS_ERROR:
    case GCS_FATAL:
      log_message(MY_ERROR_LEVEL, message.c_str());
      break;

    default:
      DBUG_ASSERT(0); /* purecov: inspected */
  }

  DBUG_VOID_RETURN;
}
