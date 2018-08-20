/* Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_xcom_view_identifier.h"

#include <sstream>

Gcs_xcom_view_identifier::Gcs_xcom_view_identifier(uint64_t fixed_part_arg,
                                                   uint32_t monotonic_part_arg)
    : fixed_part(0), monotonic_part(0), representation() {
  init(fixed_part_arg, monotonic_part_arg);
}

void Gcs_xcom_view_identifier::init(uint64_t fixed_part_arg,
                                    uint32_t monotonic_part_arg) {
  fixed_part = fixed_part_arg;
  monotonic_part = monotonic_part_arg;

  std::ostringstream builder;

  builder << fixed_part << ":" << monotonic_part;

  representation = builder.str();
}

void Gcs_xcom_view_identifier::increment_by_one() {
  monotonic_part += 1;
  init(fixed_part, monotonic_part);
}

const std::string &Gcs_xcom_view_identifier::get_representation() const {
  return representation;
}

Gcs_view_identifier *Gcs_xcom_view_identifier::clone() const {
  return new Gcs_xcom_view_identifier(*this);
}
