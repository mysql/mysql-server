/*
 * Copyright (c) 2019, 2024, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is designed to work with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have either included with
 * the program or referenced in the documentation.
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
#ifndef PLUGIN_X_CLIENT_VALIDATOR_DESCRIPTOR_H_
#define PLUGIN_X_CLIENT_VALIDATOR_DESCRIPTOR_H_

#include <cassert>
#include <memory>
#include <string>
#include <utility>

#include "plugin/x/client/mysqlxclient/mysqlxclient_error.h"
#include "plugin/x/client/mysqlxclient/xerror.h"
#include "plugin/x/client/validator/value_validator.h"

namespace xcl {

class Descriptor {
 public:
  Descriptor() = default;
  explicit Descriptor(Validator *validator) : m_validator(validator) {}
  Descriptor(Descriptor &&descpriptor) = default;
  virtual ~Descriptor() = default;

  template <typename Value_type>
  XError is_valid(void *context, const Value_type &value) {
    Argument_value argument_value{value};

    if (!m_validator || !m_validator->valid_type(argument_value))
      return get_supported_error();

    if (!m_validator->valid_value(argument_value))
      return get_wrong_value_error(argument_value);

    m_validator->store(context, argument_value);

    return {};
  }

  virtual XError get_supported_error() const = 0;
  virtual XError get_wrong_value_error(const Argument_value &value) const = 0;

 private:
  std::unique_ptr<Validator> m_validator;
};

}  // namespace xcl

#endif  // PLUGIN_X_CLIENT_VALIDATOR_DESCRIPTOR_H_
