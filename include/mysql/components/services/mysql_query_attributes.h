/* Copyright (c) 2019, 2024, Oracle and/or its affiliates.

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

#ifndef MYSQL_QUERY_ATTRIBUTES_H
#define MYSQL_QUERY_ATTRIBUTES_H

#include <field_types.h>
#include <mysql/components/service.h>
#include <mysql/components/services/mysql_string.h>

#ifdef __cplusplus
class THD;
#define MYSQL_THD THD *
#else
#define MYSQL_THD void *
#endif

DEFINE_SERVICE_HANDLE(mysqlh_query_attributes_iterator);

/**
  @ingroup group_components_services_inventory

  A service to fetch the query attributes for the current thread

  Use in conjunction with all the related services that operate on thread ids
  @sa mysql_component_mysql_query_attributes_imp
*/
BEGIN_SERVICE_DEFINITION(mysql_query_attributes_iterator)
/**
  Creates iterator that iterates through all parameters supplied.
  The iterator will be positioned at the first parameter if any.

  @param thd The thread handle for the thread you want data for. It NULL it
             will read the current thread.
  @param name The name of the parameter to position on. UTF8mb4. if non-NULL
              will position the iterator on the first occurrence of a parameter
              with the specified name, if any.
  @param [out] out_iterator place to store the iterator handle.
  @return Status of performed operation
  @retval false success. Can read data.
  @retval true failure. Either failed to initialize iterator or no parameters
  to read.
*/
DECLARE_BOOL_METHOD(create, (MYSQL_THD thd, const char *name,
                             mysqlh_query_attributes_iterator *out_iterator));
/**
  Gets the type of element pointed to by the iterator.

  @param iterator query attributes iterator handle.
  @param [out] out_type place to store the parameter type.
  @return status of performed operation
  @retval false success
  @retval true Invalid iterator or not on an element
*/
DECLARE_BOOL_METHOD(get_type, (mysqlh_query_attributes_iterator iterator,
                               enum enum_field_types *out_type));
/**
  Advances specified iterator to next element.

  @param iterator iterator handle to advance.
  @return Status of performed operation and validity of iterator after
    operation.
  @retval false success
  @retval true Failure or no more elements
*/
DECLARE_BOOL_METHOD(next, (mysqlh_query_attributes_iterator iter));
/**
  Gets the name of the parameter.

  @param iterator query attributes iterator handle.
  @param[out] out_name_handle the name of the parameter if supplied.
              Otherwise a nullptr.
  @return status of operation
  @retval false Valid
  @retval true Invalid iterator or not on an element
*/
DECLARE_BOOL_METHOD(get_name, (mysqlh_query_attributes_iterator iterator,
                               my_h_string *out_name_handle));
/**
  Releases the Service Implementations iterator. Releases read lock on the
  Registry.

  @param iterator Service Implementation iterator handle.
  @return Status of performed operation
  @retval false success
  @retval true failure
*/
DECLARE_METHOD(void, release, (mysqlh_query_attributes_iterator iterator));
END_SERVICE_DEFINITION(mysql_query_attributes_iterator)

/**
  @ingroup group_components_services_inventory

  A service to fetch the query attribute value as a string

  Use in conjunction with mysql_query_atrributes_iterator service

  @sa mysql_component_mysql_query_attributes_imp
*/
BEGIN_SERVICE_DEFINITION(mysql_query_attribute_string)
/**
  Gets the parameter as a string

  @param iterator query attributes iterator handle.
  @param[out] out_string_value the value.
  @return status of operation
  @retval false success. out_string_value valid
  @retval true Invalid iterator or a null value
*/
DECLARE_BOOL_METHOD(get, (mysqlh_query_attributes_iterator iterator,
                          my_h_string *out_string_value));
END_SERVICE_DEFINITION(mysql_query_attribute_string)

/**
  @ingroup group_components_services_inventory

  A service to fetch the query attribute null flag

  Use in conjunction with mysql_query_atrributes_iterator service

  @sa mysql_component_mysql_query_attributes_imp
*/
BEGIN_SERVICE_DEFINITION(mysql_query_attribute_isnull)
/**
  Checks if the parameter value is a null

  @param iterator query attributes iterator handle.
  @param[out] out_null set to true if the value is NULL. false otherwise.
  @return status of operation
  @retval false success. out_null valid
  @retval true Invalid iterator no current element
*/
DECLARE_BOOL_METHOD(get, (mysqlh_query_attributes_iterator iterator,
                          bool *out_null));
END_SERVICE_DEFINITION(mysql_query_attribute_isnull)

#endif /* MYSQL_QUERY_ATTRIBUTES_H */
