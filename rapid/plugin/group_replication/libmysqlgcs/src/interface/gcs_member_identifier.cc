/* Copyright (c) 2014, 2016, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "gcs_member_identifier.h"

Gcs_member_identifier::Gcs_member_identifier(const std::string &id):
  member_id(id)
{
}


const std::string &Gcs_member_identifier::get_member_id() const
{
  return member_id;
}


bool Gcs_member_identifier::operator<(const Gcs_member_identifier &other) const
{
  return member_id.compare(other.member_id) < 0;
}


bool Gcs_member_identifier::operator==(const Gcs_member_identifier &other) const
{
  return member_id.compare(other.member_id) == 0;
}
