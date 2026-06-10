/* Copyright (c) 2022, 2026, Oracle and/or its affiliates.

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

#ifndef LANGUAGE_SERVICE_GUARD
#define LANGUAGE_SERVICE_GUARD

#include "mysql/components/service.h"
#include "mysql/components/services/bits/thd.h"
#include "mysql/components/services/defs/mysql_string_defs.h"

/**
  The handle is created by the caller of
  external_program_execution service.

  It is guaranteed to be available only during the execution of
  external_program_execution service API's methods.
*/
DEFINE_SERVICE_HANDLE(external_program_handle);

/**
  The handle is an opaque pointer to a sp_head item.
*/
DEFINE_SERVICE_HANDLE(stored_program_handle);

/**
  The handle is an opaque pointer to the Stored Program's statement state.
*/
DEFINE_SERVICE_HANDLE(stored_program_statement_handle);

/**
  @ingroup group_components_services_inventory

  A service to query various properties/capabilities of the implementer of
  @ref s_mysql_external_program_execution service.
*/
BEGIN_SERVICE_DEFINITION(external_program_capability_query)

/**
  Retrieve capability information

  Supported capabilities and value type

  capability: "supports_language"
  property: "<language>"
  value: Boolean (true - Supported, false - Not supported)

  @param [in]     capability Capability name (see description above)
  @param [in,out] property   Capability's property (may be null)
                             Must be one of the capability's
                             supported properties.
  @param [out]    value      Implementation's capability/property information

  @returns Status
    @retval false Success
    @retval true  Error
*/
DECLARE_BOOL_METHOD(get, (const char *capability, char *property, void *value));

END_SERVICE_DEFINITION(external_program_capability_query)

/**
  @ingroup group_components_services_inventory

  A service to setup and execute multi-lingual stored procedures
*/
BEGIN_SERVICE_DEFINITION(external_program_execution)
/**
  Create and initialize stored program state if language is supported.

  @param [in]  sp           The stored program used for associating
                            language sp state.
  @param [in]  sp_statement The statement where this stored program
                            is created (optional).
  @param [out] lang_sp      external program pointer if created,
                            nullptr otherwise.
  @return status of initialization
    @retval false Success
    @retval true  Error
*/
DECLARE_BOOL_METHOD(init, (stored_program_handle sp,
                           stored_program_statement_handle sp_statement,
                           external_program_handle *lang_sp));

/**
  Deinits and cleans up stored program state.

  @param [in] thd     (optional) The THD this stored program was attached to.
  @param [in] lang_sp (optional) The stored program state to clean up.
  @param [in] sp      (optional) The stored program used for associating
                                 language sp state when lang_sp was created.
  @note: At least one of lang_sp or sp should be provided.
  @returns status of de-initialization
    @retval false Success
    @retval true  Error
*/
DECLARE_BOOL_METHOD(deinit, (MYSQL_THD thd, external_program_handle lang_sp,
                             stored_program_handle sp));

/**
  Parse given external program

  @param [in] lang_sp      The stored program state
  @param [in] sp_statement The statement where this stored program
                           is parsed (optional).
  @returns Status of parsing
    @retval false Success
    @retval true  Error
*/
DECLARE_BOOL_METHOD(parse, (external_program_handle lang_sp,
                            stored_program_statement_handle sp_statement));

/**
  Execute given external program

  @param [in] lang_sp      The stored program state
  @param [in] sp_statement The statement where this stored program
                           is executed (optional).
  @returns Status of execution
    @retval false Success
    @retval true  Error
*/
DECLARE_BOOL_METHOD(execute, (external_program_handle lang_sp,
                              stored_program_statement_handle sp_statement));

END_SERVICE_DEFINITION(external_program_execution)

/**
  @ingroup group_components_services_inventory

  A service to parse library code
*/
BEGIN_SERVICE_DEFINITION(external_library)

/**
  Check if the language of the library is supported

  @param [in]  language   Language of the library source code
  @param [out] result     Returns true if the language is supported

  @return Status of the performed operation
  @retval false success
  @retval true failure
*/
DECLARE_BOOL_METHOD(is_language_supported,
                    (mysql_cstring_with_length language, bool *result));

/**
  Parses library code.

  @param [in]  name       Name of the library
  @param [in]  language   Language of the library source code
  @param [in]  body       Library's source code in UTF8MB4 charset

  @return Status of the performed operation
  @retval false success
  @retval true failure
*/
DECLARE_BOOL_METHOD(parse, (mysql_cstring_with_length name,
                            mysql_cstring_with_length language,
                            mysql_cstring_with_length body));

END_SERVICE_DEFINITION(external_library)

/**
  @ingroup group_components_services_inventory

  A service to parse library code in either UTF8 or BINARY character set.
*/
BEGIN_SERVICE_DEFINITION(external_library_ext)

/**
  Parses library code.

  @param [in]  name          Name of the library.
  @param [in]  language      Language of the library source code.
  @param [in]  body          Library's source code in provided charset.
  @param [in]  is_binary     Is the library body stored with a binary character
  set?
  @param [out] result        True if the parse succeeds.
                             False if the library cannot be parsed.

  @return Status of the performed operation.
  @retval false success.
  @retval true failure.
*/
DECLARE_BOOL_METHOD(parse, (mysql_cstring_with_length name,
                            mysql_cstring_with_length language,
                            mysql_cstring_with_length body, bool is_binary,
                            bool *result));

END_SERVICE_DEFINITION(external_library_ext)

#endif /* LANGUAGE_SERVICE_GUARD */
