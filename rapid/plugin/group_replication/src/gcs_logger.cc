/* Copyright (c) 2016, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "gcs_logger.h"
#include "plugin_log.h"

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

void Gcs_gr_logger_impl::log_event(gcs_log_level_t level,
                                   const char *message)
{
  DBUG_ENTER("Gcs_gr_logger_impl::log_event");

  switch (level)
  {
    case GCS_TRACE:
    case GCS_DEBUG:
    case GCS_INFO:
      log_message(MY_INFORMATION_LEVEL, message);
      break;

    case GCS_WARN:
      log_message(MY_WARNING_LEVEL, message);
      break;

    case GCS_ERROR:
    case GCS_FATAL:
      log_message(MY_ERROR_LEVEL, message);
      break;

    default:
      DBUG_ASSERT(0); /* purecov: inspected */
  }

  DBUG_VOID_RETURN;
}
