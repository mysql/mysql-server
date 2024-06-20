/*
 * Copyright (c) 2024, Oracle and/or its affiliates.
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
 * This program is distributed in the hope that it will be useful,  but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
 * the GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "mrs/database/duality_view/json_input.h"
#include "mrs/database/duality_view/errors.h"

namespace mrs {
namespace database {
namespace dv {

JSONInputObject make_input_object(const JSONInputArray::ValueReference &ref,
                                  const std::string &table,
                                  const std::string &field) {
  if (ref.has_new() && ref.has_old()) {
    if (!ref.new_value().IsObject() || !ref.old_value().IsObject())
      throw_invalid_type(table, field);

    return JSONInputObject(ref.new_value(), ref.old_value());
  } else if (ref.has_new()) {
    if (!ref.new_value().IsObject()) throw_invalid_type(table, field);

    return JSONInputObject(ref.new_value());
  } else if (ref.has_old()) {
    if (!ref.old_value().IsObject()) throw_invalid_type(table, field);

    return JSONInputObject(nullptr, ref.old_value());
  } else {
    return JSONInputObject();
  }
}

JSONInputArray make_input_array(const JSONInputObject::MemberReference &ref,
                                const std::string &table,
                                const std::string &field) {
  if (ref.has_new() && ref.has_old()) {
    if (!ref.new_value().IsArray() || !ref.old_value().IsArray())
      throw_invalid_type(table, field);

    return JSONInputArray(ref.new_value(), ref.old_value());
  } else if (ref.has_new()) {
    if (!ref.new_value().IsArray()) throw_invalid_type(table, field);

    return JSONInputArray(ref.new_value());
  } else if (ref.has_old()) {
    if (!ref.old_value().IsArray()) throw_invalid_type(table, field);

    return JSONInputArray(nullptr, ref.old_value());
  } else {
    return JSONInputArray();
  }
}

JSONInputObject make_input_object(const JSONInputObject::MemberReference &ref,
                                  const std::string &table,
                                  const std::string &field) {
  if (ref.has_new() && ref.has_old()) {
    if (!ref.new_value().IsObject() || !ref.old_value().IsObject())
      throw_invalid_type(table, field);

    return JSONInputObject(ref.new_value().GetObject(),
                           ref.old_value().GetObject());
  } else if (ref.has_new()) {
    if (!ref.new_value().IsObject()) throw_invalid_type(table, field);

    return JSONInputObject(ref.new_value().GetObject());
  } else if (ref.has_old()) {
    if (!ref.old_value().IsObject()) throw_invalid_type(table, field);

    return JSONInputObject(nullptr, ref.old_value().GetObject());
  } else {
    return JSONInputObject();
  }
}

}  // namespace dv
}  // namespace database
}  // namespace mrs
