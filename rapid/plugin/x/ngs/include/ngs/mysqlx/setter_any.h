/*
 * Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.
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

#ifndef _NGS_SETTER_ANY_H_
#define _NGS_SETTER_ANY_H_

#include <string>
#include <vector>
#include "ngs_common/protocol_protobuf.h"


namespace ngs
{


class Setter_any
{
public:
  static void set_scalar(::Mysqlx::Datatypes::Scalar &scalar, const bool value)
  {
    scalar.set_type(::Mysqlx::Datatypes::Scalar::V_BOOL);
    scalar.set_v_bool(value);
  }


  static void set_scalar(::Mysqlx::Datatypes::Scalar &scalar, const int64_t value)
  {
    scalar.set_type(::Mysqlx::Datatypes::Scalar::V_SINT);
    scalar.set_v_signed_int(value);
  }


  static void set_scalar(::Mysqlx::Datatypes::Scalar &scalar, const uint64_t value)
  {
    scalar.set_type(::Mysqlx::Datatypes::Scalar::V_UINT);
    scalar.set_v_unsigned_int(value);
  }


  static void set_scalar(::Mysqlx::Datatypes::Scalar &scalar, const float value)
  {
    scalar.set_type(::Mysqlx::Datatypes::Scalar::V_FLOAT);
    scalar.set_v_float(value);
  }


  static void set_scalar(::Mysqlx::Datatypes::Scalar &scalar, const double value)
  {
    scalar.set_type(::Mysqlx::Datatypes::Scalar::V_DOUBLE);
    scalar.set_v_double(value);
  }


  static void set_scalar(::Mysqlx::Datatypes::Scalar &scalar, const char *value)
  {
    scalar.set_type(::Mysqlx::Datatypes::Scalar::V_STRING);

    scalar.set_allocated_v_string(new ::Mysqlx::Datatypes::Scalar_String());

    scalar.mutable_v_string()->set_value(value);
  }


  static void set_scalar(::Mysqlx::Datatypes::Scalar &scalar, const std::string &value)
  {
    scalar.set_type(::Mysqlx::Datatypes::Scalar::V_STRING);
    scalar.set_allocated_v_string(new ::Mysqlx::Datatypes::Scalar_String());

    scalar.mutable_v_string()->set_value(value);
  }


  template<typename ValueType>
  static void set_scalar(::Mysqlx::Datatypes::Any &any, const ValueType value)
  {
    any.set_type(::Mysqlx::Datatypes::Any::SCALAR);

    set_scalar(*any.mutable_scalar(), value);
  }

  template<typename ValueType>
  static void set_array(::Mysqlx::Datatypes::Any &any, const std::vector<ValueType> &values)
  {
    ::Mysqlx::Datatypes::Array &array = *any.mutable_array();

    any.set_type(::Mysqlx::Datatypes::Any::ARRAY);

    typename std::vector<ValueType>::const_iterator i = values.begin();

    for (; i != values.end(); ++i)
    {
      set_scalar(*array.add_value(), *i);
    }
  }
};


} // namespace ngs


#endif // _NGS_SETTER_ANY_H_
