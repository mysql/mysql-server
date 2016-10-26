/* Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.

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

#include "gcs_xcom_view_identifier.h"

#include <sstream>

Gcs_xcom_view_identifier::
Gcs_xcom_view_identifier(uint64_t fixed_part_arg,
                         int monotonic_part_arg)
  :fixed_part(0), monotonic_part(0), representation()
{
  init(fixed_part_arg, monotonic_part_arg);
}


void Gcs_xcom_view_identifier::
init(uint64_t fixed_part_arg, int monotonic_part_arg)
{
  fixed_part=     fixed_part_arg;
  monotonic_part= monotonic_part_arg;

  std::ostringstream builder;

  builder << fixed_part << ":" << monotonic_part;

  representation= builder.str();
}


void Gcs_xcom_view_identifier::increment_by_one()
{
  monotonic_part+= 1;
  init(fixed_part, monotonic_part);
}


Gcs_xcom_view_identifier::~Gcs_xcom_view_identifier() {}


const std::string &Gcs_xcom_view_identifier::get_representation() const
{
  return representation;
}


Gcs_view_identifier *Gcs_xcom_view_identifier::clone() const
{
  return new Gcs_xcom_view_identifier(*this);
}
