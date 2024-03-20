/* Copyright (c) 2024, Oracle and/or its affiliates.

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

#ifndef GLOBAL_VARIABLE_ATTRIBUTES_BITS_H
#define GLOBAL_VARIABLE_ATTRIBUTES_BITS_H

#include <cstddef>  // size_t

#include <mysql/components/service.h>
#include <mysql/components/services/mysql_string.h>

DEFINE_SERVICE_HANDLE(global_variable_attributes_iterator);

/**
  Initialize System Variable Attributes iterator object
  to enumerate key/value attributes attached to system variables, pointing to
  1st matching attribute

  @param variable_base Variable prefix or NULL if none.
  @param variable_name define system variable scope
  @param attribute_name Optional attribute name scope (NULL to iterate all
  attributes of a variable)
  @param [out] iterator iterator object

  @returns Result of iterator creation
    @retval false Success
    @retval true Failure
*/
typedef bool (*global_variable_attributes_iterator_create_t)(
    const char *variable_base, const char *variable_name,
    const char *attribute_name, global_variable_attributes_iterator *iterator);

/**
  Uninitialize System Variable Attributes iterator

  @param iterator iterator object

  @returns Result of iterator creation
    @retval false Success
    @retval true Failure
*/
typedef bool (*global_variable_attributes_iterator_destroy_t)(
    global_variable_attributes_iterator iterator);

/**
  Advance System Variable Attributes iterator to next element.

  @param iterator iterator object

  @returns Result of iterator creation
    @retval false Success
    @retval true Failure or no more elements
*/
typedef bool (*global_variable_attributes_iterator_advance_t)(
    global_variable_attributes_iterator iterator);

/**
  Return attribute name for the element pointed by
  System Variable Attributes iterator.

  @param iterator iterator object
  @param[out] out_name_handle pointer to receive name string content

  @returns Result of operation
    @retval false Success
    @retval true Failure or no more elements
*/
typedef bool (*global_variable_attributes_iterator_get_name_t)(
    global_variable_attributes_iterator iterator, my_h_string *out_name_handle);

/**
  Return attribute value for the element pointed by
  System Variable Attributes iterator.

  @param iterator iterator object
  @param[out] out_value_handle pointer to receive value string content

  @returns Result of operation
    @retval false Success
    @retval true Failure or no more elements
*/
typedef bool (*global_variable_attributes_iterator_get_value_t)(
    global_variable_attributes_iterator iterator,
    my_h_string *out_value_handle);

/**
  Attach a single key/value attribute to a given global system variable,
  or delete one or all attributes of a given variable.

  @param variable_base Variable prefix, NULL if none.
  @param variable_name Variable name.
  @param attribute_name Attribute name, if nullptr unset all attributes for the
  variable.
  @param attribute_value Attribute value, if nullptr unset the attribute.

  @returns Result of operation
    @retval false Success
    @retval true Failure or no more elements
*/
typedef bool (*global_variable_attributes_assign_t)(
    const char *variable_base, const char *variable_name,
    const char *attribute_name, const char *attribute_value);

/**
  Read a single global system variable attribute value, if exists.

  @param variable_base Variable prefix, NULL if none.
  @param variable_name Variable name.
  @param attribute_name Attribute name.
  @param attribute_value_buffer Pointer to buffer to receive the result.
  @param inout_attribute_value_length Buffer size, also used to return value
  string size.

  @returns Result of operation
    @retval false Success
    @retval true Failure or no more elements
*/
typedef bool (*global_variable_attributes_get_t)(
    const char *variable_base, const char *variable_name,
    const char *attribute_name, char *attribute_value_buffer,
    size_t *inout_attribute_value_length);

/**
  Read timestamp indicating when a global system variable was last modified.

  @param variable_base Variable prefix, NULL if none.
  @param variable_name Variable name.
  @param timestamp_value_buffer Pointer to buffer to receive the result.
  @param inout_timestamp_value_length Buffer size, also used to return value
  string size.

  @returns Result of operation
    @retval false Success
    @retval true Failure or no more elements
*/
typedef bool (*global_variable_attributes_get_time_t)(
    const char *variable_base, const char *variable_name,
    char *timestamp_value_buffer, size_t *inout_timestamp_value_length);

/**
  Read user name that last modified a global system variable.

  @param variable_base Variable prefix, NULL if none.
  @param variable_name Variable name.
  @param user_value_buffer Pointer to buffer to receive the result.
  @param inout_user_value_length Buffer size, also used to return value
  string size.

  @returns Result of operation
    @retval false Success
    @retval true Failure or no more elements
*/
typedef bool (*global_variable_attributes_get_user_t)(
    const char *variable_base, const char *variable_name,
    char *user_value_buffer, size_t *inout_user_value_length);

#endif /* GLOBAL_VARIABLE_ATTRIBUTES_BITS_H */
