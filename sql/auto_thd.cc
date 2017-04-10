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
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02111-1307  USA */

#include "auto_thd.h"

#include "log.h"                      // sql_print_error
#include "sql_class.h"                // THD
#include "sql_thd_internal_api.h"     // create_thd / destroy_thd

/**
  Create THD object and initialize internal variables.
*/
Auto_THD::Auto_THD() :
  thd(create_thd(false, true, false, 0))
{
  thd->push_internal_handler(this);
}

/**
  Deinitialize THD.
*/
Auto_THD::~Auto_THD()
{
  thd->pop_internal_handler();
  destroy_thd(thd);
}

/**
  Error handler that prints error message on to the error log.

  @param thd       Current THD.
  @param sql_errno Error id.
  @param sqlstate  State of the SQL error.
  @param level     Error level.
  @param msg       Message to be reported.

  @return This function always return false.
*/
bool Auto_THD::handle_condition(THD *thd MY_ATTRIBUTE((unused)),
  uint sql_errno MY_ATTRIBUTE((unused)),
  const char* sqlstate MY_ATTRIBUTE((unused)),
  Sql_condition::enum_severity_level *level MY_ATTRIBUTE((unused)),
  const char* msg)
{
  sql_print_error("%s", msg);
  return false;
}
