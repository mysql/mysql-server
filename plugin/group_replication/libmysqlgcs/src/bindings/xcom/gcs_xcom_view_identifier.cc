/* Copyright (c) 2015, 2023, Oracle and/or its affiliates.

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
    : m_fixed_part(0), m_monotonic_part(0), m_representation() {
  init(fixed_part_arg, monotonic_part_arg);
}

void Gcs_xcom_view_identifier::init(uint64_t fixed_part_arg,
                                    uint32_t monotonic_part_arg) {
  m_fixed_part = fixed_part_arg;
  m_monotonic_part = monotonic_part_arg;

  std::ostringstream builder;

  builder << m_fixed_part << ":" << m_monotonic_part;

  m_representation = builder.str();
}

void Gcs_xcom_view_identifier::increment_by_one() {
  m_monotonic_part += 1;
  init(m_fixed_part, m_monotonic_part);
}

const std::string &Gcs_xcom_view_identifier::get_representation() const {
  return m_representation;
}

Gcs_view_identifier *Gcs_xcom_view_identifier::clone() const {
  return new Gcs_xcom_view_identifier(*this);
}

bool Gcs_xcom_view_identifier::equals(const Gcs_view_identifier &other) const {
  // Static cast is safe because of the typeid safeguard in the base class
  const Gcs_xcom_view_identifier &cast_other =
      static_cast<const Gcs_xcom_view_identifier &>(other);
  return m_fixed_part == cast_other.m_fixed_part &&
         m_monotonic_part == cast_other.m_monotonic_part;
}

bool Gcs_xcom_view_identifier::lessThan(
    const Gcs_view_identifier &other) const {
  // Static cast is safe because of the typeid safeguard in the base class
  const Gcs_xcom_view_identifier &cast_other =
      static_cast<const Gcs_xcom_view_identifier &>(other);
  return (m_fixed_part == cast_other.m_fixed_part
              ? m_monotonic_part < cast_other.m_monotonic_part
              : m_fixed_part < cast_other.m_fixed_part);
}
