/*
  Copyright (c) 2022, 2026, Oracle and/or its affiliates.

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
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "tm_propagation.h"
#include "tm_required_services.h"

namespace telemetry {

QueryAttributeTextMapCarrier::QueryAttributeTextMapCarrier(MYSQL_THD thd)
    : m_thd(thd) {
  /* trace_parent + trace_state */
  m_seen = new std::vector<my_h_string>(2);
}

QueryAttributeTextMapCarrier::~QueryAttributeTextMapCarrier() {
  for (auto *attr_value : *m_seen) {
    string_factory_srv->destroy(attr_value);
  }

  delete m_seen;
}

opentelemetry::nostd::string_view QueryAttributeTextMapCarrier::Get(
    opentelemetry::nostd::string_view key) const noexcept {
  mysqlh_query_attributes_iterator iterator = nullptr;
  opentelemetry::nostd::string_view value("");

  std::string null_terminated_key(key.data(), key.length());
  int rc = qa_iter_srv->create(m_thd, null_terminated_key.data(), &iterator);

  if (rc != 0) {
    /* Not found. */
    return value;
  }

  enum enum_field_types attr_type = MYSQL_TYPE_NULL;
  my_h_string attr_value = nullptr;
  const char *str_data = nullptr;
  size_t str_length = 0;
  CHARSET_INFO_h charset = nullptr;

  /*
    Don't use the returned status.
    mysql_query_attributes_imp::get_type() returns true,
    which indicates an error.
  */

  (void)qa_iter_srv->get_type(iterator, &attr_type);

  if (attr_type != MYSQL_TYPE_STRING) {
    goto done;
  }

  rc = qa_string_srv->get(iterator, &attr_value);

  if (rc != 0) {
    goto done;
  }

  rc = string_get_data_srv->get_data(attr_value, &str_data, &str_length,
                                     &charset);

  if (rc != 0) {
    goto done;
  }

  /*
    The attribute value, (str_data, str_length),
    is memory owned by the attr_value object.

    The QueryAttributeTextMapCarrier::Get() method
    __must__ return a nostd::string_view,
    meaning it returns memory that __points__ to somewhere,
    owned by something else.

    This method does not return a std::string,
    so we can not give memory ownership to the caller.

    Hence, we __must__ keep the attribute value in a safe place,
    for the life cycle of this carrier object.
  */

  m_seen->push_back(attr_value);
  attr_value = nullptr;

  /*
    Now that attr_value is kept in m_seen,
    it is safe to return a string_view on it.
  */
  value = opentelemetry::nostd::string_view(str_data, str_length);

done:
  qa_iter_srv->release(iterator);
  if (attr_value != nullptr) {
    string_factory_srv->destroy(attr_value);
  }
  return value;
}

void QueryAttributeTextMapCarrier::Set(
    opentelemetry::nostd::string_view /* key */,
    opentelemetry::nostd::string_view /* value */) noexcept {
  assert(false);
}

bool QueryAttributeTextMapCarrier::Keys(
    opentelemetry::nostd::function_ref<bool(opentelemetry::nostd::string_view)>
    /* callback */) const noexcept {
  assert(false);
  return false;
}

}  // namespace telemetry
