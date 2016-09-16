/*
 * Copyright (c) 2015, 2016 Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */

#ifndef _NGS_GETTER_ANY_H_
#define _NGS_GETTER_ANY_H_

#include <string>
#include <vector>
#include <sstream>

#include "ngs_common/protocol_protobuf.h"
#include "ngs/ngs_error.h"
#include "ngs/error_code.h"


namespace ngs
{


class Getter_any
{
public:
  template <typename Value_type>
  static Value_type get_numeric_value(const ::Mysqlx::Datatypes::Any &any)
  {
    using namespace ::Mysqlx::Datatypes;

    if (Any::SCALAR != any.type())
      throw ngs::Error_code(ER_X_INVALID_PROTOCOL_DATA, "Invalid data, expecting scalar");

    const Scalar &scalar = any.scalar();

    switch (scalar.type())
    {
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
      throw ngs::Error_code(ER_X_INVALID_PROTOCOL_DATA, "Invalid data, expected numeric type");
    }
  }


  template <typename Value_type>
  static Value_type get_numeric_value_or_default(const ::Mysqlx::Datatypes::Any &any, const Value_type & default_value)
  {
    try
    {
      return get_numeric_value<Value_type>(any);
    }
    catch (const ngs::Error_code&)
    {
    }

    return default_value;
  }


  template <typename Functor>
  static void put_scalar_value_to_functor(const ::Mysqlx::Datatypes::Any &any, Functor & functor)
  {
    if (!any.has_type())
      throw ngs::Error_code(ER_X_INVALID_PROTOCOL_DATA, "Invalid data, expecting type");

    if (::Mysqlx::Datatypes::Any::SCALAR != any.type())
      throw ngs::Error_code(ER_X_INVALID_PROTOCOL_DATA, "Invalid data, expecting scalar");

    using ::Mysqlx::Datatypes::Scalar;
    const Scalar &scalar = any.scalar();

    switch (scalar.type())
    {
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
      throw_invalid_type_if_false(scalar, scalar.has_v_octets() && scalar.v_octets().has_value());
      functor(scalar.v_octets().value());
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
      //XXX
      // implement char-set handling
      const bool is_valid = scalar.has_v_string() && scalar.v_string().has_value();

      throw_invalid_type_if_false(scalar, is_valid);
      functor(scalar.v_string().value());
      break;
    }
  }

private:
  static void throw_invalid_type_if_false(const ::Mysqlx::Datatypes::Scalar &scalar, const bool is_valid)
  {
    if (!is_valid)
      throw ngs::Error(ER_X_INVALID_PROTOCOL_DATA,
                       "Missing field required for ScalarType: %d", scalar.type());
  }
};


} // namespace ngs


#endif // _NGS_GETTER_ANY_H_
