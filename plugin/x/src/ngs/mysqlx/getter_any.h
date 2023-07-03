/*
 * Copyright (c) 2015, 2022, Oracle and/or its affiliates.
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

#ifndef PLUGIN_X_SRC_NGS_MYSQLX_GETTER_ANY_H_
#define PLUGIN_X_SRC_NGS_MYSQLX_GETTER_ANY_H_

#include <sstream>
#include <string>
#include <vector>

#include "plugin/x/src/ngs/error_code.h"
#include "plugin/x/src/ngs/protocol/protocol_protobuf.h"
#include "plugin/x/src/xpl_error.h"

namespace ngs {

class Getter_any {
 public:
  template <typename Value_type>
  static Value_type get_numeric_value(const ::Mysqlx::Datatypes::Any &any) {
    using ::Mysqlx::Datatypes::Any;
    using ::Mysqlx::Datatypes::Scalar;

    if (Any::SCALAR != any.type())
      throw Error_code(ER_X_INVALID_PROTOCOL_DATA,
                       "Invalid data, expecting scalar");

    const Scalar &scalar = any.scalar();

    switch (scalar.type()) {
      case Scalar::V_BOOL:
        return static_cast<Value_type>(scalar.v_bool());

      case Scalar::V_DOUBLE:
        return static_cast<Value_type>(scalar.v_double());

      case Scalar::V_FLOAT:
        return static_cast<Value_type>(scalar.v_float());

      case Scalar::V_SINT:
        return static_cast<Value_type>(scalar.v_signed_int());

      case Scalar::V_UINT:
        return static_cast<Value_type>(scalar.v_unsigned_int());

      default:
        throw Error_code(ER_X_INVALID_PROTOCOL_DATA,
                         "Invalid data, expected numeric type");
    }
  }

  template <typename Value_type>
  static Value_type get_numeric_value(const ::Mysqlx::Datatypes::Any &any,
                                      ngs::Error_code *out_error) {
    try {
      return get_numeric_value<Value_type>(any);
    } catch (const Error_code &e) {
      if (out_error) *out_error = e;
    }

    return {};
  }

  static std::string get_string_value(const ::Mysqlx::Datatypes::Any &any,
                                      ngs::Error_code *out_error = nullptr) {
    using ::Mysqlx::Datatypes::Any;
    using ::Mysqlx::Datatypes::Scalar;

    if (Any::SCALAR != any.type()) {
      ngs::Error_code error(ER_X_INVALID_PROTOCOL_DATA,
                            "Invalid data, expecting scalar");

      if (out_error) {
        *out_error = error;
        return {};
      }

      throw error;
    }

    const Scalar &scalar = any.scalar();

    switch (scalar.type()) {
      case Scalar::V_STRING:
        return scalar.v_string().value();

      case Scalar::V_OCTETS:
        return scalar.v_octets().value();

      default: {
        ngs::Error_code error(ER_X_INVALID_PROTOCOL_DATA,
                              "Invalid data, expected string type");

        if (out_error) {
          *out_error = error;
          return {};
        }

        throw error;
      }
    }
  }

  template <typename Value_type>
  static Value_type get_numeric_value_or_default(
      const ::Mysqlx::Datatypes::Any &any, const Value_type &default_value) {
    try {
      return get_numeric_value<Value_type>(any);
    } catch (const Error_code &) {
    }

    return default_value;
  }

  template <typename Functor>
  static void put_scalar_value_to_functor(const ::Mysqlx::Datatypes::Any &any,
                                          Functor &functor) {
    if (!any.has_type())
      throw Error_code(ER_X_INVALID_PROTOCOL_DATA,
                       "Invalid data, expecting type");

    if (::Mysqlx::Datatypes::Any::SCALAR != any.type())
      throw Error_code(ER_X_INVALID_PROTOCOL_DATA,
                       "Invalid data, expecting scalar");

    using ::Mysqlx::Datatypes::Scalar;
    const Scalar &scalar = any.scalar();

    switch (scalar.type()) {
      case Scalar::V_SINT:
        throw_invalid_type_if_false(scalar, scalar.has_v_signed_int());
        functor(scalar.v_signed_int());
        break;

      case Scalar::V_UINT:
        throw_invalid_type_if_false(scalar, scalar.has_v_unsigned_int());
        functor(scalar.v_unsigned_int());
        break;

      case Scalar::V_NULL:
        functor();
        break;

      case Scalar::V_OCTETS:
        throw_invalid_type_if_false(
            scalar, scalar.has_v_octets() && scalar.v_octets().has_value());
        functor(scalar.v_octets().value(), scalar.v_octets().content_type());
        break;

      case Scalar::V_DOUBLE:
        throw_invalid_type_if_false(scalar, scalar.has_v_double());
        functor(scalar.v_double());
        break;

      case Scalar::V_FLOAT:
        throw_invalid_type_if_false(scalar, scalar.has_v_float());
        functor(scalar.v_float());
        break;

      case Scalar::V_BOOL:
        throw_invalid_type_if_false(scalar, scalar.has_v_bool());
        functor(scalar.v_bool());
        break;

      case Scalar::V_STRING:
        // XXX
        // implement char-set handling
        const bool is_valid =
            scalar.has_v_string() && scalar.v_string().has_value();

        throw_invalid_type_if_false(scalar, is_valid);
        functor(scalar.v_string().value());
        break;
    }
  }

 private:
  static void throw_invalid_type_if_false(
      const ::Mysqlx::Datatypes::Scalar &scalar, const bool is_valid) {
    if (!is_valid)
      throw Error(ER_X_INVALID_PROTOCOL_DATA,
                  "Missing field required for ScalarType: %d", scalar.type());
  }
};

}  // namespace ngs

#endif  // PLUGIN_X_SRC_NGS_MYSQLX_GETTER_ANY_H_
