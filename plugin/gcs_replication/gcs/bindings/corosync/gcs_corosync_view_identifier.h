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

#ifndef GCS_COROSYNC_VIEW_IDENTIFIER_INCLUDED
#define GCS_COROSYNC_VIEW_IDENTIFIER_INCLUDED

#include "gcs_view_identifier.h"
#include "gcs_types.h"

#include <string>

class Gcs_corosync_view_identifier: public Gcs_view_identifier
{
public:
  Gcs_corosync_view_identifier(unsigned long int fixed_part,
                               int monotonic_part);

  Gcs_corosync_view_identifier(Gcs_corosync_view_identifier &other);

  virtual ~Gcs_corosync_view_identifier();

  unsigned long int get_fixed_part()
  {
    return fixed_part;
  }

  int get_monotonic_part()
  {
    return monotonic_part;
  }

  void increment_by_one();

  virtual char* get_representation();

private:
  void init(unsigned long int fixed_part, int monotonic_part);

private:
  unsigned long int fixed_part;
  int monotonic_part;

  std::string representation;
} ;

#endif	/* GCS_COROSYNC_VIEW_IDENTIFIER_INCLUDED */

