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

/**
  @file sql/server_component/mysql_global_variable_attributes_service_imp.cc
  The server implementation of system variable attributes service.
*/

#include "mysql_global_variable_attributes_service_imp.h"

#include <my_systime.h>  // my_micro_time_to_timeval
#include <my_time.h>
#include <mysql/components/services/mysql_global_variable_attributes_service.h>
#include <sql/set_var.h>
#include <sql/sql_class.h>
#include <sql/tztime.h>
#include <map>
#include <string>

/* clang-format off */
/**

  @page PAGE_MYSQL_GLOBAL_VARIABLE_ATTRIBUTES_SERVICE System variable attribute services
  Performance Schema system variable attribute services provide a way for
  plugins/components to set or query key/value attributes attached to the global system variables.

  @subpage SERVER_GLOBAL_VARIABLE_ATTRIBUTES_SERVICE_INTRODUCTION

  @subpage SERVER_GLOBAL_VARIABLE_ATTRIBUTES_SERVICE_INTERFACE

  @page SERVER_GLOBAL_VARIABLE_ATTRIBUTES_SERVICE_INTRODUCTION Services Introduction
  The service <i>mysql_global_variable_attributes</i> exposes the set of methods to:\n
  - read value of an attribute assigned to a given GLOBAL system variable, if such attribute exists
  - assign an attribute to a given variable
  - remove one or all attributes of a given variable
  - helper methods to read timestamp and user name related to last system variable change

  Another service <i>mysql_global_variable_attributes_iterator</i> supports dynamic
  attribute data discoverability by giving an ability to iterate all attributes of
  a given GLOBAL system variable.

  As an alternative to this interface, the same data is being exported in the following table
  within the performance_schema database:
   - global_variable_attributes

  @page SERVER_GLOBAL_VARIABLE_ATTRIBUTES_SERVICE_INTERFACE Service Interface

  Service <i>mysql_global_variable_attributes</i> exposes the following methods to read/write attributes:
  - @c get : get attribute value, given an attribute name and a name of global system variable
  - @c set : attach a single attribute to a global system variable, alternatively delete one or all attributes of this variable
  - @c get_time : get timestamp of the last system variable change
  - @c get_user : get user name related to the last system variable change

  Service <i>mysql_global_variable_attributes_iterator</i> exposes the following methods to discover attached atributes using an attribute iterator
  - @c create : create attribute iterator (on success points to 1st attribute)
  - @c destroy : destroy attribute iterator
  - @c advance : advance attribute iterator to point to next item (if exists)
  - @c get_name : get name of the attribute, given an iterator pointing to it
  - @c get_value : get value of the attribute, given an iterator

*/
/* clang-format on */

class global_variable_attributes_iterator_internal {
 private:
  // current position
  uint m_idx{0};
  std::vector<std::pair<std::string, std::string>> m_attributes;
  const char *m_attribute_name{nullptr};

 public:
  /**
    Create global system variable attributes iterator.
    Sets the iterator to the first matching element (if any) or at eof.

    @param variable_base Variable prefix, NULL if none.
    @param variable_name System variable name to be matched.
    @param attribute_name Attribute name to be matched or NULL to iterate all
    attributes of a variable
    @retval false : found
    @retval true : not found or error initializing
  */
  bool init(const char *variable_base, const char *variable_name,
            const char *attribute_name) {
    if (get_global_variable_attributes(variable_base, variable_name,
                                       m_attributes))
      return true;

    if (m_attributes.empty()) return true;
    if (attribute_name != nullptr && *attribute_name != '\0') {
      m_attribute_name = attribute_name;
      auto f = [attribute_name](const std::pair<std::string, std::string> &x) {
        return x.first == attribute_name;
      };
      const auto it = std::find_if(m_attributes.begin(), m_attributes.end(), f);
      if (it == m_attributes.end()) return true;
      m_idx = std::distance(m_attributes.begin(), it);
    } else
      m_idx = 0;
    return false;
  }

  /**
    Advance iterator to next element.
    Sets the iterator to the next matching element (if any) or at eof.

    @retval false : found
    @retval true : not found
  */
  bool next() {
    if (m_attribute_name != nullptr && *m_attribute_name != '\0') return true;
    m_idx++;
    return (m_idx >= m_attributes.size());
  }

  /**
      Return key/value attribute element currently pointed by the iterator.

      @retval non NULL : found
      @retval NULL : not found
    */
  std::pair<std::string, std::string> *get_current() {
    if (m_idx >= m_attributes.size()) return nullptr;
    return &m_attributes[m_idx];
  }
};

static bool imp_global_variable_attributes_iterator_create(
    const char *variable_base, const char *variable_name,
    const char *attribute_name,
    global_variable_attributes_iterator *out_iterator) {
  std::unique_ptr<global_variable_attributes_iterator_internal> iter(
      new global_variable_attributes_iterator_internal());
  if (iter->init(variable_base, variable_name, attribute_name)) {
    return true;
  }
  *out_iterator =
      reinterpret_cast<global_variable_attributes_iterator>(iter.release());
  return false;
}

static bool imp_global_variable_attributes_iterator_destroy(
    global_variable_attributes_iterator iterator) {
  auto *iter_ptr =
      reinterpret_cast<global_variable_attributes_iterator_internal *>(
          iterator);
  assert(iter_ptr != nullptr);
  delete iter_ptr;
  return false;
}

static bool imp_global_variable_attributes_iterator_next(
    global_variable_attributes_iterator iterator) {
  auto *iter_ptr =
      reinterpret_cast<global_variable_attributes_iterator_internal *>(
          iterator);
  assert(iter_ptr != nullptr);

  const bool res = iter_ptr->next();
  return res;
}

static bool imp_global_variable_attributes_iterator_get_name(
    global_variable_attributes_iterator iterator,
    my_h_string *out_name_handle) {
  auto *iter_ptr =
      reinterpret_cast<global_variable_attributes_iterator_internal *>(
          iterator);
  assert(iter_ptr != nullptr);

  const std::pair<std::string, std::string> *attribute =
      iter_ptr->get_current();
  assert(attribute != nullptr);

  if (attribute == nullptr) {
    return true;
  }
  auto *val = new String[1];
  val->set(attribute->first.c_str(), attribute->first.size(), &my_charset_bin);

  *out_name_handle = reinterpret_cast<my_h_string>(val);
  return false;
}

static bool imp_global_variable_attributes_iterator_get_value(
    global_variable_attributes_iterator iterator,
    my_h_string *out_value_handle) {
  auto *iter_ptr =
      reinterpret_cast<global_variable_attributes_iterator_internal *>(
          iterator);
  assert(iter_ptr != nullptr);

  const std::pair<std::string, std::string> *attribute =
      iter_ptr->get_current();
  assert(attribute != nullptr);

  if (attribute == nullptr) {
    return true;
  }
  auto *val = new String[1];
  val->set(attribute->second.c_str(), attribute->second.size(),
           &my_charset_bin);

  *out_value_handle = reinterpret_cast<my_h_string>(val);
  return false;
}

static bool imp_global_variable_attributes_set(const char *variable_base,
                                               const char *variable_name,
                                               const char *attribute_name,
                                               const char *attribute_value) {
  return set_global_variable_attribute(variable_base, variable_name,
                                       attribute_name, attribute_value);
}

static bool imp_global_variable_attributes_get(
    const char *variable_base, const char *variable_name,
    const char *attribute_name, char *attribute_value_buffer,
    size_t *inout_attribute_value_length) {
  std::string value;
  if (get_global_variable_attribute(variable_base, variable_name,
                                    attribute_name, value))
    return true;

  const size_t len = std::min(*inout_attribute_value_length, value.size());
  if (len > 0) strncpy(attribute_value_buffer, value.c_str(), len);
  *inout_attribute_value_length = len;
  return false;
}

static bool imp_global_variable_attributes_get_time(
    const char *variable_base, const char *variable_name,
    char *timestamp_value_buffer [[maybe_unused]],
    size_t *inout_timestamp_value_length) {
  // example result "2024-01-29 04:46:44.009907" (local time with 6 decimals of
  // time fraction)
  if (inout_timestamp_value_length == nullptr ||
      *inout_timestamp_value_length < 27)
    return true;

  ulonglong timestamp_usec = 0;

  const System_variable_tracker var_tracker =
      System_variable_tracker::make_tracker(
          variable_base == nullptr ? "" : variable_base, variable_name);
  auto f = [&](const System_variable_tracker &, sys_var *var) -> int {
    timestamp_usec = var->get_timestamp();
    return 0;
  };
  const int ret = var_tracker
                      .access_system_variable<int>(current_thd, f,
                                                   Suppress_not_found_error::NO)
                      .value_or(-1);

  // format timestamp to string, format identical to SET_TIME from
  // performance_schema.variables_info
  my_timeval tm{};
  my_micro_time_to_timeval(timestamp_usec, &tm);
  MYSQL_TIME mt{};
  current_thd->variables.time_zone->gmt_sec_to_TIME(&mt, tm);
  current_thd->time_zone_used = true;
  my_datetime_to_str(mt, timestamp_value_buffer, 6);

  *inout_timestamp_value_length = strlen(timestamp_value_buffer);

  return (ret != 0);
}

static bool imp_global_variable_attributes_get_user(
    const char *variable_base, const char *variable_name,
    char *user_value_buffer, size_t *inout_user_value_length) {
  if (inout_user_value_length == nullptr ||
      *inout_user_value_length < (USERNAME_CHAR_LENGTH + 1))
    return true;

  const System_variable_tracker var_tracker =
      System_variable_tracker::make_tracker(
          variable_base == nullptr ? "" : variable_base, variable_name);
  auto f = [&](const System_variable_tracker &, sys_var *var) -> int {
    memcpy(user_value_buffer, var->get_user(), USERNAME_CHAR_LENGTH + 1);
    *inout_user_value_length = strlen(user_value_buffer);
    return 0;
  };
  const int ret = var_tracker
                      .access_system_variable<int>(current_thd, f,
                                                   Suppress_not_found_error::NO)
                      .value_or(-1);
  return (ret != 0);
}

BEGIN_SERVICE_IMPLEMENTATION(mysql_server, mysql_global_variable_attributes)
imp_global_variable_attributes_set, imp_global_variable_attributes_get,
    imp_global_variable_attributes_get_time,
    imp_global_variable_attributes_get_user, END_SERVICE_IMPLEMENTATION();

BEGIN_SERVICE_IMPLEMENTATION(mysql_server,
                             mysql_global_variable_attributes_iterator)
imp_global_variable_attributes_iterator_create,
    imp_global_variable_attributes_iterator_destroy,
    imp_global_variable_attributes_iterator_next,
    imp_global_variable_attributes_iterator_get_name,
    imp_global_variable_attributes_iterator_get_value,
    END_SERVICE_IMPLEMENTATION();
