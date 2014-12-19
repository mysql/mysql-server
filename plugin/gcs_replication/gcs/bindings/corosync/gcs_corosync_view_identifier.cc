/* Copyright (c) 2014, Oracle and/or its affiliates. All rights reserved.

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

#include "gcs_corosync_view_identifier.h"

#include <sstream>

Gcs_corosync_view_identifier::
Gcs_corosync_view_identifier(unsigned long int fixed_part,
                             int monotonic_part)
                     : representation(NULL)
{
  init(fixed_part, monotonic_part);
}

Gcs_corosync_view_identifier::
Gcs_corosync_view_identifier(Gcs_corosync_view_identifier &other)
                     : representation(NULL)
{
  init(other.get_fixed_part(), other.get_monotonic_part());
}

void Gcs_corosync_view_identifier::
init(unsigned long int fixed_part, int monotonic_part)
{
  this->fixed_part     = fixed_part;
  this->monotonic_part = monotonic_part;

  std::ostringstream builder;

  builder << fixed_part << ":" << monotonic_part;

  if(representation != NULL)
    delete representation;

  representation= new std::string(builder.str());
}

void Gcs_corosync_view_identifier::increment_by_one()
{
  this->monotonic_part+=1;
  init(fixed_part, monotonic_part);
}

Gcs_corosync_view_identifier::
~Gcs_corosync_view_identifier()
{
  if(representation != NULL)
    delete representation;
}

char*
Gcs_corosync_view_identifier::get_representation()
{
  return const_cast<char*>(representation->c_str());
}

