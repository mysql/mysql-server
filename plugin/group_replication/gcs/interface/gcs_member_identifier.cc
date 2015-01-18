/* Copyright (c) 2014, 2015, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#include "gcs_member_identifier.h"

Gcs_member_identifier::Gcs_member_identifier(string member_id)
{
  this->member_id.clear();
  this->member_id.append(member_id.c_str());
}

Gcs_member_identifier::~Gcs_member_identifier()
{
  this->member_id.clear();
}

string* Gcs_member_identifier::get_member_id()
{
  return &this->member_id;
}

bool Gcs_member_identifier::operator<(const Gcs_member_identifier& other) const
{
  return this->member_id.compare(other.member_id) < 0;
}

bool Gcs_member_identifier::operator==(const Gcs_member_identifier& other) const
{
  return this->member_id.compare(other.member_id) == 0;
}
