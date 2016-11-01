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

#include "gcs_group_identifier.h"

Gcs_group_identifier::Gcs_group_identifier(const std::string &gid)
  : group_id(gid)
{
}


const std::string &Gcs_group_identifier::get_group_id() const
{
  return group_id;
}

/* purecov: begin deadcode */
bool Gcs_group_identifier::operator<(const Gcs_group_identifier &other) const
{
  return group_id.compare(other.group_id) < 0;
}


bool Gcs_group_identifier::operator==(const Gcs_group_identifier &other) const
{
  return group_id.compare(other.group_id) == 0;
}
/* purecov: end */