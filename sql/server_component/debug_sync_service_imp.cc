/* Copyright (c) 2023, 2024, Oracle and/or its affiliates.

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

#include "debug_sync_service_imp.h"

#include <cassert>
#include <list>
#include <string>

#include "mysql/components/my_service.h"
#include "sql/current_thd.h"
#include "sql/debug_sync.h" /* debug_sync */
#include "sql/mysqld.h"     /* srv_registry */

#if !defined(NDEBUG)

DEFINE_METHOD(void, mysql_debug_sync_service_imp::debug_sync,
              (MYSQL_THD thd, const char *name)) {
  /*
    TODO:
    We can probably use macro DEBUG_SYNC(thd, name) in sql/debug_sync.h, however
    it seems STRING_WITH_LEN(_sync_point_name_) in there uses sizeof() for
    finding length of name. I observer that the length calculated is not correct
    in some cases.  This is probably because, compiler finds the length of const
    char* using sizeof() creating it as char array. And this is probably not
    possible when we invoke via component service ?
  */
  if (unlikely(opt_debug_sync_timeout)) {
    ::debug_sync(thd, name, strlen(name));
  }
}

#endif