/* Copyright (c) 2016, 2023, Oracle and/or its affiliates.

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

#include <mysql/components/services/log_builtins.h>
#include <mysqld_error.h>
#include "my_dbug.h"

enum_gcs_error Gcs_gr_logger_impl::initialize() {
  DBUG_TRACE;
  return GCS_OK;
}

enum_gcs_error Gcs_gr_logger_impl::finalize() {
  DBUG_TRACE;
  return GCS_OK;
}

void Gcs_gr_logger_impl::log_event(const gcs_log_level_t level,
                                   const std::string &message) {
  DBUG_TRACE;

  switch (level) {
    case GCS_INFO:
      LogPluginErr(INFORMATION_LEVEL, ER_GRP_RPL_GCS_GR_ERROR_MSG,
                   message.c_str());
      break;

    case GCS_WARN:
      LogPluginErr(WARNING_LEVEL, ER_GRP_RPL_GCS_GR_ERROR_MSG, message.c_str());
      break;

    case GCS_ERROR:
    case GCS_FATAL:
      LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_GCS_GR_ERROR_MSG, message.c_str());
      break;

    default:
      assert(0); /* purecov: inspected */
  }
}
