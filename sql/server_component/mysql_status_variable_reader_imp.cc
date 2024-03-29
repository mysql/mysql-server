/* Copyright (c) 2022, 2023, Oracle and/or its affiliates.

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

#include "mysql_status_variable_reader_imp.h"
#include <mysql/components/minimal_chassis.h>  // mysql_components_handle_std_exception
#include <sql/sql_show.h>
#include "mysql/components/services/log_builtins.h"  // LogErr
#include "storing_auto_thd.h"

/**
  Gets the string value of a global status variable by name

  This is the implementation of mysql_status_variable_reader::get
  service method.
*/
DEFINE_BOOL_METHOD(mysql_status_variable_reader_imp::get,
                   (MYSQL_THD hthd, const char *name, bool get_global,
                    my_h_string *out_string)) {
  try {
    char buf[SHOW_VAR_FUNC_BUFF_SIZE + 1];
    size_t length = sizeof(buf);
    const CHARSET_INFO *cs = nullptr;
    THD *thd;
    std::unique_ptr<Storing_auto_THD> athd = nullptr;

    /* Use either the THD provided or create a temporary one */
    if (hthd)
      thd = static_cast<THD *>(hthd);
    else {
      /* A session variable update for a temporary THD has no effect
         and is not supported. */
      if (!get_global) {
        LogErr(ERROR_LEVEL, ER_TMP_SESSION_FOR_VAR, name);
        return true;
      }
      athd.reset(new Storing_auto_THD());
      thd = athd->get_THD();
    }

    if (get_recursive_status_var(thd, name, buf,
                                 get_global ? OPT_GLOBAL : OPT_SESSION, &length,
                                 &cs)) {
      String *res = new String[1];

      if (!cs || res->copy(buf, length, cs)) return true;
      *out_string = (my_h_string)res;
      return false;
    }
  } catch (...) {
    mysql_components_handle_std_exception(__func__);
  }
  return true;
}
