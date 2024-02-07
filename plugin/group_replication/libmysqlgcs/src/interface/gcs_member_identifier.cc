/* Copyright (c) 2014, 2024, Oracle and/or its affiliates.

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

#include <assert.h>
#include <cstring>
#include <sstream>

#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_member_identifier.h"
#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/xplatform/byteorder.h"
#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/xplatform/my_xp_util.h"

Gcs_member_identifier::Gcs_member_identifier(const std::string &id)
    : m_member_id(id) {}

const std::string &Gcs_member_identifier::get_member_id() const {
  return m_member_id;
}

bool Gcs_member_identifier::operator<(
    const Gcs_member_identifier &other) const {
  return m_member_id.compare(other.m_member_id) < 0;
}

bool Gcs_member_identifier::operator==(
    const Gcs_member_identifier &other) const {
  return m_member_id.compare(other.m_member_id) == 0;
}
