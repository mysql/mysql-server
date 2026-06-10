/* Copyright (c) 2024, 2026, Oracle and/or its affiliates.

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

#ifndef MYSQL_LIBRARY_IMP_H
#define MYSQL_LIBRARY_IMP_H

#include <optional>
#include <string>
#include "mysql/components/services/bits/thd.h"

#include <mysql/components/service_implementation.h>
#include <mysql/components/services/mysql_library.h>
#include <mysql/components/services/mysql_library_ext.h>

/**
  Implementation of the mysql_library_imp services.
*/
class mysql_library_imp {
 public:
  static DEFINE_BOOL_METHOD(exists,
                            (MYSQL_THD thd,
                             mysql_cstring_with_length schema_name,
                             mysql_cstring_with_length library_name,
                             mysql_cstring_with_length version, bool *result));

  static DEFINE_BOOL_METHOD(init, (MYSQL_THD thd,
                                   mysql_cstring_with_length schema_name,
                                   mysql_cstring_with_length library_name,
                                   mysql_cstring_with_length version,
                                   my_h_library *library_handle));

  static DEFINE_BOOL_METHOD(get_body, (my_h_library library_handle,
                                       mysql_cstring_with_length *body));

  static DEFINE_BOOL_METHOD(get_language,
                            (my_h_library library_handle,
                             mysql_cstring_with_length *language));

  static DEFINE_BOOL_METHOD(deinit, (my_h_library library_handle));
};

/**
  Implementation of the mysql_library_ext_imp services.
*/
class mysql_library_ext_imp {
 public:
  static DEFINE_BOOL_METHOD(get_body,
                            (my_h_library library_handle,
                             mysql_cstring_with_length *body, bool *is_binary));
};

#endif /* MYSQL_LIBRARY_IMP_H */