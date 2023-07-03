/*
 * Copyright (c) 2018, 2023, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is also distributed with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have included with MySQL.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

#include "plugin/x/src/capabilities/handler_connection_attributes.h"

#include <algorithm>
#include <string>
#include <utility>

#include "include/mysql_com.h"
#include "mysql/psi/mysql_thread.h"
#include "plugin/x/src/mysql_variables.h"
#include "plugin/x/src/xpl_log.h"

namespace xpl {

void Capability_connection_attributes::get_impl(::Mysqlx::Datatypes::Any *any) {
  assert(false && "This method should not be used with CapGet");
}

ngs::Error_code Capability_connection_attributes::set_impl(
    const ::Mysqlx::Datatypes::Any &any) {
  if (!any.has_obj() || any.obj().fld_size() == 0) {
    log_capability_corrupted();
    return ngs::Error(ER_X_CAPABILITIES_PREPARE_FAILED,
                      "Capability prepare failed for '%s'", name().c_str());
  }
  const auto fields = any.obj().fld();
  for (const auto &field : fields) {
    auto validation_error = validate_field(field);
    if (validation_error) return validation_error;

    const auto &value = field.value().scalar().v_string().value();
    const auto &key = field.key();
    m_attributes.emplace_back(std::make_pair(key, value));
    m_attributes_length += key.size() + value.size() +
                           net_length_size(key.size()) +
                           net_length_size(value.size());
  }

  if (m_attributes_length > k_max_buffer_size) {
    log_size_exceeded("buffer", m_attributes_length, k_max_buffer_size);
    return ngs::Error(ER_X_BAD_CONNECTION_SESSION_ATTRIBUTE_LENGTH,
                      "There are too many bytes in connection session"
                      " attributes the capability is limited to %u",
                      static_cast<unsigned>(k_max_buffer_size));
  }
  return {};
}

void Capability_connection_attributes::commit() {
  auto buffer = create_buffer();

#ifdef HAVE_PSI_THREAD_INTERFACE
  const auto bytes_lost = PSI_THREAD_CALL(set_thread_connect_attrs)(
      reinterpret_cast<char *>(buffer.data()), buffer.size(),
      mysqld::get_default_charset());

  if (bytes_lost != 0)
    log_debug(
        "Capability session connect attributes commit failed, %u bytes lost",
        static_cast<unsigned>(bytes_lost));
#endif  // HAVE_PSI_THREAD_INTERFACE

  m_attributes.clear();
  m_attributes_length = 0;
}

std::vector<unsigned char> Capability_connection_attributes::create_buffer() {
  std::vector<unsigned char> result(m_attributes_length);

  auto ptr = result.data();
  for (const auto &attribute : m_attributes) {
    const auto &key = attribute.first;
    const auto &value = attribute.second;

    assert(!key.empty());
    ptr = write_length_encoded_string(ptr, key);
    ptr = write_length_encoded_string(ptr, value);
  }
  return result;
}

void Capability_connection_attributes::log_size_exceeded(
    const char *const name, const std::size_t value,
    const std::size_t max_value) const {
  log_debug(
      "Capability session connect attributes failed, exceeded max %s size (%u"
      " bytes), current value is %u bytes long",
      name, static_cast<unsigned>(max_value), static_cast<unsigned>(value));
}

unsigned char *Capability_connection_attributes::write_length_encoded_string(
    unsigned char *buf, const std::string &string) {
  buf = net_store_length(buf, string.size());
  std::copy(std::begin(string), std::end(string), buf);
  buf += string.size();
  return buf;
}

ngs::Error_code Capability_connection_attributes::validate_field(
    const Mysqlx::Datatypes::Object_ObjectField &field) const {
  if (!field.has_value() || !field.value().has_scalar() || !field.has_key()) {
    log_capability_corrupted();
    return ngs::Error(ER_X_CAPABILITIES_PREPARE_FAILED,
                      "Capability prepare failed for '%s'", name().c_str());
  }
  if (!field.value().scalar().has_v_string() ||
      !field.value().scalar().v_string().has_value()) {
    log_capability_corrupted();
    return ngs::Error(ER_X_BAD_CONNECTION_SESSION_ATTRIBUTE_TYPE,
                      "Key and values support only string values");
  }

  const auto &value = field.value().scalar().v_string().value();
  const auto &key = field.key();
  if (key.size() > k_max_key_size) {
    log_size_exceeded("key", key.size(), k_max_key_size);
    return ngs::Error(ER_X_BAD_CONNECTION_SESSION_ATTRIBUTE_KEY_LENGTH,
                      "Key name beginning with '%s'... is too long, currently"
                      " limited to %u",
                      key.substr(0, k_max_key_size).c_str(),
                      static_cast<unsigned>(k_max_key_size));
  }
  if (value.size() > k_max_value_size) {
    log_size_exceeded("value", value.size(), k_max_value_size);
    return ngs::Error(
        ER_X_BAD_CONNECTION_SESSION_ATTRIBUTE_VALUE_LENGTH,
        "Value is too long for '%s' attribute, currently limited to %u",
        key.c_str(), static_cast<unsigned>(k_max_value_size));
  }
  if (key.size() == 0) {
    log_debug("Capability session connect attributes failed, empty key given");
    return ngs::Error(ER_X_BAD_CONNECTION_SESSION_ATTRIBUTE_EMPTY_KEY,
                      "Empty key name given");
  }
  return {};
}

void Capability_connection_attributes::log_capability_corrupted() const {
  log_debug(
      "Capability session connect attributes failed due to a corrupted"
      " capability format");
}

}  // namespace xpl
