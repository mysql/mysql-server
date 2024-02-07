/* Copyright (c) 2022, 2024, Oracle and/or its affiliates.

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

#endif /* LANGUAGE_SERVICE_GUARD */
