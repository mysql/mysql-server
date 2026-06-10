/* Copyright (c) 2025, 2026, Oracle and/or its affiliates.

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

#ifndef MYSQL_LIBRARY_EXT_H
#define MYSQL_LIBRARY_EXT_H

#include <mysql/components/service.h>
#include "defs/mysql_string_defs.h"

DEFINE_SERVICE_HANDLE(my_h_library);

/**
  Services for reading the properties of the Libraries that are imported
      by the routines.

  How to use:
    my_h_library library_handle;
    library->init(nullptr, schema, name, version, &library_handle);
    mysql_cstring_with_length body;
    bool is_binary;
    library_ext->get_body(library_handle, &is_binary);
    // ... use the body throughout the caller.
    library->deinit(library_handle);
*/
BEGIN_SERVICE_DEFINITION(mysql_library_ext)

/**
  Get library's body.

  @param [in]  library_handle   Handle to library.
  @param [out] body             Library's body.
  @param [out] is_binary        Is the library body stored with a binary
  character set?

  @note: The returned Library's body is valid only while the library_handle
        is valid.

  @returns Status of get operation:
  @retval  false Success.
  @retval  true  Failure.
*/
DECLARE_BOOL_METHOD(get_body,
                    (my_h_library library_handle,
                     mysql_cstring_with_length *body, bool *is_binary));

END_SERVICE_DEFINITION(mysql_library_ext)

#endif /* MYSQL_LIBRARY_EXT_H */
