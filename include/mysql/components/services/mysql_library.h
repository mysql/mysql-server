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

#ifndef MYSQL_LIBRARY_H
#define MYSQL_LIBRARY_H

#include <mysql/components/service.h>
#include "defs/mysql_string_defs.h"
#include "mysql/components/services/bits/thd.h"

DEFINE_SERVICE_HANDLE(my_h_library);

/**
  Services for reading the properties of the Libraries that are imported
       by the routines.

  How to use:
    my_h_library library_handle;
    library->init(nullptr, schema, name, version, &library_handle);
    mysql_cstring_with_length body;
    library->get_body(library_handle, &body);
    // ... use the body throughout the caller.
    library->deinit(library_handle);
*/
BEGIN_SERVICE_DEFINITION(mysql_library)

/**
  Checks if library exists.

  @param [in]  thd            (optional) Thread where the check will be
                              made. current_thd will be used if nullptr.
  @param [in]  schema_name   Schema where the library is stored.
  @param [in]  library_name  Name of the library.
  @param [in]  version       Version of the library.
  @param [out] result        True if library exists, false otherwise.

  @return Status of the performed operation:
  @retval false success.
  @retval true failure.
 */
DECLARE_BOOL_METHOD(exists,
                    (MYSQL_THD thd, mysql_cstring_with_length schema_name,
                     mysql_cstring_with_length library_name,
                     mysql_cstring_with_length version, bool *result));

/**
  Construct a new library object. Locks the library object with a shared
  reentrant read-lock.

  @note If the library handle provided points to non-nullptr, an error is
        reported.
  @note call deinit(library_handle) to release the lock.

  @param [in]  thd            (optional) Thread where the handle will be
                              allocated. current_thd will be used if nullptr.
  @param [in]  schema_name    Schema where the library is stored.
  @param [in]  library_name   Name of the library.
  @param [in]  version        Version of the library.
  @param [out] library_handle A handle to the library object.
                              Must be set to nullptr when calling the function.

  @return Status of the performed operation:
  @retval false success.
  @retval true failure.
 */
DECLARE_BOOL_METHOD(init, (MYSQL_THD thd, mysql_cstring_with_length schema_name,
                           mysql_cstring_with_length library_name,
                           mysql_cstring_with_length version,
                           my_h_library *library_handle));

/**
  Get library's body.

  @param [in]  library_handle   Handle to library.
  @param [out] body             Library's body.

  @note: The returned Library's body is valid only while the library_handle
         is valid.

  @returns Status of get operation:
  @retval  false Success.
  @retval  true  Failure.
*/
DECLARE_BOOL_METHOD(get_body, (my_h_library library_handle,
                               mysql_cstring_with_length *body));

/**
  Get library's language.

  @param [in]  library_handle   Handle to library.
  @param [out] language         Library's language.

  @note: The returned Library's language is valid only while the library_handle
         is valid.

  @returns Status of get operation:
  @retval  false Success.
  @retval  true  Failure.
*/
DECLARE_BOOL_METHOD(get_language, (my_h_library library_handle,
                                   mysql_cstring_with_length *language));

/**
  Clean up the resources related to a library. Release the shared read lock.

  @param [in] library_handle A handle to the library.

  @note: library_handle will no longer be valid at the end of this function.

  @return Status of the performed operation:
  @retval false success.
  @retval true failure.
 */
DECLARE_BOOL_METHOD(deinit, (my_h_library library_handle));

END_SERVICE_DEFINITION(mysql_library)

#endif /* MYSQL_LIBRARY_H */